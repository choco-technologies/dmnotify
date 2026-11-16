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
    dmnotify_privacy_public_message = (1<<2),       //!< The source of the notification is public

    dmnotify_privacy_full_private       = 0,            //!< The notification is completely private, and cannot be displayed on the locked screen at all
    dmnotify_privacy_private_message    = dmnotify_privacy_public_brief | dmnotify_privacy_public_source,
                                                        //!< Only the message content is private - the rest can be displayed

    dmnotify_privacy_full_public        = dmnotify_privacy_public_brief
                                        | dmnotify_privacy_public_source
                                        | dmnotify_privacy_public_message,    //!< The notification is public and can be displayed on the locked screen

    dmnotify_privacy_count
} dmnotify_privacy_t;

//===============================================================================================
/**
 * @brief channel of the notification
 * 
 * The ID of the notification channel
 */
//===============================================================================================
typedef uint32_t dmnotify_channel_t;

//===============================================================================================
/**
 * @brief stores notification data
 * 
 * The type is for storing of the notification data
 */
//===============================================================================================
typedef struct 
{
    const char*         brief;          //!< Brief string with the notification 
    const char*         message;        //!< Message of the notification to show
    const char*         source;         //!< Source module of the notification
    const char*         icon_path;      //!< Path to the icon of this notification
    dmnotify_privacy_t  privacy;        //!< Privacy setting for this notification
} dmnotify_notification_t;

//===============================================================================================
/**
 * @brief subscription configuration
 */
//===============================================================================================
typedef struct 
{
    const char*         module_name;    //!< Name of the module that wants to receive the notifications
} dmnotify_subscribe_t;

#endif // DMNOTIFY_TYPES_H