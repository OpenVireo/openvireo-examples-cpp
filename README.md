# OpenVireo examples: C++

Runnable examples for the [OpenVireo](https://openvireo.com) C++ binding (`libovcore`, `<ovcore.h>`), one
file per guide on [developer.openvireo.com](https://developer.openvireo.com). Each file holds the code from
its guide's **C++** tab, wrapped in a program you can run.

Examples in other languages: [TypeScript](https://github.com/openvireo/openvireo-examples-typescript) · [Python](https://github.com/openvireo/openvireo-examples-python) · [Kotlin](https://github.com/openvireo/openvireo-examples-kotlin) · [Java](https://github.com/openvireo/openvireo-examples-java) · [Swift](https://github.com/openvireo/openvireo-examples-swift) · [ov CLI](https://github.com/openvireo/openvireo-examples-cli)

## Run them

You need CMake 3.21+, a C++17 compiler and libcurl (macOS has it; on Debian or Ubuntu,
`apt install libcurl4-openssl-dev`). CMake downloads the `libovcore` 1.0.0 package for your platform
from the [core release](https://github.com/openvireo/openvireo-core/releases/tag/v1.0.0) and checks it
against the release's checksums. To use a package you built, pass
`-DOpenVireoCore_DIR=<pkg>/lib/cmake/OpenVireoCore`.

```sh
git clone https://github.com/openvireo/openvireo-examples-cpp
cd openvireo-examples-cpp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
build/examples check
```

Set your account in the environment. `OV_SECRET_KEY` is only needed the first time on a device;
`OV_ISSUER` picks an account server other than `https://accounts.openvireo.com`.

```sh
export OV_EMAIL=you@example.com OV_PASSWORD='...' OV_SECRET_KEY='A3-...'
```

```sh
build/examples first-program "ov://Personal/GitHub/username"
build/examples vaults-and-sharing
```

Every core call blocks: a GUI app makes them on a worker thread and cancels with the `CancelToken`.

### The Host

The core does no I/O: every program passes it a Host for HTTP, a secure store, an offline cache, a
clock and logging. [`src/host.cpp`](src/host.cpp) is a small one: libcurl with no redirects and no
cookies, and memory for the store and the cache. A desktop app keeps the store in the OS keychain and
the cache on disk so it can open vaults offline.

### Another core release

`scripts/update-core.sh <version>` points `cmake/OpenVireoCore.cmake` at another release, with the
checksums from its `SHA256SUMS`.

## The examples

| Example | What it does | Guide |
|---|---|---|
| [`first_program.cpp`](src/first_program.cpp) | Sign in, list the vaults, read one field, sign out | [A first program](https://developer.openvireo.com/sdk/#a-first-program) |
| [`hosted_account.cpp`](src/hosted_account.cpp) | Connect, sign up, sign in with two-step codes, lock, devices, account deletion | [Hosted accounts](https://developer.openvireo.com/sdk/guides/hosted-account/) |
| [`vaults_and_sharing.cpp`](src/vaults_and_sharing.cpp) | List, open, read, write and edit vaults; share, revoke, organisations | [Vaults and sharing](https://developer.openvireo.com/sdk/guides/vaults-and-sharing/) |
| [`events.cpp`](src/events.cpp) | Follow live changes from other devices | [Events](https://developer.openvireo.com/sdk/guides/events/) |
| [`offline.cpp`](src/offline.cpp) | Edit offline, merge when back online | [Offline](https://developer.openvireo.com/sdk/guides/offline/) |
| [`move_to_account.cpp`](src/move_to_account.cpp) | Import a `.kdbx` file as a new vault or merge it into one | [Moving to an account](https://developer.openvireo.com/sdk/guides/move-to-account/) |
| [`recovery.cpp`](src/recovery.cpp) | Inspect a recovery link, reset, restore the old vaults | [Recovery](https://developer.openvireo.com/sdk/guides/recovery/) |
| [`self_hosted_sync.cpp`](src/self_hosted_sync.cpp) | Pair with a self-hosted sync server and sync a `.kdbx` file | [Self-hosted sync](https://developer.openvireo.com/sdk/guides/self-hosted-sync/) |
| [`service_accounts.cpp`](src/service_accounts.cpp) | Create a service account; read, inject and write with its token | [Service accounts](https://developer.openvireo.com/sdk/guides/service-accounts/) |
| [`share_links.cpp`](src/share_links.cpp) | Share an item or text by link, list and revoke links, open one | [Share links](https://developer.openvireo.com/sdk/guides/share-links/) |

Each file has one function per section of its guide. Running an example does something safe and
mostly read-only; the functions that change an account for good (deleting it, revoking access,
rotating keys) are there to read and copy, and the programs don't call them. Try the rest on a test
account. `check` reaches the account server without signing in, to test the setup.

## Checks

CI builds the examples with `-Wall -Wextra -Werror` (`/W4 /WX` on Windows) and runs `check` on Linux
and macOS, on every push and weekly.

## License

MIT. Copy the code into your own programs.
