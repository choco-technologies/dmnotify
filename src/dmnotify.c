#define DMOD_ENABLE_REGISTRATION ON
#define ENABLE_DIF_REGISTRATIONS ON
#include "dmod.h"
#include "dmnotify.h"
#include "dmdrvi.h"
#include "dmini.h"
#include "dmlist.h"
#include "dmosi.h"
#include "dmnotify_ioctl.h"
#include <errno.h>
#include <string.h>

#define DMNOTIFY_MAGIC              0x6E6F7469
#define DMNOTIFY_HANDLE_MAGIC       0x6E6F7468
#define DMNOTIFY_DEFAULT_MAX        16          //!< Active notifications kept when the configuration does not say (max_notifications)
#define DMNOTIFY_WRITE_SOURCE       "console"   //!< Source of the notifications written to the node as text
#define DMNOTIFY_WRITE_MAX          256         //!< Longest text taken from one write()

//===============================================================================================
/**
 * @brief stores context of the driver
 */
//===============================================================================================
struct dmdrvi_context
{
    uint32_t            magic;          //!< Magic number to check if the context is valid
    dmosi_mutex_t       lock;           //!< Guards the lists (recursive: a subscriber is called with it taken)
    dmlist_context_t*   subscriptions;  //!< List of the open files that subscribe (dmnotify_handle_t*), with the names of their modules
    dmlist_context_t*   notifications;  //!< List of the active notifications (dmnotify_notification_t*, own copies), oldest first
    size_t              max;            //!< Most active notifications kept
    bool                dispatching;    //!< The subscribers are being called: the node cannot be used by them
    char*               report;         //!< Text of read(), made when it starts at offset 0
};

//===============================================================================================
/**
 * @brief an open file of the node
 */
//===============================================================================================
typedef struct
{
    uint32_t            magic;          //!< Magic number to check if the handle is valid
    int                 flags;          //!< DMDRVI_O_* the file was opened with
    char*               subscriber;     //!< Name of the module subscribed through this file, NULL when none
} dmnotify_handle_t;

//===============================================================================================
/**
 * @brief what is delivered to the subscribers
 */
//===============================================================================================
typedef struct
{
    const dmnotify_notification_t*  notification;   //!< The notification to deliver
    bool                            cancel;         //!< true: it was cancelled (_on_cancel), false: it is new (_on_notify)
} dmnotify_delivery_t;

static bool is_valid_context( dmdrvi_context_t context )
{
    return context != NULL && context->magic == DMNOTIFY_MAGIC;
}

static bool is_valid_handle( const dmnotify_handle_t* handle )
{
    return handle != NULL && handle->magic == DMNOTIFY_HANDLE_MAGIC;
}

static bool can_read( const dmnotify_handle_t* handle )
{
    return ( handle->flags & ( DMDRVI_O_RDONLY | DMDRVI_O_RDWR ) ) != 0;
}

static bool can_write( const dmnotify_handle_t* handle )
{
    return ( handle->flags & ( DMDRVI_O_WRONLY | DMDRVI_O_RDWR ) ) != 0;
}

static void notification_free( dmnotify_notification_t* notification )
{
    if( notification == NULL )
    {
        return;
    }
    Dmod_Free( (void*)notification->brief );
    Dmod_Free( (void*)notification->message );
    Dmod_Free( (void*)notification->source );
    Dmod_Free( (void*)notification->icon_path );
    Dmod_Free( notification );
}

/**
 * @brief Make the driver's own copy of a notification (NULL when out of memory)
 */
static dmnotify_notification_t* notification_copy( const dmnotify_notification_t* from )
{
    dmnotify_notification_t* copy = Dmod_Malloc( sizeof(*copy) );
    if( copy == NULL )
    {
        return NULL;
    }
    memset( copy, 0, sizeof(*copy) );
    copy->privacy   = from->privacy;
    copy->channel   = from->channel;
    copy->brief     = Dmod_StrDup( from->brief );
    copy->source    = Dmod_StrDup( from->source );
    copy->message   = from->message   != NULL ? Dmod_StrDup( from->message )   : NULL;
    copy->icon_path = from->icon_path != NULL ? Dmod_StrDup( from->icon_path ) : NULL;
    if( copy->brief == NULL || copy->source == NULL
     || ( from->message   != NULL && copy->message   == NULL )
     || ( from->icon_path != NULL && copy->icon_path == NULL ) )
    {
        notification_free( copy );
        return NULL;
    }
    return copy;
}

