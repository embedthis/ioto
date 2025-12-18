# EmbedThis Ioto Device Agent

<p align="center">
  <a href="https://github.com/embedthis/ioto/actions/workflows/ci.yml"><img
src="https://github.com/embedthis/ioto/actions/workflows/ci.yml/badge.svg"
alt="CI Status"></a>
</p>

The EmbedThis Ioto device agent is an IoT Device Management agent that can be
used for cloud-based management or 
direct management over a local network.

Ioto provides everything you need in an IoT device agent, including: an HTTP
web server, HTTP client, MQTT client,
embedded database and JSON data management. Ioto offers extensive cloud-based
management using the AWS IoT platform.
Integration is provided for cloud-based state persistency, provisioning, cloud
database sync and OTA device upgrading.

What is unique about Ioto is that these components are tightly integrated to
give an extremely efficient and powerful
device agent. Ioto's embedded database will transparently sync device data to
the cloud to enable effortless remote
management.

This distribution includes several [Sample Apps](./apps) to help you get
started with Ioto. You can also download the
separate Ioto Apps package for a suite of device management apps.

See the [README-Cloud](./README-CLOUD.md) for more details about cloud-based
management.
See the [README-ESP32](./README-ESP32.md) for details about building Ioto for
the ESP32 platform.
See the [README-FREERTOS](./README-FREERTOS.md) for details about building Ioto
for the FreeRTOS platform.

Full documentation for the Ioto Agent, the Builder, Device Manager and Ioto
service is available at:

* https://www.embedthis.com/doc/

## Licensing

Ioto is commercial software and is provided under the following licenses.

