#ifndef TEST_LISTENER_H
#define TEST_LISTENER_H

#include <stdbool.h>
#include "dmod_types.h"
#include "test_listener_defs.h"
#include "dmnotify_types.h"

/*
 * The subscriber of the tests: a library that implements the DIF of dmnotify (_on_notify, _on_cancel) and records what it was told,
 * for the test to look at. It is a library because dmod looks for the implementations of a DIF in the enabled modules, and the
 * test itself only runs.
 */
#define TEST_LISTENER_NAME      "test_listener"
#define TEST_LISTENER_RECORDS   8

typedef struct
{
    bool                cancel;         /* told by _on_cancel */
    char                source[24];
    char                brief[32];
    char                message[48];
    bool                has_message;
    dmnotify_channel_t  channel;
    dmnotify_privacy_t  privacy;
} test_listener_record_t;

/* Forget what was recorded; the handlers stop calling the node */
dmod_test_listener_api(1.0, void, _reset, ( void ));

/* What was recorded since the reset, in the order of the calls */
dmod_test_listener_api(1.0, const test_listener_record_t*, _records, ( unsigned* count ));

/* From now on _on_notify notifies through this file of the node itself (what a handler must not do) and keeps the result of it;
 * ioctl is the dmod_dmdrvi_ioctl_t of the driver */
dmod_test_listener_api(1.0, void, _reenter, ( void* ioctl, void* node, void* file ));
dmod_test_listener_api(1.0, int, _reenter_result, ( void ));

#endif
