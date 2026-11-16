# dmnotify

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/choco-technologies/dmnotify/actions/workflows/ci.yml/badge.svg)](https://github.com/choco-technologies/dmnotify/actions/workflows/ci.yml)

The notifications of a DMOD system, as a device node.

## Description

dmnotify is a driver (dmdrvi): dmdevfs makes a node of it, `/dev/notify`. A module that has something to tell the user
(a message came, a download is done, the battery is low) opens the node for writing and **notifies**. The interface of the device
(a launcher, a status bar) opens it for reading, **subscribes** and is called with every notification - and with every one that is
taken back.

It is a driver on purpose: who may see the notifications and who may only send them is decided by the access rights of the
node in the file system, so there is no second mechanism of privacy to keep.

```
  libmeshcore --NOTIFY--> +-------------+ --_on_notify--> phoneui (the home screen)
  a shell ----write()---> | /dev/notify | --_on_cancel-->
  meshcore ---CANCEL----> +-------------+ <--SUBSCRIBE---
```

- A notification is a brief, an optional message, its source (the name of a module), an optional icon, a privacy setting and
  a channel. The source and the channel are its identity: the same source on the same channel replaces the notification, a
  cancel names them.
- The driver keeps the active notifications (`max_notifications`, 16 by default; one more cancels the oldest), so a subscriber
  that comes later gets what is active at once.
- A subscriber is a module that implements the DIF of `dmnotify.h` (`_on_notify`, optionally `_on_cancel`).
- From a shell: `echo "Tea is ready" > /dev/notify` notifies, `cat /dev/notify` lists what is active.

See [docs/README.md](docs/README.md) for the whole story and [docs/api-reference.md](docs/api-reference.md) for the reference.

## Building

### Using CMake

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

CMake automatically uses an installed `dmod-dev` SDK. Without an SDK it fetches
dmod's `develop` branch from GitHub. With the SDK installed, a self-contained
module builds without a DMOD checkout or network access.

Pass `-DDMOD_DIR=/path/to/local/dmod` to select a local checkout, even when the
SDK is installed. The checkout needs its submodules: use `git clone --recursive`
or run `git submodule update --init --recursive` in an existing checkout.
To force the GitHub source build while an SDK is installed, pass
`-DDMOD_USE_INSTALLED_SDK=OFF`. Use separate build directories when switching
between source and installed SDK builds.

For an SDK in a custom prefix, pass `-DCMAKE_PREFIX_PATH=/path/to/prefix`.
Existing `DMOD_TOOLS_NAME` profiles also work with the SDK, for example
`-DDMOD_TOOLS_NAME=arch/armv7/cortex-m7` with the corresponding compiler
installed.

### Using Make

```bash
make DMOD_MODE=DMOD_MODULE DMOD_DIR=/path/to/dmod
```

The legacy Make integration uses a source checkout. For a `dmod-dev` installation,
use the CMake commands above.

## Testing

Tests are built automatically alongside the module (see `tests/`). Once built,
run them with `ctest`:

```bash
cd build
ctest --output-on-failure
```

`ctest` installs the test module's dependencies with `dmf-get` and then runs
it through `dmod_loader`. To run it manually instead:

```bash
export DMOD_DMF_DIR=$(pwd)/build/dmf
dmf-get install -d ${DMOD_DMF_DIR}/test_dmnotify-local.dmd -y
dmod_loader build/dmf/test_dmnotify.dmf
```

## Usage

Add the node to the system (dmdevfs; `configs/notify.ini` is in the package):

```
dmnotify driver=notify.ini
```

Notify (a module that links `dmnotify` for its headers):

```c
#include "dmnotify_ioctl.h"

void* node = Dmod_FileOpen("/dev/notify", "w");
dmnotify_notification_t n;
memset(&n, 0, sizeof(n));
n.source  = "meshcore";                         /* the module: the application a tap on the notification opens */
n.channel = 3;                                  /* what it is about, for the source: here the index of a chat */
n.brief   = "#test";
n.message = "Marek: hello";
n.privacy = dmnotify_privacy_private_message;   /* the message is not for a locked screen */
Dmod_Ioctl(node, DMNOTIFY_IOCTL_CMD_NOTIFY, &n);
/* ... the user has read it ... */
Dmod_Ioctl(node, DMNOTIFY_IOCTL_CMD_CANCEL, &n);
```

Subscribe (the module `myui`):

```c
#define DMOD_ENABLE_REGISTRATION ON
#include "dmnotify.h"
#include "dmnotify_ioctl.h"

dmod_dmnotify_dif_api_declaration(1.0, myui, void, _on_notify, ( const dmnotify_notification_t* notification ))
{
    /* called in the thread of whoever notifies: copy and return */
}

dmod_dmnotify_dif_api_declaration(1.0, myui, void, _on_cancel, ( const dmnotify_notification_t* notification ))
{
}

void* node = Dmod_FileOpen("/dev/notify", "r");
dmnotify_subscribe_t s;
s.module_name = "myui";
Dmod_Ioctl(node, DMNOTIFY_IOCTL_CMD_SUBSCRIBE, &s);    /* lasts until the file is closed */
```

## Documentation

See the `docs/` directory:

- **[api-reference.md](docs/api-reference.md)** - Complete API documentation

View documentation using `dmf-man dmnotify`.
## Project Structure

```
dmnotify/
├── docs/              # Documentation (markdown format)
├── include/           # Public headers
│   └── dmnotify.h
├── src/
│   └── dmnotify.c
├── tests/
│   ├── CMakeLists.txt
│   └── dmnotify_test.c
├── CMakeLists.txt
├── Makefile
├── dmnotify.dmr
└── manifest.dmm
```

## Author

Patryk Kubiak

## License

MIT