See [LICENSE.md](LICENSE.md) and [EVAL.md](EVAL.md) for details. It is also
offered with a
[GPLv2](https://www.gnu.org/licenses/gpl-2.0.html) license from the
[Embedthis GitHub Repository](https://github.com/embedthis/ioto).

## Sample Apps

The Ioto distribution build includes several sample apps for local or
cloud-based management. Apps demonstrate
device-side logic to implement various management use cases.

The default app is the "unit" test app which runs the test suite.

Name | Description
-|-
ai | Demonstrates using the OpenAI APIs.
blank | Empty slate application.
blink | Minimal ESP32 blink app to demonstrate linking with Ioto on ESP32
microcontrollers.
demo | Cloud-based management of a device. Demonstrates simple data
synchronization and metrics.
http | Simple embedded web server user/group authentication sample.
unit | Unit testing app.

Each application has a README.md in the apps/APP directory that describes the
application.

## Building from Source

Ioto releases are available as source code distributions from the
[Builder Site](https://admin.embedthis.com/product). To download, first create
an account and login, then navigate
to the product list, select the Ioto Eval and click the download link.

[Download Source Package](https://admin.embedthis.com/product)

The Ioto source distribution contains all the required source files, headers,
and build tools.

Ioto is cross-platform and runs on a wide variety of operating systems and CPU
architectures. EmbedThis offers
different levels of verification for different platforms. The platform support
tiers are:

### Tier 1

Tier 1 platforms are those that are supported and verified by Ioto. Each
release is tested and verified on these
platforms. The tier 1 platforms are:

- Linux
- Mac OS X
- Windows

### Tier 2

Tier 2 platforms are those that are supported by Ioto but each release is not
always tested and verified on these
platforms. The tier 2 platforms are:

- ESP32
- FreeBSD
- FreeRTOS
- VxWorks

### Tier 3

Tier 3 platforms are those that customers have ported Ioto to and may work with
little or no effort by customers.

For tier 3 environments, you will need to cross-compile. The source code has
been designed to run on Arduino, ESP32,
FreeBSD, FreeRTOS, Linux, Mac OS X, VxWorks and other operating systems. Ioto
supports the X86, X64, Riscv, Riscv64,
Arm, Arm64, and other CPU architectures. Ioto can be ported to new platforms,
operating systems and CPU architectures.
Ask us if you need help doing this.

See [Porting
Ioto](https://www.embedthis.com/doc/agent/user/hardware.html#porting-ioto-to-a-new-platform/) for
porting details.

## Build Configuration

When building Ioto, you do not need to use a `configure` program. Instead, you
simply run **make** and select your
desired App. This will copy the App's configuration and conditionally compile
the required services based on the
App's **ioto.json5** configuration settings. During the initial build, the
App's configuration will be copied to
the **state/config** directory.

The selectable Ioto services are:

* ai -- Enable the AI service
* database -- Enable the embedded database
* keys -- Get AWS IAM keys for local AWS API invocation (dedicated clouds only)
* logs -- Capture log files and send to AWS CloudWatch logs (dedicated clouds
only)
* mqtt -- Enable MQTT protocol
* provision -- Dynamically provision keys and certificates for cloud-based
management
* register -- Register with the Ioto Builder
* serialize -- Run a serialization service when making the device
* shadow -- Enable AWS IoT shadow state storage
* sync -- Enable transparent database synchronization with the cloud
* url -- Enable client HTTP request support
* web -- Enable the local embedded web server

## Building

If you are building on Windows, or for ESP32 or FreeRTOS, please read the
specific instructions for various build
environments:

* [Building on Windows](#building-on-windows)
* [Building for the ESP32](README-ESP32.md)
* [Building for FreeRTOS](README-FREERTOS.md)

## Building with Make

To build on Linux, MacOS or Windows via WSL, use the system **make** command.
The supplied Makefile will build the
Ioto library (libioto.a) and the "ioto" main program. If you are embedding Ioto
in another program, you should link
the Ioto library with your program. See [Linking](#linking-with-ioto) below for
details.

To build, nominate your selected app via the "APP=NAME" makefile option. For
example:

    $ make APP=unit

## Building on Windows

For Windows, you can build one of three ways:

* Natively with Windows Visual Studio
* Using Windows Command Prompt and nmake
* Using Windows WSL using make

The easiest way to build is to use Windows WSL and make, but this yields a
lower-performing executable. We recommend
building with WSL initially to test the Ioto agent.

### Building with Windows WSL

Using the [Windows Subsystem for Linux
(WSL)](https://learn.microsoft.com/en-us/windows/wsl/about) you get a tightly
integrated Linux environment from which you can build and debug using VS Code.

To build, first [install
WSL](https://learn.microsoft.com/en-us/windows/wsl/install) by running the
following command
as an administrator:

    $ wsl --install

Then invoke **wsl** to run a wsl (bash) shell:

    $ wsl

To configure WSL for building, install the following packages from the wsl
shell.

    $ apt update
    $ apt install make gcc build-essential libc6-dev openssl libssl-dev

Then extract the Ioto source distribution:

    $ tar xvfz ioto-VERSION.tgz ioto

Finally build via **make**:

    $ cd ioto
    $ make

To debug with VS Code, add the
[WSL
extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-wsl) to VS Code and
then from a WSL terminal, open VS Code from the ioto directory:

    $ cd ioto
    $ code .

This will open a remote WSL project for the Ioto distribution.

### Building Natively on Windows

To build natively on Windows, you will need to install the compiler from the
Visual Studio Community Edition. You
can download the installer from the [Visual Studio
website](https://visualstudio.microsoft.com/downloads/).

You will also need to install `vcpkg` to manage the dependencies and install
the required SSL libraries.

Newer versions of Visual Studio include Vcpkg. You can check if you have Vcpkg
installed by running:

```bash
    > vcpkg --version
```

If installed, you can skip these steps. If you do not have Vcpkg installed, you
can install it by running:

```bash
    > git clone https://github.com/microsoft/vcpkg.git
    > cd vcpkg
    > .\bootstrap-vcpkg.bat
    > .\vcpkg integrate install
```

Once installed, you can install the required Ioto dependencies:

```bash
$ vcpkg install
```

OpenSSL will be installed to:

* -I<vcpkg-root>\installed\x64-windows\include
* -L<vcpkg-root>\installed\x64-windows\lib

Vcpkg is automatically configured for building with Visual Studio. For `nmake`
you will need to set the
ME_COM_OPENSSL_PATH environment variable to the path to the OpenSSL
installation.

### Building with Windows Visual Studio

To build natively with Visual Studio, open the Ioto solution file:

    projects/ioto-windows-default/ioto.sln

For debug, set the "Debug" configuration and build the project. Then select
`ioto` as the startup project. Edit the
`ioto` properties and set the working directory to the `${ProjectDir}\..\..`
which is the Ioto root directory.

You can change the included Ioto demo application by editing the `APP` property
for the `app` project. It is set to
`demo` by default. After changing, do a clean rebuild of the project.

### Building with Nmake and Windows Command Prompt

You can build natively on Windows from the command line. First set the
environment variables for the Visual Studio
version you are using:

```bash
c:\Program Files\Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
```

Next, install the Vcpkg and OpenSSL dependencies as described above in the
[Building Natively](#building-natively) section.

Once the dependencies are installed, define the path to the OpenSSL
installation so the build can find the headers
and libraries by setting the `ME_COM_OPENSSL_PATH` environment variable.

Then build the Ioto project with `make`. This will run the `make.bat` script
which will invoke `nmake` to build the
Ioto project. For example:

```bash
SET ME_COM_OPENSSL_PATH=C:\vcpkg\installed\x64-windows
make
```

## Running the Ioto Command

To run Ioto, type:

    $ make run

or add the directory **build/bin** to your PATH environment variable and invoke
directly.

    export PATH=`make path`

    $ ioto -v

on Windows:

```bash
SET PATH=%PATH%;%CD%\build\bin
ioto -v
```

If your selected app enables the web server, Ioto will listen for connections
on ports 80 for HTTP and 443 for HTTPS
and serve documents from the **site** directory when run with the **dev**
profile. When run with the **prod** profile,
it will serve documents from **/var/www/ioto**. You can change the listening
ports in **web.json5**.

## Running the Web Server

If you only require local management and do not need cloud-based management or
the embedded database, you can run the
web server directly by running the **web** program instead of the **ioto**
program. For example:

    $ web

This will start the web server listening on ports 80 for HTTP and 443 for HTTPS
and serve documents from the
**site** directory.

You can change the listening ports in **web.json5**.

## Changing the TLS Stack

Ioto includes support for two TLS stacks: OpenSSL and MbedTLS. Ioto is built
with OpenSSL by default. OpenSSL is a
leading open source TLS stack that is faster, but bigger. MbedTLS is a compact
TLS implementation. When built for
ESP32, the supplied ESP32 MbedTLS stack will be used.

To build with MbedTLS, install MbedTLS version 3 or later using your O/S
package manager.

For example on Mac OS:

    $ brew install mbedtls

Then build with MbedTLS:

    $ make ME_COM_MBEDTLS=1 ME_COM_OPENSSL=0

## Embedding Ioto

If you wish to embed the Ioto library in your main program, such as you will do
with FreeRTOS, you will need to
include the **ioto.h** header in your source and call Ioto APIs to initialize
and run.

### Including Ioto

In your main program, use:

```c
#include "ioto.h"

int main() {
    // If FreeRTOS, you need to call vTaskStartScheduler() before ioRun

    //  Start the Ioto runtime
    ioStartRuntime(1);

    //  Run Ioto services until instructed to exit
    ioRun(ioStart);

    ioStopRuntime();
}

int ioStart() {
    //  This is invoked by ioRun when Ioto is ready
    //  Put your user code here
}
void ioStop() {
    //  This is invoked by ioRun when Ioto is shutting down
}
```

### Linking with the Ioto Library

To compile, reference the Ioto include directory. For example:

    $ gcc -I/path/to/ioto/build/inc

To link, reference the Ioto library. For example:

    $ cc -o prog -I .../ioto/build/inc -L .../ioto/build/bin main.c -lioto
-lssl -lcrypto

If you are building with OpenSSL or MbedTLS, you will probably need to add
those libraries to the link path.
For example with OpenSSL:

    $ gcc -lcrypto -lssl

### Fiber Stacks

Ioto uses [fiber coroutines](https://www.embedthis.com/agent/doc/dev/fiber/)
for parallelism instead of threads or
callbacks. This results in a faster and simpler codebase. Each fiber has a
stack. On Linux, MacOS and Windows, the
stacks are grown automatically as required from an initial stack size of 32K
(64K on Windows). On other platforms
such as FreeRTOS or systems without an MMU, the stack size is fixed. On such
platforms, it is recommended that you
limit the use of large stack-based allocations and use heap allocations
instead. It is also advised to limit the
use of recursive algorithms.

The initial size of a fiber stack is defined via the **limits.fiberStack**
property (previously `limits.stack`) in
the **ioto.json5** configuration file.

For Linux, MacOS and Windows, you can configure the maximum stack size and the
stack growth increment via the
properties: **limits.fiberStackMax** and **limits.fiberStackGrow**. If a fiber
is reused from the pool, the stack
size can be reset to the initial size if the stack is larger than the initial
size via the
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

You can change Ioto's build and execution **profile** by editing the
**ioto.json5** configuration file. Two build
profiles are supported:

-   dev
-   prod

The **dev** profile will configure Ioto suitable for development. It will use
local directories for state,
web site and config files. It will also define the "optimize" property to be
"debug" which will build Ioto with
debug symbols.

The **prod** profile will build Ioto suitable for production deployment. It
will define system standard directories
for state, web site and config files. It will also define the "optimize"
property to be set to "release" which will
build Ioto optimized without debug symbols.

The **ioto.json5** configuration file has some conditional properties that are
applied depending on the selected
**profile**. These properties are nested under the **conditional** property and
the relevant set are copied to
overwrite properties of the same name at the top level. This allows a single
configuration file to apply different
settings based on the current value of the profile property.

You can override the **"optimize"** property by building with an
"OPTIMIZE=release" or "OPTIMIZE=debug" make
environment variable.

## Tests

The test suite is located in the `test/` directory and uses the
[TestMe](https://www.embedthis.com/testme/) framework.

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

To run a specific test or group of tests, use the `tm` command with the test
name.

    tm basic/

### Other Test Suites

The distribution includes test suites for: tracking memory leaks, fuzz testing
and performance benchmarking.

* [Memory Leak Testing](test/web/leak/)
* [Fuzz Testing](test/web/fuzz/)
* [Performance Benchmarking](test/web/benchmark/)


## Directories

| Directory     | Purpose                                                      
           |
| :------------ |
:----------------------------------------------------------------------- |
| apps          | Directory of application examples                            
           |
| build         | Build output objects and executables                         
           |
| cmds          | Main programs for Ioto and tools                             
           |
| lib           | Source code to build the Ioto library                        
           |
| projects      | Generated Makefiles and IDE projects                         
           |
| scripts       | Device management scripts including OTA script               
           |
| state         | Runtime Ioto state                                           
           |
| state/certs   | Test certificates                                            
           |
| state/config  | Ioto configuration files for device registration, testing and
web server |
| state/site    | Web documents for embedded web server                        
           |
| test          | Test suites                                                  
           |
| tools         | Build tools                                                  
           |

## Key Files

Some of these files may not be present unless required by the built app.

| File             | Purpose                                                |
| :----------------| :------------------------------------------------------|
| LICENSE.md       | License information                                    |
| device.json5     | Device registration file                               |
| display.json5    | UI display configuration for management apps           |
| ioto.json5       | Primary Ioto configuration file                        |
| signature.json5  | Web server REST API signatures                        |
| web.json5        | Embedded web server configuration                      |
| schema.json5     | Database schema                                        |

## Resources

### Online Documentation
-   [EmbedThis web site](http://www.embedthis.com/)
-   [EmbedThis Ioto Documentation](https://www.embedthis.com/doc/)
-   [EmbedThis Ioto Documentation GitHub
repository](https://github.com/embedthis/ioto-doc)

### Project Documentation
-   [AI/designs/DESIGN.md](AI/designs/DESIGN.md) - Architecture and design
overview
-   [AI/context/CONTEXT.md](AI/context/CONTEXT.md) - Module documentation index
-   [AI/references/REFERENCES.md](AI/references/REFERENCES.md) - External
references and links
-   [AI/plans/PLAN.md](AI/plans/PLAN.md) - Development plans

### Module Documentation

Detailed documentation for each module is available in
[AI/context/](AI/context/):

-   [crypt.md](AI/context/crypt.md) - Cryptographic functions
-   [db.md](AI/context/db.md) - Embedded database
-   [json.md](AI/context/json.md) - JSON5/JSON6 parser
-   [mqtt.md](AI/context/mqtt.md) - MQTT client
-   [openai.md](AI/context/openai.md) - OpenAI integration
-   [osdep.md](AI/context/osdep.md) - OS abstraction
-   [r.md](AI/context/r.md) - Safe Runtime foundation
-   [uctx.md](AI/context/uctx.md) - User context/fibers
-   [url.md](AI/context/url.md) - HTTP client
-   [web.md](AI/context/web.md) - Web server
-   [websock.md](AI/context/websock.md) - WebSocket support
