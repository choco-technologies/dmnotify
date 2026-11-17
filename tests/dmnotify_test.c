#define DMOD_ENABLE_REGISTRATION ON
#define ENABLE_DIF_REGISTRATIONS ON
#include "dmod_test.h"
#include "dmnotify.h"
#include "dmnotify_ioctl.h"
#include "test_listener.h"
#include "dmdrvi.h"
#include "dmini.h"
#include <errno.h>
#include <string.h>

/*
 * dmnotify has no API of its own: dmdevfs makes a node of it from an .ini file. These steps do what dmdevfs does - build the
 * .ini, call the dmdrvi functions of the module. The subscriber is the library test_listener (test_listener.c), which records
 * what it was told.
 */
#define SUBSCRIBER      TEST_LISTENER_NAME

#define DMOD_TEST_EXPECT_STR_EQ(a, b)   DMOD_TEST_EXPECT(strcmp((a), (b)) == 0)

static const test_listener_record_t* g_records;    /* what the listener was told (refreshed by told()) */

static dmdrvi_context_t g_node;
static dmdrvi_dev_num_t g_number;
static dmod_dmdrvi_create_t g_create;
static dmod_dmdrvi_free_t g_free;
static dmod_dmdrvi_open_t g_open;
static dmod_dmdrvi_close_t g_close;
static dmod_dmdrvi_read_t g_read;
static dmod_dmdrvi_write_t g_write;
static dmod_dmdrvi_ioctl_t g_ioctl;
static dmod_dmdrvi_stat_t g_stat;

/* How many times the listener was called */
static unsigned told(void)
{
    unsigned count = 0;
    g_records = test_listener_records(&count);
    return count;
}

static bool load_functions(void)
{
    if (g_create != NULL)
        return true;
    if (Dmod_LoadModuleByName("dmnotify") == NULL || !Dmod_EnableModule("dmnotify", false, NULL))
        return false;
    Dmod_Context_t* m = Dmod_GetModuleContext("dmnotify");
    g_create = Dmod_GetDifFunction(m, dmod_dmdrvi_create_sig);
    g_free = Dmod_GetDifFunction(m, dmod_dmdrvi_free_sig);
    g_open = Dmod_GetDifFunction(m, dmod_dmdrvi_open_sig);
    g_close = Dmod_GetDifFunction(m, dmod_dmdrvi_close_sig);
    g_read = Dmod_GetDifFunction(m, dmod_dmdrvi_read_sig);
    g_write = Dmod_GetDifFunction(m, dmod_dmdrvi_write_sig);
    g_ioctl = Dmod_GetDifFunction(m, dmod_dmdrvi_ioctl_sig);
    g_stat = Dmod_GetDifFunction(m, dmod_dmdrvi_stat_sig);
    return g_create && g_free && g_open && g_close && g_read && g_write && g_ioctl && g_stat;
}

/* The node of an .ini with max_notifications (0: not given) */
static dmdrvi_context_t make_node(int max)
{
    dmini_context_t ini = dmini_create();
    dmini_set_string(ini, NULL, "driver_name", "dmnotify");         /* (dmdevfs gives the driver its section as the global one) */
    if (max > 0)
        dmini_set_int(ini, NULL, "max_notifications", max);
    dmdrvi_context_t node = g_create(ini, &g_number);
    dmini_destroy(ini);
    return node;
}

/* (not inlined: with the constant texts of a step in sight, the x86_64 compiler builds the structure from a pair of pointers kept in
 * relocated data, and a module has none) */
#define NOINLINE    __attribute__((noinline))

static NOINLINE int notify(void* file, const char* source, dmnotify_channel_t channel, const char* brief, const char* message)
{
    dmnotify_notification_t n;
    memset(&n, 0, sizeof(n));
    n.brief = brief;
    n.message = message;
    n.source = source;
    n.privacy = dmnotify_privacy_private_message;
    n.channel = channel;
    return g_ioctl(g_node, file, DMNOTIFY_IOCTL_CMD_NOTIFY, &n);
}

static NOINLINE int cancel(void* file, const char* source, dmnotify_channel_t channel)
{
    dmnotify_notification_t n;
    memset(&n, 0, sizeof(n));
    n.source = source;
    n.channel = channel;
    return g_ioctl(g_node, file, DMNOTIFY_IOCTL_CMD_CANCEL, &n);
}

