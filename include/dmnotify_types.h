#ifndef DMNOTIFY_TYPES_H
#define DMNOTIFY_TYPES_H

#include <stdint.h>

//===============================================================================================
/**
 * @brief privacy info of the notification
 * 
 * The type is designed for storing privacy information about the notifiaction
 */
//===============================================================================================
typedef enum
{
    dmnotify_privacy_public_brief   = (1<<0),       //!< The brief of the notification is public
    dmnotify_privacy_public_source  = (1<<1),       //!< The source of the notification is public
    dmnotify_privacy_public_message = (1<<2),       //!< The message of the notification is public

    dmnotify_privacy_full_private       = 0,            //!< The notification is completely private, and cannot be displayed on the locked screen at all
    dmnotify_privacy_private_message    = dmnotify_privacy_public_brief | dmnotify_privacy_public_source,
                                                        //!< Only the message content is private - the rest can be displayed

    dmnotify_privacy_full_public        = dmnotify_privacy_public_brief
                                        | dmnotify_privacy_public_source
                                        | dmnotify_privacy_public_message,    //!< The notification is public and can be displayed on the locked screen

    dmnotify_privacy_count                                                    //!< First value that is not a privacy setting
} dmnotify_privacy_t;

//===============================================================================================
/**
 * @brief channel of the notification
 * 
 * The ID of the notification channel. A source (a module) gives one channel to every thing it notifies about - a conversation,
 * a download, an alarm. The source and the channel together are the identity of a notification: a new notification of the same 
 * source on the same channel replaces the previous one, and #DMNOTIFY_IOCTL_CMD_CANCEL removes the one they name.
 */
//===============================================================================================
typedef uint32_t dmnotify_channel_t;

//===============================================================================================
/**
 * @brief stores notification data
 * 
 * The type is for storing of the notification data.
 *
 * The texts belong to the caller: the driver copies them in #DMNOTIFY_IOCTL_CMD_NOTIFY, and a subscriber has to copy what it 
 * wants to keep - the structure given to `_on_notify` and `_on_cancel` is valid only until the function returns.
 * 
 * `brief` and `source` are required, `message` and `icon_path` can be NULL.
 */
//===============================================================================================
typedef struct 
{
    const char*         brief;          //!< Brief string with the notification 
    const char*         message;        //!< Message of the notification to show
    const char*         source;         //!< Source module of the notification
    const char*         icon_path;      //!< Path to the icon of this notification
    dmnotify_privacy_t  privacy;        //!< Privacy setting for this notification
    dmnotify_channel_t  channel;        //!< Channel of the source the notification is on (see #dmnotify_channel_t)
} dmnotify_notification_t;

//===============================================================================================
/**
 * @brief subscription configuration
 *
 * The module named here has to implement `dmod_dmnotify_on_notify` (and can implement `dmod_dmnotify_on_cancel`): the driver 
 * looks the functions up by this name every time it has something to deliver, so a module that was unloaded is simply skipped.
 */
//===============================================================================================
typedef struct 
{
    const char*         module_name;    //!< Name of the module that wants to receive the notifications
} dmnotify_subscribe_t;

#endif // DMNOTIFY_TYPES_H
