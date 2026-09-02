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
ai | Demonstrates using the OpenAI APIs.
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
Each app is built as `ioto-NAME` (e.g., `ioto-http`, `ioto-ai`) and placed in `build/bin/`.
If you are embedding Ioto in another program, you should link the Ioto library with your program.
See [Linking](#linking-with-the-ioto-library) below for details.

### Make Targets

Target | Description
-|-
make | Build the library, all utility tools, and all apps (default)
make lib | Build libioto.a and utility tools only
make apps | Build all app binaries (requires library)
make *name* | Shortcut to build a single app (e.g. `make http`, `make ai`)
make run | Build and run the default app (http)
make test | Run the unit test suite
make clean | Remove all build artifacts
make verify-projects | Verify projects/gmake2 matches what premake5.lua generates (requires premake5)
make help | Show available targets and options

### Make Variables

Variable | Description
-|-
OPTIMIZE=debug\|release | Set optimization level (default: debug)
SHOW=1 | Display compiler/linker commands during build
ME_COM_OPENSSL=1 | Build with OpenSSL TLS (default)
ME_COM_MBEDTLS=1 | Build with MbedTLS TLS

### Build Flow

A full `make` (or `make build`) runs these steps in order:

1. **lib** -- Builds `libioto.a`, utility tools (password, db, json, url, web) and `gen-config` using pre-generated premake makefiles under `projects/gmake2/`.
2. **apps** -- Runs each app's `bin/prepare` script to copy and blend configuration files into `apps/<APP>/state/config/`, then builds each app binary (e.g. `ioto-http`, `ioto-ai`) using pre-generated premake makefiles under `apps/<APP>/projects/gmake2/`.

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
    $ ioto-ai -v

On Windows:

```bash
SET PATH=%PATH%;%CD%\build\bin
ioto-http -v
```

Each app binary is named `ioto-NAME` (e.g., `ioto-http`, `ioto-ai`, `ioto-blank`). If the app enables the web server, Ioto will listen for connections on ports 80 for HTTP and 443 for HTTPS and serve documents from the **site** directory when run with the **dev** profile. When run with the **prod** profile, it will serve documents from **/var/www/ioto**. You can change the listening ports in **web.json5**.

## Running the Web Server

If you only need a web server without the database or other Ioto services, you can run the **web** program directly
instead of the **ioto-NAME** app. For example:

    $ web

This will start the web server listening on ports 80 for HTTP and 443 for HTTPS and serve documents from the
**site** directory.

You can change the listening ports in **web.json5**.

## Changing the TLS Stack

Ioto includes support for two TLS stacks: OpenSSL and MbedTLS. Ioto is built with OpenSSL by default. OpenSSL is a
leading open source TLS stack that is faster, but bigger. MbedTLS is a compact TLS implementation. When built for
ESP32, the supplied ESP32 MbedTLS stack will be used.

To build with MbedTLS, install MbedTLS version 3 or later using your O/S package manager.

For example on Mac OS:

    $ brew install mbedtls

Then build with MbedTLS:

    $ make ME_COM_MBEDTLS=1 ME_COM_OPENSSL=0

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

The [link-agent-main](https://github.com/embedthis/ioto/tree/main/samples/agent/link-agent-main/README.md) sample
demonstrates using the Ioto main and providing ioStart/ioStop functions.

The [own-main](https://github.com/embedthis/ioto/tree/main/samples/agent/own-main/README.md) sample demonstrates
embedding Ioto with your own main().

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

You can change Ioto's build and execution **profile** by editing the **ioto.json5** configuration file. Two build
profiles are supported:

-   dev
-   prod

The **dev** profile will configure Ioto suitable for development. It will use local directories for state,
web site and config files. It will also define the "optimize" property to be "debug" which will build Ioto with
debug symbols.

The **prod** profile will build Ioto suitable for production deployment. It will define system standard directories
for state, web site and config files. It will also define the "optimize" property to be set to "release" which will
build Ioto optimized without debug symbols.

The **ioto.json5** configuration file has some conditional properties that are applied depending on the selected
**profile**. These properties are nested under the **conditional** property and the relevant set are copied to
overwrite properties of the same name at the top level. This allows a single configuration file to apply different
settings based on the current value of the profile property.

You can override the **"optimize"** property by building with an "OPTIMIZE=release" or "OPTIMIZE=debug" make
environment variable.

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

or manually via the `tm` command.

    tm

To run a specific test or group of tests, use the `tm` command with the test name.

    tm basic/

### Other Test Suites

The distribution includes test suites for: tracking memory leaks, fuzz testing and performance benchmarking.

* [Memory Leak Testing](test/web/leak/)
* [Fuzz Testing](test/web/fuzz/)
* [Performance Benchmarking](test/web/benchmark/)


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

Ioto includes AI-ready project documentation and skills that enable AI coding assistants like [Claude Code](https://claude.com/claude-code) to understand, extend, and enhance your Ioto applications. By leveraging AI, you can rapidly create REST device APIs, user interfaces, database schemas, and complete device management apps.

### AI Project Documentation

Each Ioto module includes detailed AI context documentation under the [doc/](doc/) directory. This gives AI assistants deep knowledge of the Ioto APIs, architecture, and coding patterns so they can generate correct, idiomatic Ioto code. Key documentation includes:

- [doc/MAP.md](doc/MAP.md) -- AI navigation entry point
- [doc/references/modules/](doc/references/modules/) -- Per-module API guides (web, db, json, mqtt, openai, etc.)
- [doc/references/api-guide.md](doc/references/api-guide.md) -- Quick API reference
- [doc/architecture/system.md](doc/architecture/system.md) -- Architecture and design overview

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

Schema files follow the format shown in `apps/ai/DemoSchema.json5` and are composed via the `blend` array in `schema.json5`.

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
- **Real-time voice/audio** via the WebSocket-based Real-Time API

See the [AI sample app](apps/ai/) for working examples of each pattern, including an agentic workflow that monitors patient temperature and dispatches emergency response.

### AI Skills

The project includes AI skills (under `.claude/skills/`) that teach AI assistants how to work with each Ioto subsystem:

- **getting-started** -- Build embedded IoT applications using the Ioto SDK
- **rest** -- REST endpoints, web action routines, authentication, and WebSocket handlers
- **http** -- Make outbound HTTP requests from the device
- **database** -- Schema design, CRUD operations, expiry, and pagination
- **mqtt** -- Send and receive data via MQTT to a cloud broker
- **runtime** -- Safe Runtime memory management, string functions, fibers, timers, and logging
- **migrate-appweb** -- Migrate Appweb applications to Ioto with API conversion and verification
- **migrate-goahead** -- Migrate GoAhead applications to Ioto with API conversion and verification

These skills encode Ioto API patterns and conventions so AI assistants generate correct, idiomatic code for your device applications.

## Resources

### Online Documentation
-   [EmbedThis web site](http://www.embedthis.com/)
-   [EmbedThis Ioto Documentation](https://www.embedthis.com/doc/)
-   [EmbedThis Ioto Documentation GitHub repository](https://github.com/embedthis/ioto-doc)

### Project Documentation
-   [doc/MAP.md](doc/MAP.md) - AI navigation entry point
-   [doc/architecture/system.md](doc/architecture/system.md) - Architecture and design overview
-   [doc/overview/product.md](doc/overview/product.md) - Product overview
-   [doc/references/external-references.md](doc/references/external-references.md) - External references and links
-   [doc/features/INDEX.md](doc/features/INDEX.md) - Development plans / feature index

### Module Documentation

Detailed documentation for each module is available in [doc/references/modules/](doc/references/modules/):

-   [crypt.md](doc/references/modules/crypt.md) - Cryptographic functions
-   [db.md](doc/references/modules/db.md) - Embedded database
-   [json.md](doc/references/modules/json.md) - JSON5/JSON6 parser
-   [mqtt.md](doc/references/modules/mqtt.md) - MQTT client
-   [openai.md](doc/references/modules/openai.md) - OpenAI integration
-   [osdep.md](doc/references/modules/osdep.md) - OS abstraction
-   [r.md](doc/references/modules/r.md) - Safe Runtime foundation
-   [uctx.md](doc/references/modules/uctx.md) - User context/fibers
-   [url.md](doc/references/modules/url.md) - HTTP client
-   [web.md](doc/references/modules/web.md) - Web server
-   [websock.md](doc/references/modules/websock.md) - WebSocket support