static NOINLINE int subscribe(void* file, const char* module)
{
    dmnotify_subscribe_t s;
    s.module_name = module;
    return g_ioctl(g_node, file, DMNOTIFY_IOCTL_CMD_SUBSCRIBE, &s);
}

static void read_all(void* file, char* text, size_t size)
{
    size_t at = 0;
    dmdrvi_ssize_t n;
    while (at + 1 < size && (n = g_read(g_node, file, text + at, 7 < size - 1 - at ? 7 : size - 1 - at, (dmdrvi_offset_t)at)) > 0)
        at += (size_t)n;                                            /* (in pieces: the offset has to work) */
    text[at] = '\0';
}

void dmod_test_setup(void)
{
    test_listener_reset();
    g_node = load_functions() ? make_node(0) : NULL;
}

void dmod_test_teardown(void)
{
    if (g_node != NULL)
        g_free(g_node);
    g_node = NULL;
}

DMOD_TEST_STEP(the_node_is_created_and_named_by_its_section)
{
    DMOD_TEST_EXPECT_NOT_NULL(g_node);
    DMOD_TEST_EXPECT_TRUE((g_number.flags & DMDRVI_NUM_ALT_NAME) != 0);
    DMOD_TEST_EXPECT_TRUE((g_number.flags & (DMDRVI_NUM_MAJOR | DMDRVI_NUM_MINOR)) == 0);
    DMOD_TEST_EXPECT_NULL(g_create(NULL, NULL));
    dmdrvi_stat_t st;
    DMOD_TEST_EXPECT_EQ(g_stat(g_node, "/dev/notify", &st), 0);
    DMOD_TEST_EXPECT_EQ(g_stat(g_node, "/dev/notify", NULL), -EINVAL);
    dmdrvi_context_t without_config = g_create(NULL, &g_number);    /* the defaults */
    DMOD_TEST_EXPECT_NOT_NULL(without_config);
    g_free(without_config);
}

DMOD_TEST_STEP(a_subscriber_is_told_about_a_notification)
{
    void* reader = g_open(g_node, DMDRVI_O_RDONLY, &g_number);
    void* writer = g_open(g_node, DMDRVI_O_WRONLY, &g_number);
    DMOD_TEST_EXPECT_NOT_NULL(reader);
    DMOD_TEST_EXPECT_NOT_NULL(writer);
    DMOD_TEST_EXPECT_EQ(subscribe(reader, SUBSCRIBER), 0);
    DMOD_TEST_EXPECT_EQ(notify(writer, "meshcore", 7, "#test", "Marek: hello"), 0);
    DMOD_TEST_EXPECT_EQ(told(), 1u);
    DMOD_TEST_EXPECT_FALSE(g_records[0].cancel);
    DMOD_TEST_EXPECT_STR_EQ(g_records[0].source, "meshcore");
    DMOD_TEST_EXPECT_STR_EQ(g_records[0].brief, "#test");
    DMOD_TEST_EXPECT_STR_EQ(g_records[0].message, "Marek: hello");
    DMOD_TEST_EXPECT_EQ(g_records[0].channel, 7u);
    DMOD_TEST_EXPECT_EQ(g_records[0].privacy, dmnotify_privacy_private_message);
    DMOD_TEST_EXPECT_EQ(notify(writer, "meshcore", 8, "no message", NULL), 0);
    DMOD_TEST_EXPECT_FALSE(g_records[1].has_message);
    g_close(g_node, writer);
    g_close(g_node, reader);
}

