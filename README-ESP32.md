# Building Ioto on ESP32

This document covers building Ioto for the ESP32 using the [ESP IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html).

Please read the [README](./README.md) for general background first.

Full documentation for the Ioto Agent, the Builder, Device Apps and Ioto service is available at:

* https://www.embedthis.com/doc/

## Requirements

Ioto has been tested on the esp32-s3 and esp32-c6 devices.

Ioto uses the MbedTLS that ESP-IDF bundles, and requires MbedTLS 3.6.7 or later in the 3.6 LTS line. The ESP-IDF releases that bundle it are **v5.2.8**, **v5.3.6** and **v5.5.6** or later on those branches. No v5.4 release bundles it yet, and ESP-IDF v6 bundles MbedTLS 4.x, which Ioto does not support. The apps' **idf_component.yml** files enforce this range.

## Step 1: Install Prerequisites

**Ubuntu/Debian:**
```bash
sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv \
    cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
```

**macOS:**
```bash
brew install cmake ninja dfu-util python3
```

## Step 2: Install ESP-IDF

Clone and install ESP-IDF:

```bash
git clone -b v5.5.6 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32    # or esp32s3, esp32c3, esp32c6, etc.
```

Add ESP-IDF to your environment (run this in each new terminal):

```bash
. ~/esp/esp-idf/export.sh
```

For detailed instructions, see the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html).

## Step 3: Create Project Directory

```bash
mkdir -p myproject/components
cd myproject
```

Source ESP-IDF (if not already done):

```bash
# Note: dotted in, not executed
. ~/esp/esp-idf/export.sh
```

## Step 4: Download Ioto

