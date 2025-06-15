# EmbedThis Ioto Device Agent

The EmbedThis Ioto device agent is an IoT Device Management agent that can be
used for cloud-based management or direct management over a local network. 

Ioto provides everything you need in an IoT device agent, including: a HTTP web
server, HTTP client, MQTT client, embedded database and JSON data management.
Ioto offers extensive cloud-based management using the AWS IoT platform.
Integration is provided for cloud-based state persistency, provisioning, cloud
database sync and OTA device upgrading.

What is unique about Ioto, is that these components are tightly integrated to
give an extremely efficient and powerful device agent. Ioto's embedded database
will transparently sync device data to the cloud to enable effortless remote
management.

This distribution includes several [Sample Apps](./apps) to help you get
started with Ioto. You can also download the separate Ioto Sample Apps package
for a suite of device sample apps. 

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

See [LICENSE.md](LICENSE.md) and [EVAL.md](EVAL.md) for details.

## Sample Apps

The Ioto distribution build includes several sample apps for local or
cloud-based management. Apps demonstrate device-side logic to implement various
management use cases.

The default app is the "demo" app which sends device data and metrics to the
cloud.

Name | Description
-|-|-
ai | Demonstrate using the OpenAI APIs.
auth | Simple embedded web server user/group authentication sample.
blank | Empty slate application. 
blink | Minimal ESP32 blink app to demonstrate linking with Ioto on ESP32
microcontrollers.
demo | Cloud-based management of a device. Demonstrates simple data
synchronization and metrics.
unit | Unit testing app.

Each application has a README.md in the apps/APP directory that describes the
application.

## Building from Source

