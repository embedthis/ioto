# Building Ioto for FreeRTOS 

This document covers building Ioto with FreeRTOS. Please read the
[README.md](./README.md) for general background first.

## Requirements

Ioto on the FreeRTOS has the following requirements for the target hardware:

* TLS stack
* A flash file system
* 32-bit CPU
* 2MB RAM

The flash file system such as
[LittleFS](https://github.com/littlefs-project/littlefs) is required to store
database and configuration data. 

The instructions below assume you have your development environment setup with
FreeRTOS installed and you have successfully built one of the FreeRTOS demo
applications for your target device.

Ioto requires a TLS stack for secure network communications. Ioto support
MbedTLS and OpenSSL. See [Building TLS](../building/) for details.

Please read [Supported Hardware](https://www.embedthis.com/user/hardware/) for
a complete target hardware list.

## Download Ioto

Navigate to the [Builder](https://admin.embedthis.com/clouds) site and select
`Products` in the sidebar menu and click on the download link for the `Ioto
Evaluation`.  

Extract the Ioto source code into your FreeRTOS app directory and rename the
ioto-VERSION director to **ioto**.

    tar xvfz ioto-VERSION.tgz
    mv ioto-* ioto

## Preparing Ioto Source

Before building your Ioto source code, you need to configure Ioto for your
desired set of services. The Ioto distribution build includes several demo apps
that define a set of services required for that app. The Ioto services are
selected via the **state/config/ioto.json5** configuration file. The services
are:

Service | Description
-|-
database | Enable the embedded database
keys | Get AWS IAM keys for local AWS API invocation (dedicated clouds only)
logs | Capture log files and send to AWS CloudWatch logs (dedicated clouds only)
mqtt | Enable MQTT protocol
provision | Dynamically provision keys and certificates for cloud based
management
register | Register with the Ioto Builder
serialize | Run a serialization service when making the device
shadow | Enable AWS IoT shadow state storage
sync | Enable transparent database synchronization with the cloud
url | Enable client HTTP request support
web | Enable the local embedded web server

The included apps are:

Name | Directory | Description
-|-|-
auth | Simple embedded web server user/group authentication.
blink | Minimal ESP32 blink app to demonstrate linking with Ioto on ESP32
microcontrollers.
demo | Cloud-based management of a device. Demonstrates simple data
synchronization and metrics.

The default app is the **demo** app which sends device data and metrics to the
cloud. 

To select an app, invoke **make config** with your desired APP:

    make APP=demo config

During the first time build, the App's configuration will be copied to the
**state/config** directory. From there, you can copy the config files to your
Flash file system.

If you want a complete sample, please see the [README-ESP32](./README-ESP32.md)
for a demo app that uses FreeRTOS on ESP32 hardware micro-controllers.

## Embedding Ioto in main.c

To invoke the Ioto library from your main program you will need to include the
**ioto.h** header in your source and call Ioto APIs to initialize and run. 

In your main program, use:

```c
#include "ioto.h"

int main() {
    //  Start the Ioto runtime
    ioStartRuntime(1);

    //  Run Ioto services until instructed to exit
    // You need to call vTaskStartScheduler() before ioRun
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

## Building Ioto and FreeRTOS

The Ioto source files are contained in the **ioto/lib** directory which
contains C files and one assembly file. The assembly code contains some high
performace stack management code that is used on Arm, Mips, RiscV and X86
platforms.

To build your FreeRTOS application with Ioto, edit the FreeRTOS app Makefile
and add the following lines. This will cause the Makefile to build the Ioto
source code when compiling FreeRTOS.

```make
// Must be after the OBJ_FILES definition

C_FILES     = $(wildcard ioto/lib/*.c)
A_FILES     = $(wildcard ioto/lib/*.S)
OBJ_FILES   += $(C_FILES:%.c=$(BUILD_DIR)/%.o)
OBJ_FILES   += $(A_FILES:%.S=$(BUILD_DIR)/%.o)
CFLAGS      += -Iioto/include -I/path/to/openssl/include
LDFLAGS     += -L/path/to/openssl/lib -lssl -lcrypto
```

Use the following if you are using MbedTLS:

    CFLAGS  += -I/path/to/mbedtls/include
    LDFLAGS += -L/path/to/mbedtls -lmbedtls -lmbedcrypto -lmbedx509

Then build with **make**

    $ make

## Filesystem

Ioto requires a flash file system with a Posix API to store config and state
files. The file system should be defined with the following directories and
files:

Directory | Description
-|-
state | Runtime state including database, provisioning data and provisioned
certificates
certs | X509 certificates
state/config | Configuration files
state/site | Local web site 

File | Description
-|-
state/config/ioto.json5 | Primary Ioto configuration file
state/config/web.json5 | Local web server configuration file
state/config/device.json5 | Per-device definition
state/certs/roots.crt | Top-level roots certificate
state/certs/aws.crt | AWS root certificate
state/schema.json5 | Database schema

Copy the **ioto/config/** files and the required the **ioto/certs**
certificates and key files to your flash file system.

## Local Management

If the selected app enables the embedded web server, files will be served from
the **site** directory. The Ioto embedded web server is configured via the
**config/web.json5** configuration file which is then copied to the
**config/web.json5** directory.

## Tech Notes

The stack size is configured to be 32K for the main app task and for spawned
fiber tasks. Observationally, the minimum stack for the core Ioto is ~14K.

Ioto uses its own optimized printf implementation which uses less stack (<1K)
and is more secure, being tolerant of errant NULL arguments.