Navigate to the [Builder](https://admin.embedthis.com/clouds) site, select `Products` in the sidebar, and download the `Ioto Evaluation`.

<img src="https://www.embedthis.com/images/builder/product-list.avif" alt="Product List"><br>

Extract Ioto into the components directory and run setup:

```bash
cd components
tar xvfz ioto-VERSION.tgz
mv ioto-* ioto
cd ioto
bin/setup-esp32
cd ../..
```

## Step 5: Select App

The Ioto source distribution includes ESP32 test apps. Each app includes the necessary configuration files that
are copied from the relevant app directory into the build tree.

Name | Directory | Description
-|-|-
blink | apps/blink | Blink a GPIO LED within the Ioto agent framework
http | apps/http | Simple, local, browser app to test the Ioto web server without cloud services.

You select an app by providing an **APP=NAME** option to the make command:

    make APP=blink

This will perform the following steps:

* Create the app at main/main.c
* Create the app CMakeLists.txt
* Create the file system partitions.csv
* Create the app sdkconfig.defaults
* Initialize the components/ioto for the app
* Install the TLS trust anchors, and for a dev build the published test certificate and key (see [Device Certificates](#device-certificates))

The **components/ioto/apps** contains master copies of the Ioto apps. When you select an app, the code
and configuration for the app are copied to the **./main** and **./state/config** directories.

## Step 6: Board Target

Next, ensure your ESP target device is defined. For example, to set the target to **esp32-s3** or **esp32-c6**:

    idf.py set-target esp32-c6

## Step 7: Board Configuration

Edit your **sdkconfig.defaults** to match your board.

### ESP32-S3 Configuration

Key | Value | Description
-|-|-
CONFIG_ESP_MAIN_TASK_STACK_SIZE | 16384 | Main task stack size (in bytes)
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ | 240 | CPU frequency (fastest for S3)
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240 | y | CPU frequency alias
CONFIG_ESPTOOLPY_FLASHSIZE | 8MB | Flash memory size
CONFIG_ESPTOOLPY_FLASHSIZE_8MB | y | Flash size alias

### ESP32-C6 Configuration

Key | Value | Description
-|-|-
CONFIG_ESP_MAIN_TASK_STACK_SIZE | 16384 | Main task stack size (in bytes)
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ | 160 | CPU frequency (fastest for C6)
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_160 | y | CPU frequency alias

### Required for all boards

Key | Value | Description
-|-|-
CONFIG_MBEDTLS_HAVE_TIME_DATE | y | Check certificate validity dates (ESP-IDF defaults this to n)

With this option off, MbedTLS ignores certificate validity dates, so an expired or not-yet-valid certificate would be accepted. The runtime refuses to build without it. Both shipped apps set it in their **sdkconfig.defaults**; keep it if you replace that file.

Certificate dates are checked against the system clock, which starts at 1970 on boot. An app that makes TLS connections must synchronize the time before its first handshake by calling `ioSetTime(1)`, as the http app's **main/main.c** does. The call waits for SNTP for up to 30 seconds (`IO_TIME_SYNC_TIMEOUT`) and returns an error if the time cannot be set, for example when the device is offline.

The full list of settings is in **components/ioto/apps/NAME/sdkconfig.defaults**.

You can tailor these default settings by running:

    idf.py menuconfig

The Ioto services are enabled via the Ioto menu config option. Navigate to:

    Components config --->
    Ioto

Then enable the desired Ioto services. When you build, these settings will be applied to the Ioto build configuration.
This will update the **state/config/ioto.json5** and **include/config.h** files.

If you later change the sdkconfig.defaults, you can apply the changes by running:

```bash
    idf.py reconfigure
```

## Step 8: Set WiFi Credentials

If your sample uses WiFi, you need to set the WiFi credentials.

Edit the **main/main.c** and update the WiFi credentials near the top of the file:

```c
#define WIFI_SSID       "your-wifi-network"
#define WIFI_PASSWORD   "your-wifi-password"
#define HOSTNAME        "my-device"
```

These are used by the `ioWIFI()` call in `app_main()` to connect to your network.

## Step 9: Build

Building the ESP framework, Ioto and your selected app is performed via the ESP-IDF **idf.py** command instead of using 
the normal Ioto Makefiles which are used when building natively on Linux or macOS.

To build for ESP32, run:

    make APP=NAME build

This invokes `idf.py build` internally.

### Device Certificates

The whole **state/** directory, including **state/certs/**, is packed into the storage partition and flashed to the device. What goes into **state/certs/** depends on the build profile, which is the app's `profile` setting in **ioto.json5** unless you override it with `make PROFILE=NAME`.

* **dev**: installs the published test certificate and key, `certs/test.crt` and `certs/test.key`. That key is public, so anyone can impersonate a device that uses it. Use it for development only and never ship it.
* **Any other profile** (for example **prod**): installs no test certificate or key. You must provision a per-device certificate and key in **state/certs/** and name them in the profile's `tls` settings in the app's **ioto.json5**, for example `conditional.profile.prod.tls: {certificate: '@certs/device.crt', key: '@certs/device.key'}`. The build stops with a message if they are missing or are the published test pair.

The shipped apps name the test pair only in their `dev` profile, so a non-dev configuration never uses it by accident.

## Step 10: Flash the Device

To update your device board with the built application:

    idf.py -p PORT flash

Where PORT is set to the USB/TTY serial port connected to your device. It will be something like /dev/tty.usbmodem101.

After flashing, the device will reboot and start the selected app.

## Step 11: Monitor Output

You can view the ESP32 and Ioto trace via the ESP32 monitor:

    idf.py monitor

When running, the blink app will turn the LED On/Off every 2 seconds and trace the LED status to the monitor.

## Local Management

If the selected app enables the embedded web server, files will be served from the **state/site** directory. 
The Ioto embedded web server is configured via the **state/config/web.json5** configuration file.

## Cloud Management (Add-on Plugin)

Cloud-based device management is available as a separate add-on plugin. Contact [EmbedThis](https://www.embedthis.com/) for information about cloud management for ESP32 devices.

## Project Structure

After building, your project directory should look like this:

```
myproject/
├── CMakeLists.txt
├── Makefile
├── main/
│   └── main.c                    # Your application code
├── state/                        # Flashed to the storage partition
│   ├── certs/                    # Trust anchors; the test pair only for dev builds
│   └── config/                   # Runtime configuration
│       ├── ioto.json5
│       ├── web.json5
│       └── device.json5
└── components/
    └── ioto/
        ├── lib/                  # Ioto source files
        ├── include/              # Ioto headers
        ├── apps/                 # App templates (blink, http)
        └── certs/                # Trust anchors and the published development-only test pair
```

## Tech Notes

Two stack sizes matter, and they are separate settings. `CONFIG_ESP_MAIN_TASK_STACK_SIZE` sizes the ESP-IDF main task, in bytes, and is set to 16384 by the supplied apps. Spawned fibers get `ME_FIBER_DEFAULT_STACK`, which defaults to 16K on ESP32 and is set at runtime by `limits.fiberStack` in **ioto.json5**. Observationally, the minimum stack for the core Ioto is ~14K, so neither should be lowered below 16K.

The supplied apps set `CONFIG_COMPILER_STACK_CHECK_MODE_STRONG=y`, so the whole firmware, including ESP-IDF, lwIP and MbedTLS, is compiled with `-fstack-protector-strong` and a smashed stack frame stops the device instead of running on. The canaries cost a few bytes in each protected frame. Keep the setting if you replace **sdkconfig.defaults**.

Each fiber is a FreeRTOS task. Its stack, task control block and three semaphores come from internal RAM, which WiFi and lwIP also need, so a fiber costs roughly 17K with the default 16K stack. The agent caps the number of fibers at the smaller of `web.limits.connections` (in **web.json5**) and `CONFIG_LWIP_MAX_SOCKETS`, plus 8 for its own background work, and logs the ceiling and the per-fiber cost at startup. With the ESP-IDF default of 10 sockets the ceiling is 18 fibers, about 300K at the ceiling. Before raising `CONFIG_LWIP_MAX_SOCKETS`, check that the free internal heap (`heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`) covers the logged total with room to spare. Setting `limits.fibers` in **ioto.json5** replaces the derived ceiling.

Ioto uses its own optimized printf implementation which uses less stack (<1K) and is more secure, being tolerant of 
errant NULL arguments.

The PlatformIO and Arduino build frameworks are not (yet) supported.