DMOD_TEST_STEP(a_cancel_is_delivered_and_only_once)
{
    void* file = g_open(g_node, DMDRVI_O_RDWR, &g_number);
    DMOD_TEST_EXPECT_EQ(subscribe(file, SUBSCRIBER), 0);
    DMOD_TEST_EXPECT_EQ(notify(file, "meshcore", 1, "one", NULL), 0);
    DMOD_TEST_EXPECT_EQ(notify(file, "sms", 1, "other source, same channel", NULL), 0);
    DMOD_TEST_EXPECT_EQ(cancel(file, "meshcore", 1), 0);
    DMOD_TEST_EXPECT_EQ(told(), 3u);
    DMOD_TEST_EXPECT_TRUE(g_records[2].cancel);
    DMOD_TEST_EXPECT_STR_EQ(g_records[2].source, "meshcore");
    DMOD_TEST_EXPECT_STR_EQ(g_records[2].brief, "one");
    DMOD_TEST_EXPECT_EQ(cancel(file, "meshcore", 1), -ENOENT);
    DMOD_TEST_EXPECT_EQ(cancel(file, "meshcore", 2), -ENOENT);
    DMOD_TEST_EXPECT_EQ(told(), 3u);
    g_close(g_node, file);
}

DMOD_TEST_STEP(the_same_source_and_channel_replaces_without_a_cancel)
{
    void* file = g_open(g_node, DMDRVI_O_RDWR, &g_number);
    DMOD_TEST_EXPECT_EQ(notify(file, "meshcore", 1, "1 message", NULL), 0);
    DMOD_TEST_EXPECT_EQ(notify(file, "meshcore", 2, "other", NULL), 0);
    DMOD_TEST_EXPECT_EQ(notify(file, "meshcore", 1, "2 messages", NULL), 0);
    char text[160];
    read_all(file, text, sizeof(text));
    DMOD_TEST_EXPECT_STR_EQ(text, "meshcore [1]: 2 messages\nmeshcore [2]: other\n");     /* newest first */
    DMOD_TEST_EXPECT_EQ(subscribe(file, SUBSCRIBER), 0);
    DMOD_TEST_EXPECT_EQ(told(), 2u);
    g_close(g_node, file);
}

DMOD_TEST_STEP(a_new_subscriber_gets_what_is_active_oldest_first)
{
    void* writer = g_open(g_node, DMDRVI_O_WRONLY, &g_number);
    DMOD_TEST_EXPECT_EQ(notify(writer, "a", 0, "first", NULL), 0);
    DMOD_TEST_EXPECT_EQ(notify(writer, "b", 0, "second", NULL), 0);
    DMOD_TEST_EXPECT_EQ(notify(writer, "c", 0, "gone", NULL), 0);
    DMOD_TEST_EXPECT_EQ(cancel(writer, "c", 0), 0);
    DMOD_TEST_EXPECT_EQ(told(), 0u);
    void* reader = g_open(g_node, DMDRVI_O_RDONLY, &g_number);
    DMOD_TEST_EXPECT_EQ(subscribe(reader, SUBSCRIBER), 0);
    DMOD_TEST_EXPECT_EQ(told(), 2u);
    DMOD_TEST_EXPECT_STR_EQ(g_records[0].brief, "first");
    DMOD_TEST_EXPECT_STR_EQ(g_records[1].brief, "second");
    g_close(g_node, reader);
    g_close(g_node, writer);
}

DMOD_TEST_STEP(the_oldest_is_cancelled_when_there_are_too_many)
{
    g_free(g_node);
    g_node = make_node(2);
    void* file = g_open(g_node, DMDRVI_O_RDWR, &g_number);
    DMOD_TEST_EXPECT_EQ(subscribe(file, SUBSCRIBER), 0);
    DMOD_TEST_EXPECT_EQ(notify(file, "a", 1, "one", NULL), 0);
    DMOD_TEST_EXPECT_EQ(notify(file, "a", 2, "two", NULL), 0);
    DMOD_TEST_EXPECT_EQ(notify(file, "a", 3, "three", NULL), 0);
    DMOD_TEST_EXPECT_EQ(told(), 4u);
    DMOD_TEST_EXPECT_TRUE(g_records[2].cancel);
    DMOD_TEST_EXPECT_STR_EQ(g_records[2].brief, "one");
    DMOD_TEST_EXPECT_FALSE(g_records[3].cancel);
    DMOD_TEST_EXPECT_STR_EQ(g_records[3].brief, "three");
    char text[96];
    read_all(file, text, sizeof(text));
    DMOD_TEST_EXPECT_STR_EQ(text, "a [3]: three\na [2]: two\n");
    g_close(g_node, file);
}

