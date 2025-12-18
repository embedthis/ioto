# Building Ioto for FreeRTOS 

This document covers building Ioto with FreeRTOS. Please read the
[README.md](./README.md) for general background first.

## Requirements

Ioto on FreeRTOS has the following requirements for the target hardware:

* TLS stack
* A flash file system
* 32-bit CPU
* 2MB RAM

The flash file system such as
[LittleFS](https://github.com/littlefs-project/littlefs) is required to store
database 
and configuration data. 

The instructions below assume you have your development environment set up with
FreeRTOS installed and you have
successfully built one of the FreeRTOS demo applications for your target device.

Ioto requires a TLS stack for secure network communications. Ioto supports
MbedTLS and OpenSSL.

Please read [Supported Hardware](https://www.embedthis.com/user/hardware/) for
a complete target hardware list.

## Step 1: Getting FreeRTOS

Clone the FreeRTOS repository with submodules:

```bash
git clone --recurse-submodules https://github.com/FreeRTOS/FreeRTOS.git
```

Verify your toolchain by building a demo application:

```bash
cd FreeRTOS/FreeRTOS/Demo/Posix_GCC
make
```

This builds the POSIX/GCC demo which runs on Linux and macOS.

## Step 2: Create Your Application Directory

This demo runs FreeRTOS on Linux and macOS.

```bash
cd FreeRTOS/FreeRTOS/Demo
mkdir App
cd App
```

## Step 3: Download and Extract Ioto

Navigate to the [Builder](https://admin.embedthis.com/clouds) site, select
`Products` in the sidebar, and download the 
`Ioto Evaluation`.

Copy the demo app into your application directory:

```bash
tar xvfz /path/to/ioto-VERSION.tgz
mv ioto-* ioto
cp -r ioto/apps/demo/freertos/* .
```

These are the essential files for the Ioto demo:
- `Makefile` — Build configuration
- `FreeRTOSConfig.h` — FreeRTOS kernel configuration
- `main.c` — Ioto demo app


## Step 4: Configure Ioto Library

Configure the Ioto library for the demo:

```bash
make -C ioto APP=demo
```

This copies configuration to `ioto/state/config/` and builds the Ioto library.

## Step 5: Build the App

```bash
make
```

## Step 6: Run the App

```bash
./build/demo
```

## Project Structure

After setup, your directory should look like:

```
App/
├── Makefile                 # Modified for Ioto
├── FreeRTOSConfig.h         # FreeRTOS configuration
├── main.c                   # Your Ioto main program
└── ioto/
    ├── lib/                 # C source files (rLib.c, webLib.c, etc.)
    ├── include/             # Headers (ioto.h, r.h, json.h, etc.)
    ├── apps/                # App templates (demo, auth, blank)
    ├── certs/               # Test certificates
    └── state/config/        # Runtime configuration
```

## Ioto Services Reference

Services enabled via **state/config/ioto.json5**:

| Service | Description |
|---------|-------------|
| database | Embedded database |
| keys | AWS IAM keys for local API invocation |
| logs | CloudWatch log capture |
| mqtt | MQTT protocol |
| provision | Dynamic certificate provisioning |
| register | Builder registration |
| serialize | Device serialization service |
| shadow | AWS IoT shadow state |
| sync | Database synchronization with cloud |
| url | HTTP client requests |
| web | Embedded web server |

For a complete ESP32 example, see [README-ESP32](./README-ESP32.md).

## Filesystem

Ioto requires a flash file system with a Posix API to store config and state
files. In this demo, the host file
system is used.

Directory | Description
-|-
state | Runtime state including database, provisioning data and provisioned
certificates
certs | X509 certificates
state/config | Configuration files
state/site | Local website 

File | Description
-|-
state/config/ioto.json5 | Primary Ioto configuration file
state/config/web.json5 | Local web server configuration file
state/config/device.json5 | Per-device definition
state/certs/roots.crt | Top-level roots certificate
state/certs/aws.crt | AWS root certificate
state/schema.json5 | Database schema

Copy the **ioto/state/config/** files and the required **ioto/certs**
certificates and key files to your flash file system.

## Local Management

If the selected app enables the embedded web server, files will be served from
the **site** directory. The Ioto 
embedded web server is configured via the **state/config/web.json5**
configuration file.

## Tech Notes

The stack size is configured to be 32K for the main app task and for spawned
fiber tasks.

Ioto uses its own optimized printf implementation which uses less stack (<1K)
and is more secure, being tolerant of 
errant NULL arguments.
