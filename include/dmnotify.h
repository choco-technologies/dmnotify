#ifndef DMNOTIFY_H
#define DMNOTIFY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dmod_types.h"
#include "dmnotify_defs.h"
#include "dmnotify_types.h"

//===============================================================================================
/**
 * @brief called when the new notification is available
 * 
 * The function is called by the dmnotify when the new notification is available. 
 * 
 * The module has to implement this DIF and subscribe by using #DMNOTIFY_IOCTL_CMD_SUBSCRIBE ioctl.
 * 
 * @note 
 * The notifications requires subscriptions on purpose - to protect against unauthorized access to the 
 * notifications and protect the privacy. Thanks to this mechanism the file system controls the
 * registration process. 
 * 
 * @param notification          Pointer to the structure with notification's data
 */
//===============================================================================================
dmod_dmnotify_dif( 1.0, void, _on_notify, ( const dmnotify_notification_t* notification ) );

//===============================================================================================
/**
 * @brief called when the notification has expired and needs to be removed
 * 
 * This function is called by the dmnotify whenever a notification is not valid anymore and 
 * should be removed from the lists.
 */
//===============================================================================================
dmod_dmnotify_dif( 1.0, void, _on_cancel, ( const dmnotify_notification_t* notification ) );

#endif // DMNOTIFY_H