DMOD_TEST_STEP(the_open_flags_decide_what_a_file_can_do)
{
    void* reader = g_open(g_node, DMDRVI_O_RDONLY, &g_number);
    void* writer = g_open(g_node, DMDRVI_O_WRONLY, &g_number);
    char byte;
    DMOD_TEST_EXPECT_NULL(g_open(g_node, 0, &g_number));
    DMOD_TEST_EXPECT_EQ(notify(reader, "spy", 0, "not allowed", NULL), -EACCES);
    DMOD_TEST_EXPECT_EQ(cancel(reader, "spy", 0), -EACCES);
    DMOD_TEST_EXPECT_EQ(g_write(g_node, reader, "x", 1, 0), -EACCES);
    DMOD_TEST_EXPECT_EQ(subscribe(writer, SUBSCRIBER), -EACCES);
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, writer, DMNOTIFY_IOCTL_CMD_UNSUBSCRIBE, NULL), -EACCES);
    DMOD_TEST_EXPECT_EQ(g_read(g_node, writer, &byte, 1, 0), -EACCES);
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, reader, DMDRVI_IOCTL_CUSTOM_BASE + 0x50, NULL), -ENOTTY);
    g_close(g_node, writer);
    g_close(g_node, reader);
}

DMOD_TEST_STEP(a_subscription_needs_a_module_that_takes_notifications)
{
    void* reader = g_open(g_node, DMDRVI_O_RDONLY, &g_number);
    DMOD_TEST_EXPECT_EQ(subscribe(reader, "dmnotify"), -ENOENT);            /* loaded, but no _on_notify */
    DMOD_TEST_EXPECT_EQ(subscribe(reader, "no_such_module"), -ENOENT);
    DMOD_TEST_EXPECT_EQ(subscribe(reader, ""), -EINVAL);
    DMOD_TEST_EXPECT_EQ(subscribe(reader, NULL), -EINVAL);
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, reader, DMNOTIFY_IOCTL_CMD_SUBSCRIBE, NULL), -EINVAL);
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, reader, DMNOTIFY_IOCTL_CMD_UNSUBSCRIBE, NULL), -ENOENT);
    DMOD_TEST_EXPECT_EQ(subscribe(reader, SUBSCRIBER), 0);
    DMOD_TEST_EXPECT_EQ(subscribe(reader, SUBSCRIBER), -EBUSY);
    g_close(g_node, reader);
}

DMOD_TEST_STEP(unsubscribing_and_closing_end_the_subscription)
{
    void* writer = g_open(g_node, DMDRVI_O_WRONLY, &g_number);
    void* first = g_open(g_node, DMDRVI_O_RDONLY, &g_number);
    void* second = g_open(g_node, DMDRVI_O_RDONLY, &g_number);
    DMOD_TEST_EXPECT_EQ(subscribe(first, SUBSCRIBER), 0);
    DMOD_TEST_EXPECT_EQ(subscribe(second, SUBSCRIBER), 0);
    DMOD_TEST_EXPECT_EQ(notify(writer, "a", 1, "to both files", NULL), 0);
    DMOD_TEST_EXPECT_EQ(told(), 2u);
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, first, DMNOTIFY_IOCTL_CMD_UNSUBSCRIBE, NULL), 0);
    DMOD_TEST_EXPECT_EQ(notify(writer, "a", 2, "to one", NULL), 0);
    DMOD_TEST_EXPECT_EQ(told(), 3u);
    g_close(g_node, second);
    DMOD_TEST_EXPECT_EQ(notify(writer, "a", 3, "to none", NULL), 0);
    DMOD_TEST_EXPECT_EQ(told(), 3u);
    g_close(g_node, first);
    g_close(g_node, writer);
}