/**
 * @brief Find the module that subscribed under a name; NULL when it is not loaded (anymore) or does not take notifications
 */
static Dmod_Context_t* find_subscriber( const char* module_name )
{
    Dmod_Context_t* module = NULL;
    while( ( module = Dmod_GetNextDifModule( dmod_dmnotify_on_notify_sig, module ) ) != NULL )
    {
        const char* name = Dmod_GetName( module );
        if( name != NULL && strcmp( name, module_name ) == 0 )
        {
            return module;
        }
    }
    return NULL;
}

/**
 * @brief Call the function of one subscriber for a delivery
 */
static void deliver_to( const char* module_name, const dmnotify_delivery_t* delivery )
{
    Dmod_Context_t* module = find_subscriber( module_name );
    if( module == NULL )
    {
        return;     // the module was unloaded without closing the node: nothing to call
    }
    if( delivery->cancel )
    {
        dmod_dmnotify_on_cancel_t on_cancel = Dmod_GetDifFunction( module, dmod_dmnotify_on_cancel_sig );
        if( on_cancel != NULL )
        {
            on_cancel( delivery->notification );
        }
    }
    else
    {
        dmod_dmnotify_on_notify_t on_notify = Dmod_GetDifFunction( module, dmod_dmnotify_on_notify_sig );
        if( on_notify != NULL )
        {
            on_notify( delivery->notification );
        }
    }
}

static bool deliver_iterator( void* data, void* user_data )
{
    const dmnotify_handle_t* handle = data;
    if( handle->subscriber != NULL )
    {
        deliver_to( handle->subscriber, user_data );
    }
    return true;
}

/**
 * @brief Tell every subscriber about a notification (the lock is taken)
 */
static void deliver( dmdrvi_context_t context, const dmnotify_notification_t* notification, bool cancel )
{
    dmnotify_delivery_t delivery;
    delivery.notification = notification;
    delivery.cancel       = cancel;
    context->dispatching = true;
    dmlist_foreach( context->subscriptions, deliver_iterator, &delivery );
    context->dispatching = false;
}

/**
 * @brief Position of the active notification of a source on a channel, or the size of the list when there is none
 */
static size_t find_notification( dmdrvi_context_t context, const char* source, dmnotify_channel_t channel )
{
    size_t count = dmlist_size( context->notifications );
    for( size_t i = 0; i < count; i++ )
    {
        const dmnotify_notification_t* notification = dmlist_get( context->notifications, i );
        if( notification != NULL && notification->channel == channel && strcmp( notification->source, source ) == 0 )
        {
            return i;
        }
    }
    return count;
}

static int handle_notify( dmdrvi_context_t context, const dmnotify_notification_t* notification )
{
    if( notification == NULL || notification->source == NULL || notification->source[0] == '\0'
     || notification->brief == NULL || ( (uint32_t)notification->privacy & ~(uint32_t)dmnotify_privacy_full_public ) != 0 )
    {
        return -EINVAL;
    }
    dmnotify_notification_t* copy = notification_copy( notification );
    if( copy == NULL )
    {
        return -ENOMEM;
    }
    if( !dmlist_push_back( context->notifications, copy ) )
    {
        notification_free( copy );
        return -ENOMEM;
    }
    // the one it replaces goes without a cancel: the new one has its identity
    size_t previous = find_notification( context, copy->source, copy->channel );
    if( previous + 1 < dmlist_size( context->notifications ) )
    {
        notification_free( dmlist_remove_at( context->notifications, previous ) );
    }
    while( dmlist_size( context->notifications ) > context->max )
    {
        dmnotify_notification_t* oldest = dmlist_pop_front( context->notifications );
        deliver( context, oldest, true );
        notification_free( oldest );
    }
    deliver( context, copy, false );
    return 0;
}

