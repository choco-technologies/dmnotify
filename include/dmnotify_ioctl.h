#ifndef DMNOTIFY_IOCTL_H
#define DMNOTIFY_IOCTL_H

#include "dmdrvi_ioctl.h"
#include "dmnotify_types.h"

//===============================================================================================
/**
 * @brief IOCTL for the subscription for notifications
 */
//===============================================================================================
#define DMNOTIFY_IOCTL_CMD_SUBSCRIBE            DMDRVI_IOCTL_CUSTOM_BASE + 0x01

//===============================================================================================
/**
 * @brief IOCTL for unsubscribe
 */
//===============================================================================================
#define DMNOTIFY_IOCTL_CMD_UNSUBSCRIBE          DMDRVI_IOCTL_CUSTOM_BASE + 0x02    

//===============================================================================================
/**
 * @brief IOCTL for the notification
 */
//===============================================================================================
#define DMNOTIFY_IOCTL_CMD_NOTIFY               DMDRVI_IOCTL_CUSTOM_BASE + 0x03

//===============================================================================================
/**
 * @brief IOCTL for canceling of the notification
 */
//===============================================================================================
#define DMNOTIFY_IOCTL_CMD_CANCEL               DMDRVI_IOCTL_CUSTOM_BASE + 0x04

#endif // DMNOTIFY_IOCTL_H