DMOD_TEST_STEP(a_notification_is_checked)
{
    void* file = g_open(g_node, DMDRVI_O_RDWR, &g_number);
    DMOD_TEST_EXPECT_EQ(subscribe(file, SUBSCRIBER), 0);
    DMOD_TEST_EXPECT_EQ(notify(file, NULL, 0, "no source", NULL), -EINVAL);
    DMOD_TEST_EXPECT_EQ(notify(file, "", 0, "empty source", NULL), -EINVAL);
    DMOD_TEST_EXPECT_EQ(notify(file, "a", 0, NULL, "no brief"), -EINVAL);
    dmnotify_notification_t n;
    memset(&n, 0, sizeof(n));
    n.brief = "b";
    n.source = "a";
    n.privacy = (dmnotify_privacy_t)0x10;
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, file, DMNOTIFY_IOCTL_CMD_NOTIFY, &n), -EINVAL);
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, file, DMNOTIFY_IOCTL_CMD_NOTIFY, NULL), -EINVAL);
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, file, DMNOTIFY_IOCTL_CMD_CANCEL, NULL), -EINVAL);
    DMOD_TEST_EXPECT_EQ(g_ioctl(g_node, NULL, DMNOTIFY_IOCTL_CMD_NOTIFY, &n), -EINVAL);
    DMOD_TEST_EXPECT_EQ(told(), 0u);
    g_close(g_node, file);
}

DMOD_TEST_STEP(the_texts_are_copied)
{
    void* file = g_open(g_node, DMDRVI_O_RDWR, &g_number);
    char brief[16] = "kept";
    char source[16] = "owner";
    DMOD_TEST_EXPECT_EQ(notify(file, source, 0, brief, NULL), 0);
    memset(brief, 'x', sizeof(brief) - 1);
    memset(source, 'y', sizeof(source) - 1);
    char text[64];
    read_all(file, text, sizeof(text));
    DMOD_TEST_EXPECT_STR_EQ(text, "owner [0]: kept\n");
    g_close(g_node, file);
}

DMOD_TEST_STEP(a_text_written_to_the_node_is_a_notification)
{
    void* file = g_open(g_node, DMDRVI_O_RDWR, &g_number);
    DMOD_TEST_EXPECT_EQ(subscribe(file, SUBSCRIBER), 0);
    DMOD_TEST_EXPECT_EQ(g_write(g_node, file, "Tea is ready\n", 13, 0), 13);
    DMOD_TEST_EXPECT_EQ(told(), 1u);
    DMOD_TEST_EXPECT_STR_EQ(g_records[0].source, "console");
    DMOD_TEST_EXPECT_STR_EQ(g_records[0].brief, "Tea is ready");
    DMOD_TEST_EXPECT_EQ(g_records[0].privacy, dmnotify_privacy_full_public);
    char text[64];
    read_all(file, text, sizeof(text));
    DMOD_TEST_EXPECT_STR_EQ(text, "console [0]: Tea is ready\n");
    DMOD_TEST_EXPECT_EQ(g_write(g_node, file, "\n", 1, 0), 1);                /* what a shell writes after the text: nothing happens */
    DMOD_TEST_EXPECT_EQ(g_write(g_node, file, "", 0, 0), 0);
    DMOD_TEST_EXPECT_EQ(told(), 1u);
    read_all(file, text, sizeof(text));
    DMOD_TEST_EXPECT_STR_EQ(text, "console [0]: Tea is ready\n");
    DMOD_TEST_EXPECT_EQ(cancel(file, "console", 0), 0);                        /* the ioctl takes it back */
    DMOD_TEST_EXPECT_EQ(told(), 2u);
    DMOD_TEST_EXPECT_TRUE(g_records[1].cancel);
    read_all(file, text, sizeof(text));
    DMOD_TEST_EXPECT_STR_EQ(text, "");
    g_close(g_node, file);
}

DMOD_TEST_STEP(a_subscriber_cannot_use_the_node_from_its_handler)
{
    void* file = g_open(g_node, DMDRVI_O_RDWR, &g_number);
    DMOD_TEST_EXPECT_EQ(subscribe(file, SUBSCRIBER), 0);
    test_listener_reenter(g_ioctl, g_node, file);
    DMOD_TEST_EXPECT_EQ(notify(file, "a", 0, "once", NULL), 0);
    DMOD_TEST_EXPECT_EQ(test_listener_reenter_result(), -EDEADLK);
    DMOD_TEST_EXPECT_EQ(told(), 1u);
    test_listener_reenter(NULL, NULL, NULL);
    DMOD_TEST_EXPECT_EQ(notify(file, "a", 1, "works again", NULL), 0);
    DMOD_TEST_EXPECT_EQ(told(), 2u);
    g_close(g_node, file);
}
