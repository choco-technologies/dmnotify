#ifndef DMNOTIFY_IOCTL_H
#define DMNOTIFY_IOCTL_H

#include "dmdrvi_ioctl.h"
#include "dmnotify_types.h"

//===============================================================================================
/**
 * @brief IOCTL for the subscription for notifications
 *
 * Argument: `const dmnotify_subscribe_t*`. The node has to be open for reading. The subscription belongs to the open file: 
 * it lasts until #DMNOTIFY_IOCTL_CMD_UNSUBSCRIBE or until the file is closed. The notifications that are active at the moment 
 * are delivered to the new subscriber before the ioctl returns.
 *
 * Errors: -EACCES (not open for reading), -EINVAL (no name), -ENOENT (the module does not implement `_on_notify`), 
 * -EBUSY (the file already has a subscription), -ENOMEM.
 */
//===============================================================================================
#define DMNOTIFY_IOCTL_CMD_SUBSCRIBE            (DMDRVI_IOCTL_CUSTOM_BASE + 0x01)

//===============================================================================================
/**
 * @brief IOCTL for unsubscribe
 *
 * Argument: not used (NULL). Errors: -ENOENT (the file has no subscription).
 */
//===============================================================================================
#define DMNOTIFY_IOCTL_CMD_UNSUBSCRIBE          (DMDRVI_IOCTL_CUSTOM_BASE + 0x02)

//===============================================================================================
/**
 * @brief IOCTL for the notification
 *
 * Argument: `const dmnotify_notification_t*`. The node has to be open for writing. The notification is copied, kept as active 
 * and delivered to every subscriber (`_on_notify`) before the ioctl returns. It replaces the active notification of the same 
 * source and channel. When there are more active notifications than the driver keeps (max_notifications), the oldest is cancelled.
 *
 * Errors: -EACCES (not open for writing), -EINVAL (no source, no brief, unknown privacy bits), -ENOMEM.
 */
//===============================================================================================
#define DMNOTIFY_IOCTL_CMD_NOTIFY               (DMDRVI_IOCTL_CUSTOM_BASE + 0x03)

//===============================================================================================
/**
 * @brief IOCTL for canceling of the notification
 *
 * Argument: `const dmnotify_notification_t*` - only `source` and `channel` are read. The node has to be open for writing. 
 * The notification stops being active and every subscriber is told (`_on_cancel`).
 *
 * Errors: -EACCES (not open for writing), -EINVAL (no source), -ENOENT (no such active notification).
 */
//===============================================================================================
#define DMNOTIFY_IOCTL_CMD_CANCEL               (DMDRVI_IOCTL_CUSTOM_BASE + 0x04)

#endif // DMNOTIFY_IOCTL_H