static int handle_cancel( dmdrvi_context_t context, const dmnotify_notification_t* notification )
{
    if( notification == NULL || notification->source == NULL )
    {
        return -EINVAL;
    }
    size_t position = find_notification( context, notification->source, notification->channel );
    if( position >= dmlist_size( context->notifications ) )
    {
        return -ENOENT;
    }
    dmnotify_notification_t* cancelled = dmlist_remove_at( context->notifications, position );
    deliver( context, cancelled, true );
    notification_free( cancelled );
    return 0;
}

static int handle_subscribe( dmdrvi_context_t context, dmnotify_handle_t* handle, const dmnotify_subscribe_t* subscribe )
{
    if( subscribe == NULL || subscribe->module_name == NULL || subscribe->module_name[0] == '\0' )
    {
        return -EINVAL;
    }
    if( handle->subscriber != NULL )
    {
        return -EBUSY;
    }
    if( find_subscriber( subscribe->module_name ) == NULL )
    {
        return -ENOENT;
    }
    char* name = Dmod_StrDup( subscribe->module_name );
    if( name == NULL )
    {
        return -ENOMEM;
    }
    if( !dmlist_push_back( context->subscriptions, handle ) )
    {
        Dmod_Free( name );
        return -ENOMEM;
    }
    handle->subscriber = name;
    // what is active now, oldest first - only to the new subscriber
    size_t count = dmlist_size( context->notifications );
    context->dispatching = true;
    for( size_t i = 0; i < count; i++ )
    {
        dmnotify_delivery_t delivery;
        delivery.notification = dmlist_get( context->notifications, i );
        delivery.cancel       = false;
        deliver_to( name, &delivery );
    }
    context->dispatching = false;
    return 0;
}

static int same_pointer( const void* data1, const void* data2 )
{
    return data1 == data2 ? 0 : 1;
}

static int handle_unsubscribe( dmdrvi_context_t context, dmnotify_handle_t* handle )
{
    if( handle->subscriber == NULL )
    {
        return -ENOENT;
    }
    dmlist_remove( context->subscriptions, handle, same_pointer );
    Dmod_Free( handle->subscriber );
    handle->subscriber = NULL;
    return 0;
}

/**
 * @brief Make the text of read(): a line "source [channel]: brief - message" for every active notification, newest first
 */
static int make_report( dmdrvi_context_t context )
{
    size_t count = dmlist_size( context->notifications );
    size_t size = 1;
    for( size_t i = 0; i < count; i++ )
    {
        const dmnotify_notification_t* n = dmlist_get( context->notifications, i );
        size += strlen( n->source ) + strlen( n->brief ) + ( n->message != NULL ? strlen( n->message ) : 0 ) + 24;
    }
    char* report = Dmod_Malloc( size );
    if( report == NULL )
    {
        return -ENOMEM;
    }
    report[0] = '\0';
    size_t at = 0;
    for( size_t i = count; i > 0; i-- )
    {
        const dmnotify_notification_t* n = dmlist_get( context->notifications, i - 1 );
        bool with_message = n->message != NULL && n->message[0] != '\0';
        Dmod_SnPrintf( report + at, size - at, "%s [%u]: %s%s%s\n", n->source, (unsigned)n->channel, n->brief,
                       with_message ? " - " : "", with_message ? n->message : "" );
        at += strlen( report + at );
    }
    Dmod_Free( context->report );
    context->report = report;
    return 0;
}

