# dmnotify API Reference

dmnotify has no functions of its own to call: it is used through its node (`/dev/notify`) - `Dmod_FileOpen`, `Dmod_Ioctl`,
`Dmod_FileRead`, `Dmod_FileWrite`, `Dmod_FileClose` - and it calls the DIF its subscribers implement.

## Types (`dmnotify_types.h`)

### `dmnotify_notification_t`

| Field | | |
|---|---|---|
| `brief` | `const char*` | Required. The short text: who or what ("#test", "Download done"). |
| `message` | `const char*` | Optional (NULL). The content ("Marek: hello"). |
| `source` | `const char*` | Required. The module the notification is from - an interface can open the application of this name. |
| `icon_path` | `const char*` | Optional (NULL). The path of an icon. |
| `privacy` | `dmnotify_privacy_t` | What can be shown on a locked screen. |
| `channel` | `dmnotify_channel_t` | What the notification is about, for its source. Source + channel are the identity of a notification. |

The texts belong to the caller; the driver copies them. A module has no relocated data, so fill the structure field by field
(after a `memset`) rather than with an initializer of constant texts.

### `dmnotify_privacy_t`

| Value | Meaning |
|---|---|
| `dmnotify_privacy_public_brief`, `_public_source`, `_public_message` | The bits: that part can be shown on a locked screen. |
| `dmnotify_privacy_full_private` | Nothing can. |
| `dmnotify_privacy_private_message` | The brief and the source can, the message cannot. |
| `dmnotify_privacy_full_public` | All of it can. |

### `dmnotify_channel_t`

`uint32_t`: the number a source gives to one thing it notifies about.

### `dmnotify_subscribe_t`

`module_name`: the module that implements the DIF below and wants the notifications.

## Ioctl commands (`dmnotify_ioctl.h`)

| Command | Argument | Open for | Does | Errors |
|---|---|---|---|---|
| `DMNOTIFY_IOCTL_CMD_NOTIFY` | `const dmnotify_notification_t*` | writing | Keeps the notification as active (replacing the one of the same source and channel) and tells the subscribers. | `-EINVAL` no source, no brief, unknown privacy bits; `-ENOMEM` |
| `DMNOTIFY_IOCTL_CMD_CANCEL` | `const dmnotify_notification_t*` (`source`, `channel`) | writing | Takes the notification back and tells the subscribers. | `-EINVAL` no source; `-ENOENT` not active |
| `DMNOTIFY_IOCTL_CMD_SUBSCRIBE` | `const dmnotify_subscribe_t*` | reading | Subscribes the module through this file; delivers what is active. | `-EINVAL` no name; `-ENOENT` the module does not implement `_on_notify`; `-EBUSY` the file has a subscription; `-ENOMEM` |
| `DMNOTIFY_IOCTL_CMD_UNSUBSCRIBE` | not used | reading | Ends the subscription of this file. | `-ENOENT` none |

All of them: `-EACCES` when the file is not open for what the command needs, `-EDEADLK` when called from `_on_notify` /
`_on_cancel`, `-ENOTTY` for any other command.

## The DIF of a subscriber (`dmnotify.h`)

```c
dmod_dmnotify_dif( 1.0, void, _on_notify, ( const dmnotify_notification_t* notification ) );
dmod_dmnotify_dif( 1.0, void, _on_cancel, ( const dmnotify_notification_t* notification ) );
```

| Function | Called when |
|---|---|
| `_on_notify` | A notification is new, or replaces the one of the same source and channel. Also for every active one right after subscribing. Required. |
| `_on_cancel` | A notification is taken back (by its source, or because there were too many). Optional. |

Both run in the thread of the module that notifies, with the driver locked: they copy and return. The structure is valid only
during the call.

## read() and write()

| | |
|---|---|
| `read` (open for reading) | The active notifications as text, the newest first, one a line: `source [channel]: brief - message`. |
| `write` (open for writing) | The text is a public notification of the source `console` on channel 0 (the end of the line is dropped, at most 256 bytes are taken); an empty line cancels it. |

## Configuration (the .ini of the node)

| Key | Default | |
|---|---|---|
| `driver_name` | | `dmnotify` |
| `max_notifications` | 16 | The active notifications kept; one more cancels the oldest. |

The section names the node: `[notify]` is `/dev/notify`.
