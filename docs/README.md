# dmnotify Documentation

dmnotify is the notification node of a DMOD system: a driver (dmdrvi) that dmdevfs shows as `/dev/notify`.

## Contents

- **[api-reference.md](api-reference.md)** - the types, the ioctl commands, the DIF of a subscriber, the configuration

## Why a device node

A notification can be private (who wrote, what they wrote), so not every module should be able to read the notifications of the
others. Instead of a mechanism of its own, dmnotify uses the one the system has: the access rights of a file.

- A file of the node opened **for writing** can notify and cancel. It cannot read or subscribe.
- A file opened **for reading** can read the list and subscribe. It cannot notify.
- So whoever sets the rights of `/dev/notify` decides which modules see the notifications and which only send them.

## The life of a notification

1. A module opens the node for writing and sends `DMNOTIFY_IOCTL_CMD_NOTIFY` with a `dmnotify_notification_t`. The driver copies
   it (the texts belong to the caller), keeps it as active and calls `_on_notify` of every subscriber before the ioctl returns.
2. The **source** (the name of the module) and the **channel** (a number the source gives to what it notifies about - a chat,
   a download, an alarm) are the identity of the notification. A new one of the same source and channel replaces the old one:
   the subscribers get `_on_notify` again and replace theirs.
3. `DMNOTIFY_IOCTL_CMD_CANCEL` (only the source and the channel are read) takes it back: `_on_cancel` of every subscriber.
4. The driver keeps at most `max_notifications` active; when one more comes, the oldest is cancelled.

## Subscribing

A subscriber is a module that implements the DIF of `dmnotify.h`: `_on_notify`, and `_on_cancel` if it wants to know what is
taken back. It opens the node for reading and sends `DMNOTIFY_IOCTL_CMD_SUBSCRIBE` with its name. What is active at that moment
is delivered at once, the oldest first. The subscription belongs to the open file: `DMNOTIFY_IOCTL_CMD_UNSUBSCRIBE` or closing
the file ends it (so it also ends when the process of the subscriber does).

The driver looks the functions of the subscriber up by its name every time it delivers, so a module that was unloaded is skipped.

Rules for the two functions:

- They are called **in the thread of the module that notifies**, inside its ioctl, with the driver locked. Copy what is needed
  and return: no waiting, no drawing, little stack.
- The structure and its texts are valid only until the function returns.
- The node cannot be used from them (an ioctl or a write gives `-EDEADLK`).

> A dmod before the change "a running application is looked at for a DIF" finds the implementations of a DIF only in the
> libraries (the enabled modules): there an application cannot subscribe, its library can.

## Privacy

`dmnotify_privacy_t` says what of a notification can be shown on a locked screen: nothing (`dmnotify_privacy_full_private`), the
brief and the source but not the message (`dmnotify_privacy_private_message`), or all of it (`dmnotify_privacy_full_public`).
The driver delivers every notification to every subscriber: obeying the setting is the job of the interface that shows them.

## From a shell

```
echo "Tea is ready" > /dev/notify     # a public notification of the source "console" on channel 0
cat /dev/notify                       # the active notifications, the newest first: "source [channel]: brief - message"
echo "" > /dev/notify                 # takes the one of the console back
```

## Configuration

`configs/notify.ini` (and `notify-small.ini` for a device with little RAM):

```ini
[notify]
driver_name=dmnotify
max_notifications=16
```

The node is named after the section: `[notify]` gives `/dev/notify`, the name the clients look for.

View documentation using `dmf-man`:

```bash
dmf-man dmnotify          # Main documentation
dmf-man dmnotify api      # API reference
```
