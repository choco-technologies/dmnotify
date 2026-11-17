#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "test_listener.h"
#include "dmnotify.h"
#include "dmnotify_ioctl.h"
#include "dmdrvi.h"
#include <string.h>

static test_listener_record_t g_records[TEST_LISTENER_RECORDS];
static unsigned g_count;
static dmod_dmdrvi_ioctl_t g_ioctl;
static dmdrvi_context_t g_node;
static void* g_file;
static int g_result;

static void record(const dmnotify_notification_t* notification, bool cancel)
{
    if (g_count >= TEST_LISTENER_RECORDS)
        return;
    test_listener_record_t* r = &g_records[g_count++];
    memset(r, 0, sizeof(*r));
    r->cancel = cancel;
    Dmod_SnPrintf(r->source, sizeof(r->source), "%s", notification->source);
    Dmod_SnPrintf(r->brief, sizeof(r->brief), "%s", notification->brief);
    r->has_message = notification->message != NULL;
    if (r->has_message)
        Dmod_SnPrintf(r->message, sizeof(r->message), "%s", notification->message);
    r->channel = notification->channel;
    r->privacy = notification->privacy;
}

dmod_dmnotify_dif_api_declaration(1.0, test_listener, void, _on_notify, ( const dmnotify_notification_t* notification ))
{
    record(notification, false);
    if (g_ioctl != NULL)
    {
        dmnotify_notification_t again;
        memset(&again, 0, sizeof(again));
        again.brief = "again";
        again.source = "loop";
        g_result = g_ioctl(g_node, g_file, DMNOTIFY_IOCTL_CMD_NOTIFY, &again);
    }
}

dmod_dmnotify_dif_api_declaration(1.0, test_listener, void, _on_cancel, ( const dmnotify_notification_t* notification ))
{
    record(notification, true);
}

dmod_test_listener_api_declaration(1.0, void, _reset, ( void ))
{
    g_count = 0;
    g_ioctl = NULL;
    g_result = 0;
}

dmod_test_listener_api_declaration(1.0, const test_listener_record_t*, _records, ( unsigned* count ))
{
    *count = g_count;
    return g_records;
}

dmod_test_listener_api_declaration(1.0, void, _reenter, ( void* ioctl, void* node, void* file ))
{
    g_ioctl = (dmod_dmdrvi_ioctl_t)ioctl;
    g_node = node;
    g_file = file;
}

dmod_test_listener_api_declaration(1.0, int, _reenter_result, ( void ))
{
    return g_result;
}

int dmod_init(const Dmod_Config_t *Config)
{
    (void)Config;
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}