Ioto releases are available as source code distributions from the [Builder
Site](https://admin.embedthis.com/product). To download, first create an
account and login, then navigate to the product list, select the Ioto Eval and
click the download link.

[Download Source Package](https://admin.embedthis.com/product)

The Ioto source distribution contains all the required source files, headers,
and build tools. 

Several build environments are supported:

-   **Linux** &mdash; Linux 4 with GNU C/C++ including Raspberry PI
-   **Mac OS X** &mdash; Mac OS X 11 or later
-   **Windows** &mdash; Windows 11 with WSL v2
-   **ESP-32** &mdash; Using the [ESP
IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html)

For other environments, you will need to cross-compile. The source code has
been designed to run on Arduino, ESP32, FreeBSD, FreeRTOS, Linux, Mac OS X,
VxWorks and other operating systems. Ioto supports the X86, X64, Riscv,
Riscv64, Arm, Arm64, and other CPU architecutres. Ioto can be ported to new
plaforms, operating systems and CPU architectures. Ask us if you need help
doing this.

## Build Configuration

When building Ioto, you do not need to use a `configure` program. Instead, you
simply run **make** and select your desired App. This will copy the App's
configuration and conditionally compile the required services based on the
App's **ioto.json5** configuration settings. During the first time build, the
App's configuration will be copied to the **state/config** directory.

You can provide parameters to the build via makefile command variables. See
below for details.

The selectable Ioto services are:

* ai -- Enable the AI service
* database -- Enable the embedded database
* keys -- Get AWS IAM keys for local AWS API invocation (dedicated clouds only)
* logs -- Capture log files and send to AWS CloudWatch logs (dedicated clouds
only)
* mqtt -- Enable MQTT protocol
* provision -- Dynamically provision keys and certificates for cloud based
management
* register -- Register with the Ioto Builder
* serialize -- Run a serialization service when making the device
* shadow -- Enable AWS IoT shadow state storage
* sync -- Enable transparent database synchronization with the cloud
* url -- Enable client HTTP request support
* web -- Enable the local embedded web server

## Building

If you are building on Windows, or for ESP32 or FreeRTOS, please read the
specific instructions for various build environments:

* [Building on Windows](#building-on-windows)
* [Building for the ESP32](README-ESP32.md) 
* [Building for FREERTOS](README-FREERTOS.md)

Read below for generic build instructions.

## Building with Make

To build on Linux, MacOS or Windows via WSL, use the system **make** command.
The supplied Makefile will build the Ioto library (libioto.a) and the "ioto"
main program. If you are embedding Ioto in another program, you should link the
Ioto library with your program. See [Linking](#linking-with-ioto) below for
details. 

The top level Makefile parses your **ioto.json5** configuration file, detects
your operating system and CPU architecture and then invokes the appropriate
project Makefile for your system. It copies the select app configuration from
the **apps/NAME/config** directory to the **state** directory.

To build, nominate your selected app via the "APP=NAME" makefile option:

    $ make APP=demo

## Building on Windows

Building on Windows utilizes the [Windows SubSystem for Linux
(WSL)](https://learn.microsoft.com/en-us/windows/wsl/about). Using WSL, you get
a tightly integrated Linux environment from which you can build and debug using
VS Code.

To build, first [install
WSL](https://learn.microsoft.com/en-us/windows/wsl/install) by running the
following command as an administrator:

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

To debug with VS Code, add the [WSL
extentension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-wsl) to VS Code and then from a WSL terminal, open VS Code from the ioto directory:

    $ cd ioto
    $ code .

This will open a remote WSL project for the Ioto distribution.


## Running the Ioto Command

To run Ioto, type:

    $ make run

or add the directory **build/bin** to your PATH environment variable and invoke
directly.

    export PATH=`make path`

    $ ioto -v

If your selected app enables the web server, Ioto will listen for connections
on ports 80 for HTTP and 443 for HTTPS and serve documents from the **site**
directory when run with the **dev** profile. When run with the **prod**
profile, it will serve documents from **/var/www/ioto**. You can change the
listening ports in **web.json5**.

## Changing the TLS Stack

Ioto includes support for two TLS stacks: OpenSSL and MbedTLS. Ioto is built
with OpenSSL by default. OpenSSL is a leading open source TLS stack that is
faster, but bigger. MbedTLS is a compact TLS implementation. When built for
ESP32, the supplied ESP32 MbedTLS will be used.

To build with MbedTLS, install MbedTLS version 3 or later using your O/S
package manager. 

For example on Mac OS:

    $ brew install mbedtls

Then build with MbedTLS:

    $ make ME_COM_MBEDTLS=1 ME_COM_OPENSSL=0

## Embedding Ioto

If you wish to embed the Ioto library in your main program, such as you will do
with FreeRTOS, you will need to include the **ioto.h** header in your source
and call Ioto APIs to initialize and run. 

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

int iotStart() {
    //  This is invoked by ioRun when Ioto is ready
    //  Put your user code here
}
void iotStop() {
    //  This is invoked by ioRun when Ioto is shutting down
}
```

### Linking with the Ioto Library

To compile, reference the Ioto include directory. For example:

    $ gcc -I/path/to/ioto/build/inc

To link, reference the Ioto library

For example:

    $ cc -o prog -I .../ioto/build/inc -L .../ioto/build/bin main.c -lioto
-lssl -lcrypto 

If you are building with OpenSSL or MbedTLS, you will probably need to add
those libraries to the link path. For example with OpenSSL:

    $ gcc -lcrypto -lssl

### Debugging

To build Ioto with additional memory and stack debug checks, enable the
**ME_FIBER_CHECK_STACK** compilation variable. This will cause the Ioto runtime
to check stack integrity and track overall stack and fiber usage. This does
have a performance impact and should only be used in debug builds.

    DFLAGS=-DME_FIBER_CHECK_STACK=1 make clean build

### Stack Size

Ioto uses [fiber coroutines](https://www.embedthis.com/agent/doc/dev/fiber/)
for parallelism instead of threads or callbacks. The size of fiber stacks is
defined via the **limits.stack** property in the **ioto.json5** configuration
file. Set this value to be sufficient for your application needs. 

In general, it is recommended that you avoid the use of large stack-based
allocations and use heap allocations instead. It is also advised to limit the
use of recursive algorithms. Ioto uses its own optimized printf implementation
which uses less stack (<1K) and is more secure, being tolerant of errant NULL
arguments.

A default stack of 32K should be sufficient for core Ioto use on 32-bit
systems. For 64-bit systems, 64K should be a minimum. If you are running a
debugger such as MacOS XCode, you may need a much bigger stack. Your
application also may have greater requirements.

## Build Profiles

You can change Ioto's build and execution **profile** by editing the
**ioto.json5** configuration file. Two build profiles are supported:

-   dev
-   prod

The **dev** profile will configure Ioto suitable for developement. It will
define use local directories for state, web site and config files. It will also
define the "optimize" property to be "debug" which will build Ioto with debug
symbols.

The **prod** profile will build Ioto suitable for production deployment. It
will define system standard directories for state, web site and config files.
It will also define the "optimize" property to be set to "release" which will
build Ioto optimized without debug symbols. 

The **ioto.json5** configuration file has some conditional properties that are
applied depending on the selected **profile**. These properties are nested
under the **conditional** property and the relevant set are copied to overwrite
properties of the same name at the top level. This allows a single
configuration file to apply different settings based on the current value of
the profile property.

You can override the **"optimize"** property by building with a
"OPTIMIZE=release" or "OPTIMIZE=debug" make environment variable.

## Directories

| Directory     | Purpose                                                      
           |
| :------------ |
:----------------------------------------------------------------------- |
| apps          | Directory of applications examples                           
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
| state         | Runtime Ioto provisioning state and                          
           |
| state/certs   | Test certificates                                            
           |
| state/config  | Ioto configuration files for device registration, testing and
web server |
| state/site    | Web documents for embedded web server                        
           |
| tools         | Build tools                                                  
           |

## Key Files

Some of these files may not be present unless the built app requires.

| File             | Purpose                                                |
| :----------------| :------------------------------------------------------|
| LICENSE.md       | License information                                    |
| device.json5     | Device registration file                               |
| display.json5    | UI display configuration for management apps           |
| ioto.json5       | Primary Ioto configuration file                        |
| signature.json5  | Web server REST API signnatures                        |
| web.json5        | Embedded web server configuration                      |
| schema.json5     | Database schema                                        |

## Resources

-   [EmbedThis web site](http://www.embedthis.com/)
-   [EmbedThis Ioto Documentation GitHub
repository](https://github.com/embedthis/ioto-doc)

