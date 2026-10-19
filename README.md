# EmbedThis Ioto Device Agent

<p align="center">
  <a href="https://github.com/embedthis/ioto/actions/workflows/ci.yml"><img src="https://github.com/embedthis/ioto/actions/workflows/ci.yml/badge.svg" alt="CI Status"></a>
</p>

The EmbedThis Ioto device agent is a compact IoT agent for embedded device management over a local network. It includes an HTTP web server, HTTP client, MQTT client, embedded database and JSON data management.

These components are tightly integrated to provide an extremely efficient and powerful device agent. This distribution includes several [Sample Apps](./apps) to help you get started with Ioto.

Contact [EmbedThis](https://www.embedthis.com/) for information about cloud-based device management.

See the [README-ESP32](./README-ESP32.md) for building Ioto on the ESP32 platform.
See the [README-FREERTOS](./README-FREERTOS.md) for building Ioto on FreeRTOS.

Full documentation is available at:

* https://www.embedthis.com/doc/

## Licensing

Ioto is commercial software and is provided under the following licenses.

See [LICENSE.md](LICENSE.md) and [EVAL.md](EVAL.md) for details. It is also offered with a
[GPLv2](https://www.gnu.org/licenses/gpl-2.0.html) license from the
[Embedthis GitHub Repository](https://github.com/embedthis/ioto).

## Sample Apps

The Ioto distribution includes several sample apps. Apps demonstrate device-side logic to implement various
management use cases.

The default app is "http" which runs the embedded web server.

Name | Description
-|-
blank | Empty slate application.
blink | Minimal ESP32 blink app to demonstrate linking with Ioto on ESP32 microcontrollers.
http | Simple embedded web server user/group authentication sample.
unit | Unit testing app.

Each application has a README.md in the apps/APP directory that describes the application.

## Building from Source

Ioto releases are available as source code distributions from the
[Builder Site](https://admin.embedthis.com/product). To download, first create an account and login, then navigate
to the product list, select the Ioto Eval and click the download link.

[Download Source Package](https://admin.embedthis.com/product)

The Ioto source distribution contains all the required source files, headers, and build tools.

Ioto is cross-platform and runs on a wide variety of operating systems and CPU architectures. EmbedThis offers
different levels of verification for different platforms. The platform support tiers are:

### Tier 1

Tier 1 platforms are those that are supported and verified by Ioto. Each release is tested and verified on these
platforms. The tier 1 platforms are:

- Linux
- Mac OS X
- Windows

### Tier 2

Tier 2 platforms are those that are supported by Ioto but each release is not always tested and verified on these
platforms. The tier 2 platforms are:

- ESP32
- FreeBSD
- FreeRTOS
- VxWorks

### Tier 3

Tier 3 platforms are those that customers have ported Ioto to and may work with little or no effort by customers.

For tier 3 environments, you will need to cross-compile. The source code has been designed to run on Arduino, ESP32,
FreeBSD, FreeRTOS, Linux, Mac OS X, VxWorks and other operating systems. Ioto supports the X86, X64, Riscv, Riscv64,
Arm, Arm64, and other CPU architectures. Ioto can be ported to new platforms, operating systems and CPU architectures.
Ask us if you need help doing this.

See [Porting Ioto](https://www.embedthis.com/doc/agent/user/hardware.html#porting-ioto-to-a-new-platform/) for
porting details.

## Build Configuration

When building Ioto, you do not need to use a `configure` program. Instead, you simply run **make**. 

The selectable Ioto services are:

* ai -- Enable the AI service
* database -- Enable the embedded database
* mqtt -- Enable MQTT protocol
* url -- Enable client HTTP request support
* web -- Enable the local embedded web server

## Using Ioto as a Component

Ioto is provided as a static library that you link into your own application. This lets you add IoT connectivity, an embedded web server, MQTT, database and other services to your product with minimal effort.

There are two ways to integrate Ioto:

1. **Use your own main** -- Create your own `main()` and link with the Ioto library. Best when you need full control over startup, or when integrating with an RTOS like FreeRTOS.
2. **Use the Ioto main** -- Provide **ioStart** and **ioStop** functions that Ioto calls during startup and shutdown. Simpler if you are running on a generic Linux or macOS system.

### Project Structure

A typical application that embeds Ioto has this directory layout:

```
myapp/
├── src/                    # Your application source code
├── Makefile                # Your build file
├── ioto/                   # Extracted Ioto source distribution
│   ├── include/            # Ioto headers (ioto.h and dependencies)
│   ├── build/
│   │   └── bin/            # Built library and tools
│   │       └── libioto.a   # Static library (Linux/macOS)
│   └── ...
└── state/
    └── config/             # Runtime configuration files
        ├── ioto.json5      # Main Ioto configuration
        ├── device.json5    # Device identification
        └── web.json5       # Web server config (if web enabled)
```

### Building the Ioto Library

Extract the Ioto source distribution and build:

    $ tar xf ioto-VERSION.tar.gz
    $ mv ioto-* ioto
    $ make -C ioto

This builds the Ioto static library and utility tools in **ioto/build/bin/**. The primary build outputs are:

| File | Description |
|-|-|
| `ioto/build/bin/libioto.a` | Static library (Linux/macOS) |
| `ioto/build/bin/ioto.lib` | Static library (Windows) |
| `ioto/include/ioto.h` | Main include header |

### Configuring Services

Before building the Ioto library, you can enable or disable services by editing **ioto/include/config.h**. Each service is controlled by a `SERVICES_*` define. Set to `1` to enable or `0` to disable.

| Define | Description |
|-|-|
| `SERVICES_AI` | AI / OpenAI integration |
| `SERVICES_DATABASE` | Embedded database |
| `SERVICES_MQTT` | MQTT protocol client |
| `SERVICES_URL` | HTTP client requests |
| `SERVICES_WEB` | Embedded web server |

Some services have dependencies. For example, `SERVICES_SYNC` requires `SERVICES_DATABASE` and `SERVICES_MQTT`. The `ioto.h` header will automatically enable required dependent services.

After modifying config.h, rebuild the library:

    $ make -C ioto clean
    $ make -C ioto

### Configuration Files

Your application must provide a **state/config/** directory containing the Ioto runtime configuration files. Ioto reads these files at startup.

**Required files:**

| File | Description |
|-|-|
| ioto.json5 | Main configuration: services, logging, TLS, limits |
| device.json5 | Device identification: product token, name, model |

**Optional files:**

| File | Description |
|-|-|
| web.json5 | Web server configuration (required if web service is enabled) |
| local.json5 | Development overrides (merged over ioto.json5) |
| schema.json5 | Database schema (required if database service is enabled) |

The `ioto.json5` file controls which services are active at runtime via its **services** property. This is separate from the compile-time config.h settings -- a service must be both compiled in and enabled in ioto.json5 to be active.

See `apps/http/` for examples of these configuration files.

See the [Embedding Ioto](#embedding-ioto) section below for the code required to initialize and run Ioto, and
[Linking with the Ioto Library](#linking-with-the-ioto-library) for compiler and linker flags.

## Building

If you are building on Windows, or for ESP32 or FreeRTOS, please read the specific instructions for various build
environments:

* [Building on Windows](#building-on-windows)
* [Building for the ESP32](README-ESP32.md)
* [Building for FreeRTOS](README-FREERTOS.md)

## Building with Make

To build on Linux, MacOS or Windows via WSL, use the system **make** command. The supplied Makefile will build the
Ioto library (libioto.a), utility tools (password, db, json, url, web) and all application binaries.
Each app is built as `ioto-NAME` (e.g., `ioto-http`, `ioto-blank`) and placed in `build/bin/`.
If you are embedding Ioto in another program, you should link the Ioto library with your program.
See [Linking](#linking-with-the-ioto-library) below for details.

### Make Targets

Target | Description
-|-
make | Build the library, all utility tools, and all apps (default)
make lib | Build libioto.a and utility tools only
make apps | Build all app binaries (requires library)
make *name* | Shortcut to build a single app (e.g. `make http`, `make blank`)
make run | Build and run the default app (http)
make test | Build the debug configuration and run the unit test suite
make clean | Remove all build artifacts
make verify-projects | Verify projects/gmake2 matches what premake5.lua generates (requires premake5)
make help | Show available targets and options

### Make Variables

Variable | Description
-|-
OPTIMIZE=release\|debug | Build configuration (default: release). `make test` builds debug itself
SHOW=1 | Display compiler/linker commands during build

### Build Flow

A full `make` (or `make build`) runs these steps in order:

1. **lib** -- Builds `libioto.a`, utility tools (password, db, json, url, web) and `gen-config` using pre-generated premake makefiles under `projects/gmake2/`.
2. **apps** -- Runs each app's `bin/prepare` script to copy and blend configuration files into `apps/<APP>/state/config/`, then builds each app binary (e.g. `ioto-http`, `ioto-blank`) using pre-generated premake makefiles under `apps/<APP>/projects/gmake2/`.

To build a single app by name:

    $ make http

## Building on Windows

For Windows, you can build one of three ways:

* Using Windows WSL and make
* Using Windows Visual Studio
* Using the Windows Command Prompt

We recommend building with WSL initially to evaluate the Ioto agent.

### Building with Windows WSL

Using the [Windows Subsystem for Linux (WSL)](https://learn.microsoft.com/en-us/windows/wsl/about) you get a tightly
integrated Linux environment from which you can build and debug using VS Code.

To build, first [install WSL](https://learn.microsoft.com/en-us/windows/wsl/install) by running the following command
as an administrator:

    $ wsl --install

Then invoke **wsl** to run a wsl (bash) shell:

    $ wsl

To configure WSL for building, install the following packages from the wsl shell.

    $ apt update
    $ apt install make gcc build-essential libc6-dev openssl libssl-dev

Then extract the Ioto source distribution:

    $ tar xvfz ioto-VERSION.tgz ioto

Finally build via **make**:

    $ cd ioto
    $ make

To debug with VS Code, add the
[WSL extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-wsl) to VS Code and
then from a WSL terminal, open VS Code from the ioto directory:

    $ cd ioto
    $ code .

This will open a remote WSL project for the Ioto distribution.

### Building with Windows Visual Studio

To build natively on Windows, you will need Visual Studio 2022 or later. You can download the installer from the
[Visual Studio website](https://visualstudio.microsoft.com/downloads/).

You will also need OpenSSL installed. You can use `vcpkg` to install it. Newer versions of Visual Studio include
vcpkg. If you do not have vcpkg, you can install it by following the
[vcpkg documentation](https://learn.microsoft.com/en-us/vcpkg/get_started/overview).

Install OpenSSL via vcpkg:

```bash
vcpkg install openssl
```

To build, first open the Ioto library solution file:

    projects/vs2022/ioto.sln

Set the "Debug" configuration and build the solution. This builds the Ioto library and utility tools.

Then open the app solution for the app you want to build. For example, for the http app:

    apps/http/projects/vs2022/ioto-http.sln

Build the app solution and set `ioto-http` as the startup project. Edit the project properties and set the working
directory to the app directory (e.g., `apps/http`).

The projects link the dynamic C runtime (`/MD`, and `/MDd` for debug), so Windows Update delivers C runtime security fixes without a rebuild. An application that links `ioto.lib` must also build with `/MD` (or `/MDd` for a debug build): mixing the static and dynamic runtimes fails to link.

### Building from the Windows Command Prompt

You can build from the Windows command line using the supplied `make.bat` which invokes `msbuild` on the VS2022
solution files. First, open a Visual Studio Developer Command Prompt, or set up the environment manually:

```bash
"c:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

The path varies by Visual Studio edition (Community, Professional, Enterprise).

Install OpenSSL via vcpkg as described in the [Visual Studio](#building-with-windows-visual-studio) section, then
set the `ME_COM_OPENSSL_PATH` environment variable:

```bash
SET ME_COM_OPENSSL_PATH=C:\vcpkg\installed\x64-windows
make
```

## Running Ioto

To build and run the default app (http), type:

    $ make run

Or add the directory **build/bin** to your PATH environment variable and run any app directly via `ioto-NAME`:

    export PATH=`make path`

    $ ioto-http -v
    $ ioto-blank -v

On Windows:

```bash
SET PATH=%PATH%;%CD%\build\bin
ioto-http -v
```

Each app binary is named `ioto-NAME` (e.g., `ioto-http`, `ioto-blank`, `ioto-unit`). If the app enables the web server, Ioto listens on the loopback interface only, at http://127.0.0.1:8080 and, for apps that configure https, https://127.0.0.1:4443, and serves documents from the **site** directory when run with the **dev** profile. When run with the **prod** profile, it will serve documents from **/var/www/ioto**. You can change the listening endpoints in **web.json5**. Bind a network interface only once TLS, accounts and route roles are configured for deployment, as described in the [Ioto documentation](https://www.embedthis.com/doc/) and, in the commercial source distribution, in `doc/user-information.md`.

## Running the Web Server

If you only need a web server without the database or other Ioto services, you can run the **web** program directly
instead of the **ioto-NAME** app. For example:

    $ web

This reads **web.json5** from the current directory, or the file named by `--config`, and listens on the endpoints
it names. Configure loopback endpoints such as http://127.0.0.1:8080 and https://127.0.0.1:4443 until the server is
ready to face a network.

## Changing the TLS Stack

Ioto includes support for two TLS stacks: OpenSSL and MbedTLS. Ioto is built with OpenSSL by default. OpenSSL is a
leading open source TLS stack that is faster, but bigger. MbedTLS is a compact TLS implementation. When built for
ESP32, the MbedTLS stack supplied by ESP-IDF is used, which sets the ESP-IDF releases Ioto supports (see
[README-ESP32.md](README-ESP32.md)).

The runtime refuses to compile against an unsupported TLS library version:

TLS Stack | Minimum Version
-|-
OpenSSL | 3.0
MbedTLS | 3.6.7, in the 3.6 LTS line. MbedTLS 4.x is not supported.

To build with MbedTLS, install MbedTLS 3.6.7 or later in the 3.6 line using your O/S package manager or from the
[MbedTLS releases](https://github.com/Mbed-TLS/mbedtls/releases). Many Linux distributions package an older MbedTLS
(for example 2.28), which the runtime refuses.

For example on Mac OS (Homebrew's **mbedtls** formula is 4.x, so use **mbedtls@3**):

    $ brew install mbedtls@3

The supplied projects are generated for OpenSSL. Regenerate the library and app projects for MbedTLS (see
[Premake Configuration](#premake-configuration)) and build:

    $ MBEDTLS=/opt/homebrew/opt/mbedtls@3
    $ for dir in projects apps/*/projects ; do (cd $dir && premake5 --tls=mbedtls --mbedtls-path=$MBEDTLS gmake2) ; done
    $ make

The unit tests follow the same selection: regenerating `projects` also writes `projects/gmake2/tls-<os>.rsp`, which `make test` passes to the compiler, so the tests build with the provider and libraries the library uses. On Linux the TLS libraries must be shared (for MbedTLS built from source, configure with `-DUSE_SHARED_MBEDTLS_LIBRARY=On`).

### Trusted Root Certificates

The default `tls.authority` in each app's `ioto.json5` is `@certs/roots.crt`, the Mozilla root CA bundle. It is supplied as part of each Ioto release, downloaded with checksum verification, and its "as of" date is in its header. `bin/make-certs` generates the test keys and certificates only and never replaces it. To trust a different set of authorities, replace `certs/roots.crt` with your own PEM bundle, or point `tls.authority` at your own file.

## Embedding Ioto

### Embed with Your Own Main

To embed Ioto in your own main program, include `ioto.h`, call `ioStartRuntime` and `ioRun`, and provide `ioStart` and `ioStop` functions.

```c
#include "ioto.h"

int main()
{
    ioStartRuntime(1);

    //  Service requests until told to stop
    ioRun(NULL);

    ioStopRuntime();
    return 0;
}

int ioStart(void)
{
    rInfo("sample", "Hello World\n");
    //  Your code here
    return 0;
}

void ioStop(void) {}
```

Ioto calls your `ioStart` function once initialization is complete. Your `ioStart` must not block. If you need a
long running task, spawn a fiber:

```c
int ioStart(void)
{
    rSpawnFiber("myFiber", (RFiberProc) longRunningTask, NULL);
    return 0;
}
```

### Embed Using the Ioto Main

The second way to integrate is to use the Ioto command program and provide your own **ioStart** and **ioStop**
functions. Ioto's main will handle startup and shutdown; it calls your ioStart when ready.

```c
#include "ioto.h"

PUBLIC int ioStart(void)
{
    rInfo("sample", "Hello World\n");
    //  Your code here
    return 0;
}

PUBLIC void ioStop(void) {}
```

Compile this source and link with the Ioto library as described in
[Linking with the Ioto Library](#linking-with-the-ioto-library) below.

### Linking with the Ioto Library

When compiling your source files, include the Ioto headers:

    $ cc -c -I ioto/include myapp.c

When linking, reference the Ioto static library and the TLS library.

**Linux:**

    $ cc -o myapp myapp.o ioto/build/bin/libioto.a -lssl -lcrypto

**macOS** (requires Homebrew OpenSSL):

    $ brew install openssl
    $ cc -o myapp myapp.o ioto/build/bin/libioto.a -L/opt/homebrew/lib -lssl -lcrypto

**Windows** (Visual Studio): Set `ME_COM_OPENSSL_PATH` to the OpenSSL install directory and link with `ioto.lib`.

If building with MbedTLS instead of OpenSSL, link with the MbedTLS libraries.

### Samples

The [link-agent-main](samples/link-agent-main/README.md) sample demonstrates using the Ioto main and providing
ioStart/ioStop functions.

The [own-main](samples/own-main/README.md) sample demonstrates embedding Ioto with your own main().

See [samples/README.md](samples/README.md) for the full list, including the web server samples.

### Fiber Stacks

Ioto uses [fiber coroutines](https://www.embedthis.com/agent/doc/dev/fiber/) for parallelism instead of threads or
callbacks. This results in a faster and simpler codebase. Each fiber has a stack. On Linux, MacOS and Windows, the
stacks are grown automatically as required from an initial stack size of 32K (64K on Windows). On other platforms
such as FreeRTOS or systems without an MMU, the stack size is fixed. On such platforms, it is recommended that you
limit the use of large stack-based allocations and use heap allocations instead. It is also advised to limit the
use of recursive algorithms.

The initial size of a fiber stack is defined via the **limits.fiberStack** property (previously `limits.stack`) in
the **ioto.json5** configuration file.

For Linux, MacOS and Windows, you can configure the maximum stack size and the stack growth increment via the
properties: **limits.fiberStackMax** and **limits.fiberStackGrow**. If a fiber is reused from the pool, the stack
size can be reset to the initial size if the stack is larger than the initial size via the
**limits.fiberStackReset** property.

```json5
limits: {
    fiberStack: '32k',
    fiberStackMax: '256k',
    fiberStackGrow: '16k',
    fiberStackReset: '64k'
}
```

## Build Profiles

The **profile** property in **ioto.json5** selects runtime configuration, not a build. Two profiles are supplied:

-   dev
-   prod

The **dev** profile uses local directories for state, web site and config files and logs to the console. The **prod** profile uses system standard directories for state, web site and config files.

The **ioto.json5** configuration file has some conditional properties that are applied depending on the selected **profile**. These properties are nested under the **conditional** property and the relevant set are copied to overwrite properties of the same name at the top level. This allows a single configuration file to apply different settings based on the current value of the profile property.

The build configuration is chosen when building, with the **OPTIMIZE** make variable, and is independent of the profile: one `libioto.a` serves every app. A plain `make` builds **release**: optimized, without debug symbols, with `ME_DEBUG=0`. Build **debug** explicitly with `make OPTIMIZE=debug` (or `make.bat debug` on Windows); it is unoptimized, carries symbols and compiles in debug-only code. `make test` builds the debug configuration the suite needs on its own. Both configurations write `build/bin`, so switching from one to the other removes the previous configuration's outputs first.

## Tests

The test suite is located in the `test/` directory and uses the [TestMe](https://www.embedthis.com/testme/) framework.

The test suite requires the following prerequisites:

- **Bun**: v1.2.23 or later
- **TestMe**: Test runner (installed globally)

Install Bun by following the instructions at:

    https://bun.com/docs/installation

Install TestMe globally with:

    bun install -g --trust @embedthis/testme

Run the tests with:

    make test

This builds the debug configuration the suite needs and runs it.

or manually via the `tm` command.

    tm

To run a specific test or group of tests, use the `tm` command with the test name.

    tm basic/

### Other Test Suites

The distribution includes test suites for: tracking memory leaks, fuzz testing and performance benchmarking.

* [Memory Leak Testing](test/web/leak/)
* [Fuzz Testing](test/web/fuzz/)
* [Performance Benchmarking](test/web/bench/)


## Premake Project Generation

Ioto uses [Premake5](https://premake.github.io/) to generate IDE and build system project files. Pre-generated project files are included in the distribution, so **premake5 is not required for building** -- only for regenerating project files after modifying the build configuration.

### Generated Project Locations

Location | Format
-|-
projects/gmake2/ | GNU Makefiles (Linux, macOS, FreeBSD)
projects/vs2022/ | Visual Studio 2022 solution and projects
projects/xcode/ | Xcode workspace and projects
apps/\<APP\>/projects/gmake2/ | Per-app GNU Makefiles
apps/\<APP\>/projects/vs2022/ | Per-app Visual Studio projects
apps/\<APP\>/projects/xcode/ | Per-app Xcode projects

### Regenerating Project Files

To regenerate the core library project files (requires premake5 installed), run from the `projects/` directory:

    $ cd projects
    $ premake5 gmake           # GNU Makefiles
    $ premake5 vs2022          # Visual Studio 2022
    $ premake5 xcode4          # Xcode

For a specific app:

    $ cd apps/http/projects
    $ premake5 gmake

Never edit a generated makefile by hand: the edit survives until the next regeneration and is then silently reverted. The build output formatting is applied by `projects/house-style.lua` during generation, so the committed makefiles are a pure function of the premake5 scripts. To prove they still match:

    $ make verify-projects

### Premake Configuration

The premake5.lua scripts accept these options:

Option | Description
-|-
--tls=openssl\|mbedtls | Select TLS provider (default: openssl)
--openssl-path=PATH | Path to OpenSSL installation
--mbedtls-path=PATH | Path to MbedTLS installation

Example:

    $ cd projects
    $ premake5 --tls=mbedtls --mbedtls-path=/usr/local gmake2

### Build Configurations

The generated makefiles support these configurations via `config=`:

Configuration | Description
-|-
debug_macosx | Debug build for macOS
debug_linux | Debug build for Linux
debug_freebsd | Debug build for FreeBSD
release_macosx | Release build for macOS
release_linux | Release build for Linux
release_freebsd | Release build for FreeBSD

The top-level Makefile auto-detects the platform and maps the `OPTIMIZE` variable to the correct configuration, so you typically don't need to set `config=` directly.

## Directories

| Directory     | Purpose                                                                  |
| :------------ | :----------------------------------------------------------------------- |
| apps          | Sample applications                                                      |
| bin           | Build and utility scripts                                                |
| build         | Build output objects and executables                                     |
| certs         | Test certificates                                                        |
| include       | Public API headers                                                       |
| lib           | Amalgamated module source files to build the Ioto library                |
| projects      | Generated Makefiles and IDE projects                                     |
| src           | Agent source code                                                        |
| test          | Test suites                                                              |

Each app has its own directory under `apps/<APP>/` containing configuration files and a `state/` directory for runtime state, certificates, and web documents.

## App Configuration Files

Each app directory (`apps/<APP>/`) contains these configuration files. Not all files are present in every app.

| File             | Purpose                                                |
| :----------------| :------------------------------------------------------|
| ioto.json5       | Primary Ioto configuration file                        |
| web.json5        | Embedded web server configuration                      |
| device.json5     | Device registration file                               |
| schema.json5     | Database schema                                        |
| signatures.json5 | Web server REST API signatures                         |
| db.json5         | Database configuration                                 |

## AI-Assisted Development

Ioto includes AI-ready project documentation that enables AI coding assistants like [Claude Code](https://claude.com/claude-code) to understand, extend, and enhance your Ioto applications. By leveraging AI, you can rapidly create REST device APIs, user interfaces, database schemas, and complete device management apps.

### AI Project Documentation

The root [CLAUDE.md](CLAUDE.md) gives AI assistants the conventions, APIs and coding patterns they need to generate correct, idiomatic Ioto code. The full API and architecture documentation is at https://www.embedthis.com/doc/.

### Creating REST Device APIs

Use AI to generate REST API endpoints for your device. Describe the device data you want to expose and the AI assistant can create:

- **Web action handlers** in C that respond to REST requests using `webAddAction`
- **API signature files** (`signatures.json5`) that define request/response schemas and role-based access control
- **Database-backed APIs** that use the embedded database to store and retrieve device state

For example, ask your AI assistant to "create a REST API for reading sensor data with user authentication" and it will generate the action handlers, signatures, and database models following Ioto conventions.

### Creating Device Data Schemas

Ioto uses JSON5 schema files to define database models. AI can generate complete schema definitions including:

- **Data models** with typed fields, primary/sort keys, and validation rules
- **TTL expiry** for time-series data like logs and sensor readings
- **Blended schemas** that compose reusable schema parts with app-specific models

Schema files follow the format shown in `apps/http/schema.json5` and are composed from reusable parts under `schemas/parts/` via the `blend` array.

### Creating User Interfaces

AI assistants can generate web-based device management interfaces that integrate with Ioto's embedded web server. This includes:

- **HTML/CSS/JavaScript** pages served from the app's `site/` directory
- **REST client code** that calls your device API endpoints
- **Real-time interfaces** using Server-Sent Events (SSE) or WebSocket streaming
- **Authentication flows** using Ioto's built-in user/role system

### Embedding AI on the Device

Ioto's OpenAI module enables AI capabilities directly on your device. Use AI to build:

- **Chat interfaces** using the Chat Completions or Responses API
- **Agentic workflows** where the cloud LLM invokes device-side functions via callbacks
- **Streaming responses** using SSE for real-time AI output

See the OpenAI module documentation at https://www.embedthis.com/doc/ for the API patterns behind each of these.

## EU Cyber Resilience Act

The open source release of Ioto, published in the public GitHub repository, does not carry CRA conformity documentation. A manufacturer that integrates Ioto into a product placed on the EU market is responsible for that product's conformity with the Cyber Resilience Act (Regulation (EU) 2024/2847).

CRA conformity is offered with the commercial Ioto product. Its source distribution includes the EU Declaration of Conformity, the Annex II user information and a software bill of materials, backed by Embedthis vulnerability handling over a stated support period. Contact [sales@embedthis.com](mailto:sales@embedthis.com).

## Resources

### Online Documentation
-   [EmbedThis web site](http://www.embedthis.com/)
-   [EmbedThis Ioto Documentation](https://www.embedthis.com/doc/)
-   [EmbedThis Ioto Documentation GitHub repository](https://github.com/embedthis/ioto-doc)

### Security and Compliance
-   [doc/release-notes.md](doc/release-notes.md) - Release notes, breaking changes and upgrade steps
-   `doc/user-information.md` - Security properties, secure defaults and the hardening steps to take before a device leaves a trusted network (commercial source distribution only, see [EU Cyber Resilience Act](#eu-cyber-resilience-act))
-   `doc/sbom.json` - Software bill of materials, CycloneDX (commercial source distribution only)

### Module Documentation

Documentation for each module (crypt, db, json, mqtt, openai, osdep, r, uctx, url, web, websock) is in the [Ioto Documentation](https://www.embedthis.com/doc/). The API headers under `include/` document every public function.