/**
 * @brief Create a DMDRVI context
 *
 * The node is named after the section of the configuration ([notify]: /dev/notify - the name the clients look for);
 * without a section name it is /dev/dmnotify.
 *
 * Configuration: max_notifications - how many active notifications are kept (default 16); when one more comes, the oldest
 * is cancelled.
 *
 * @param config Pointer to dmini_context object with configuration parameters (dmini module required to parse them)
 * @param dev_num Output pointer to device number structure - driver fills in major, minor, and flags (must not be NULL)
 * 
 * @return dmdrvi_context_t Created DMDRVI context
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, dmdrvi_context_t, _create, ( dmini_context_t config, dmdrvi_dev_num_t* dev_num ))
{
    if( dev_num == NULL )
    {
        return NULL;
    }
    dmdrvi_context_t context = Dmod_Malloc( sizeof(struct dmdrvi_context) );
    if( context == NULL )
    {
        return NULL;
    }
    memset( context, 0, sizeof(*context) );
    int max = config != NULL ? dmini_get_int( config, NULL, "max_notifications", DMNOTIFY_DEFAULT_MAX ) : DMNOTIFY_DEFAULT_MAX;
    context->max            = max > 0 ? (size_t)max : DMNOTIFY_DEFAULT_MAX;
    context->lock           = dmosi_mutex_create( true );
    context->subscriptions  = dmlist_create();
    context->notifications  = dmlist_create();
    if( context->lock == NULL || context->subscriptions == NULL || context->notifications == NULL )
    {
        DMOD_LOG_ERROR( "dmnotify: out of memory for the driver\n" );
        if( context->subscriptions != NULL ) dmlist_destroy( context->subscriptions );
        if( context->notifications != NULL ) dmlist_destroy( context->notifications );
        if( context->lock != NULL ) dmosi_mutex_destroy( context->lock );
        Dmod_Free( context );
        return NULL;
    }
    context->magic = DMNOTIFY_MAGIC;
    memset( dev_num, 0, sizeof(*dev_num) );
    dev_num->flags = DMDRVI_NUM_NONE | DMDRVI_NUM_ALT_NAME;
    return context;
}

/**
 * @brief Free a DMDRVI context
 *
 * @param context DMDRVI context to free
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, void, _free, ( dmdrvi_context_t context ))
{
    if( !is_valid_context( context ) )
    {
        return;
    }
    context->magic = 0;
    dmnotify_notification_t* notification;
    while( ( notification = dmlist_pop_front( context->notifications ) ) != NULL )
    {
        notification_free( notification );
    }
    dmlist_destroy( context->notifications );
    dmlist_destroy( context->subscriptions );       // (the handles belong to the open files)
    dmosi_mutex_destroy( context->lock );
    Dmod_Free( context->report );
    Dmod_Free( context );
}

/**
 * @brief Open a device
 *
 * The flags decide what the file can do: reading (and subscribing) needs DMDRVI_O_RDONLY or DMDRVI_O_RDWR, notifying
 * and cancelling needs DMDRVI_O_WRONLY or DMDRVI_O_RDWR - so the file system's access rights of the node decide who can 
 * see the notifications and who can only send them.
 *
 * @param context DMDRVI context
 * @param flags Open flags  (DMDRVI_O_RDONLY, DMDRVI_O_WRONLY, DMDRVI_O_RDWR)
 * @param dev_num Device number identifying which device within the context to open
 *
 * @return void* Device handle
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, void*, _open, ( dmdrvi_context_t context, int flags, const dmdrvi_dev_num_t* dev_num ))
{
    (void)dev_num;
    if( !is_valid_context( context ) || ( flags & ( DMDRVI_O_RDONLY | DMDRVI_O_WRONLY | DMDRVI_O_RDWR ) ) == 0 )
    {
        return NULL;
    }
    dmnotify_handle_t* handle = Dmod_Malloc( sizeof(*handle) );
    if( handle == NULL )
    {
        return NULL;
    }
    memset( handle, 0, sizeof(*handle) );
    handle->magic = DMNOTIFY_HANDLE_MAGIC;
    handle->flags = flags;
    return handle;
}

/**
 * @brief Close a device
 *
 * The subscription made through the file ends with it.
 *
 * @param context DMDRVI context
 * @param handle Device handle
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, void, _close, ( dmdrvi_context_t context, void* handle ))
{
    dmnotify_handle_t* file = handle;
    if( !is_valid_context( context ) || !is_valid_handle( file ) )
    {
        return;
    }
    dmosi_mutex_lock( context->lock );
    (void)handle_unsubscribe( context, file );
    dmosi_mutex_unlock( context->lock );
    file->magic = 0;
    Dmod_Free( file );
}

/**
 * @brief Read from a device
 *
 * Gives the active notifications as text, newest first, a line each: "source [channel]: brief - message".
 *
 * @param context DMDRVI context
 * @param handle Device handle
 * @param buffer Buffer to read data into
 * @param size Number of bytes to read; values greater than INT64_MAX must fail
 * with -EOVERFLOW because they cannot be represented by dmdrvi_ssize_t
 * @param offset Non-negative byte offset from the beginning of the device
 * 
 * @return Number of bytes read, zero at end of device, or a negative
 * errno-compatible error. A zero-length request returns zero.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, dmdrvi_ssize_t, _read, ( dmdrvi_context_t context, void* handle, void* buffer, size_t size, dmdrvi_offset_t offset ))
{
    const dmnotify_handle_t* file = handle;
    if( !is_valid_context( context ) || !is_valid_handle( file ) || ( buffer == NULL && size != 0 ) || offset < 0 )
    {
        return -EINVAL;
    }
    if( !can_read( file ) )
    {
        return -EACCES;
    }
    if( size > (size_t)INT64_MAX )
    {
        return -EOVERFLOW;
    }
    dmosi_mutex_lock( context->lock );
    dmdrvi_ssize_t result = 0;
    if( ( offset == 0 || context->report == NULL ) && make_report( context ) != 0 )
    {
        result = -ENOMEM;
    }
    else
    {
        size_t length = strlen( context->report );
        if( (size_t)offset < length && size > 0 )
        {
            size_t count = length - (size_t)offset < size ? length - (size_t)offset : size;
            memcpy( buffer, context->report + offset, count );
            result = (dmdrvi_ssize_t)count;
        }
    }
    dmosi_mutex_unlock( context->lock );
    return result;
}

/**
 * @brief Write to a device
 *
 * A text written to the node is a notification of the source "console" on channel 0, public (echo "Tea is ready" > /dev/notify),
 * so a shell and a script can notify too. The end of the line is dropped, and a write of nothing but an end of line is ignored
 * (a shell writes the text and its \"\\n\" separately). Taking it back is the job of the ioctl.
 *
 * @param context DMDRVI context
 * @param handle Device handle
 * @param buffer Buffer with data to write
 * @param size Number of bytes to write; values greater than INT64_MAX must fail
 * with -EOVERFLOW because they cannot be represented by dmdrvi_ssize_t
 * @param offset Non-negative byte offset from the beginning of the device
 * 
 * @return Number of bytes written or a negative errno-compatible error. A
 * zero-length request returns zero.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, dmdrvi_ssize_t, _write, ( dmdrvi_context_t context, void* handle, const void* buffer, size_t size, dmdrvi_offset_t offset ))
{
    const dmnotify_handle_t* file = handle;
    (void)offset;
    if( !is_valid_context( context ) || !is_valid_handle( file ) || ( buffer == NULL && size != 0 ) )
    {
        return -EINVAL;
    }
    if( !can_write( file ) )
    {
        return -EACCES;
    }
    if( size > (size_t)INT64_MAX )
    {
        return -EOVERFLOW;
    }
    if( size == 0 )
    {
        return 0;
    }
    size_t length = size < DMNOTIFY_WRITE_MAX ? size : DMNOTIFY_WRITE_MAX;
    char* text = Dmod_Malloc( length + 1 );
    if( text == NULL )
    {
        return -ENOMEM;
    }
    memcpy( text, buffer, length );
    while( length > 0 && ( text[length - 1] == '\n' || text[length - 1] == '\r' ) )
    {
        length--;
    }
    text[length] = '\0';
    dmnotify_notification_t notification;
    memset( &notification, 0, sizeof(notification) );
    notification.brief   = text;
    notification.source  = DMNOTIFY_WRITE_SOURCE;
    notification.privacy = dmnotify_privacy_full_public;
    dmosi_mutex_lock( context->lock );
    int result = 0;
    if( length > 0 )    // (a lone end of line is nothing: a shell writes the text and its "\n" separately)
    {
        result = -EDEADLK;
        if( !context->dispatching )
        {
            result = handle_notify( context, &notification );
        }
    }
    dmosi_mutex_unlock( context->lock );
    Dmod_Free( text );
    return result == 0 ? (dmdrvi_ssize_t)size : result;
}

/**
 * @brief Ioctl operation on a device
 *
 * The commands are in dmnotify_ioctl.h. A subscriber that calls the node from `_on_notify` or `_on_cancel` gets -EDEADLK.
 *
 * @param context DMDRVI context
 * @param handle Device handle
 * @param command Ioctl command
 * @param arg Argument for the ioctl command
 * 
 * @return int Result of the ioctl operation (errno)
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, int, _ioctl, ( dmdrvi_context_t context, void* handle, int command, void* arg ))
{
    dmnotify_handle_t* file = handle;
    if( !is_valid_context( context ) || !is_valid_handle( file ) )
    {
        return -EINVAL;
    }
    bool reading = command == DMNOTIFY_IOCTL_CMD_SUBSCRIBE || command == DMNOTIFY_IOCTL_CMD_UNSUBSCRIBE;
    bool writing = command == DMNOTIFY_IOCTL_CMD_NOTIFY || command == DMNOTIFY_IOCTL_CMD_CANCEL;
    if( !reading && !writing )
    {
        return -ENOTTY;
    }
    if( ( reading && !can_read( file ) ) || ( writing && !can_write( file ) ) )
    {
        return -EACCES;
    }
    dmosi_mutex_lock( context->lock );
    int result = -EDEADLK;
    if( !context->dispatching )
    {
        switch( command )
        {
            case DMNOTIFY_IOCTL_CMD_SUBSCRIBE:      result = handle_subscribe( context, file, arg );    break;
            case DMNOTIFY_IOCTL_CMD_UNSUBSCRIBE:    result = handle_unsubscribe( context, file );       break;
            case DMNOTIFY_IOCTL_CMD_NOTIFY:         result = handle_notify( context, arg );             break;
            default:                                result = handle_cancel( context, arg );             break;
        }
    }
    dmosi_mutex_unlock( context->lock );
    return result;
}

/**
 * @brief Flush device buffers
 *
 * @param context DMDRVI context
 * @param handle Device handle
 * 
 * @return int Result of the flush operation (errno)
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, int, _flush, ( dmdrvi_context_t context, void* handle ))
{
    (void)handle;
    return is_valid_context( context ) ? 0 : -EINVAL;
}

/**
 * @brief Get device status
 *
 * Gets status information for the specified device path without requiring
 * the device to be opened first (similar to POSIX stat() which works with
 * a path without requiring fopen()).
 *
 * @param context DMDRVI context
 * @param path Device path (e.g., "/dev/notify")
 * @param stat Pointer to dmdrvi_stat_t structure to fill with status information
 * 
 * @return int Result of the stat operation (errno)
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmnotify, int, _stat, ( dmdrvi_context_t context, const char* path, dmdrvi_stat_t* stat ))
{
    (void)path;
    if( !is_valid_context( context ) || stat == NULL )
    {
        return -EINVAL;
    }
    stat->size = 0;
    stat->mode = 0666;
    return 0;
}


/**
 * @brief Initialization function for the module.
 *
 * This function is called when the module is enabled.
 * Please use this function to initialize the module, for instance:
 * - initialize the module variables
 * - initialize the module hardware
 * - allocate memory
 */
int dmod_init(const Dmod_Config_t *Config)
{
    return 0;
}

/**
 * @brief De-initialization function for the module.
 *
 * This function is called when the module is disabled.
 * Please use this function to de-initialize the module, for instance:
 * - free memory
 * - de-initialize the module hardware
 * - de-initialize the module variables
 */
int dmod_deinit(void)
{
    return 0;
}
