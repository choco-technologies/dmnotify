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
 * @note
 * The function is called in the thread of the module that notifies (inside its ioctl), with the driver locked: it has to be 
 * short - copy what is needed and return - it must not block, and it must not call the notification node. The structure 
 * and its texts are valid only until the function returns.
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
 * should be removed from the lists. The notification is the one with the same `source` and `channel`. A notification
 * that is replaced by a newer one of the same source and channel is not cancelled first: `_on_notify` with the same 
 * identity replaces it.
 *
 * The function is optional. The rules of `_on_notify` apply.
 *
 * @param notification          Pointer to the structure with the data of the cancelled notification
 */
//===============================================================================================
dmod_dmnotify_dif( 1.0, void, _on_cancel, ( const dmnotify_notification_t* notification ) );

#endif // DMNOTIFY_H
