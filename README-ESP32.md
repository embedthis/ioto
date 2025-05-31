# Building Ioto on ESP32

This README-ESP32.md covers building Ioto for the ESP32 using the [ESP
IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html).

Please read the [README](./README.md) for general background first.
See the [README-Cloud](./README-CLOUD.md) for more details about cloud-based
management.

Full documentation for the Ioto Agent, the Builder, Device Apps and Ioto
service is available at:

* https://www.embedthis.com/doc/

## Requirements

Ioto on the ESP32 has the following requirements:

* An ESP32 device
* At least 2MB PSIRAM

## Building Ioto for ESP32

This build sequence assumes you have your development environment setup on
Linux or MacOS with the ESP-IDF installed.

First open a terminal and create a directory for your project and the Ioto
component. Choose any name you like for the project.

    mkdir -p myproject
    cd myproject
    mkdir components

Next, add the [ESP
IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html) to your environment.  

    . ~/Path-to-esp-idf/export.sh

### Download Ioto

Navigate to the [Builder](https://admin.embedthis.com/clouds) site and select
`Products` in the sidebar menu and click on the download link for the `Ioto
Evaluation`.  Your first device is free.

<img src="https://www.embedthis.com/images/builder/product-list.avif"
alt="Product List"><br>

Extract the Ioto source code into the components directory. Then rename the
**ioto-VERSION** directory to **ioto** and run the **setup-esp32** script.

    cd components
    tar xvfz ioto-VERSION.tgz
    mv ioto-* ioto
    cd ioto
    bin/setup-esp32
    cd ../..

### Sample apps

The Ioto source distribution includes two ESP32 example apps. Each example
includes the necessary configuration files that are copied from the relevant
app directory into build tree.

Name | Directory | Description
-|-|-
blink | apps/blink | Blink a GPIO LED within the Ioto agent framework
demo | apps/demo | Create a demo counter and synchronize with the local and
cloud databases

The default app is the **blink** app which is ideal to test whether you have
ESP and Ioto successfully installed and configured. You can select an app by
providing an **APP=NAME** option to the make command.

To prepare for building the app and Ioto, invoke **make**:

    make APP=blink

This command will perform the following steps:

* Create the blink app at main/main.c
* Create the app CMakeLists.txt
* Create the file system partitions.csv
* Create the app sdkconfig.defaults
* Initialize the components/ioto for the blink app 
* Create some test certificates that may be required by the app

The **components/ioto/apps** contains master copies of the Ioto demonstration
apps. When you select an app, the code and configuration for the app are copied
to the **./main** and **./state** directories.

## Board Configuration

Next, ensure your ESP target device is defined. For example, to set the target
to **esp32-s3**

    idf.py set-target esp32-s3

The default build configuration is defined via the **sdkconfig.defaults** file.
You can tailor the configuration by running:

    idf.py menuconfig

The Ioto services are enabled via the Ioto menu config option. Navigate to:

    Components config ---> 
    Ioto
    
Then enable the desired Ioto services. This will update the **ioto.json5** and
regenerate the **sdkconfig** and **include/ioto-config.h** files.

Check your **sdkconfig** that the following settings are defined:

Key | Value | Description 
-|-|-
CONFIG_ESP_MAIN_TASK_STACK_SIZE | 8192 | Main task stack size (in words)
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ | 240 | CPU frequency setting to the fastest
setting
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240 | y | CPU frequence alias
CONFIG_ESPTOOLPY_FLASHSIZE | 8MB | Flash memory size
CONFIG_ESPTOOLPY_FLASHSIZE_8MB | y | Flash size alias

The full list is in **components/ioto/apps/NAME/sdkconfig.defaults**.

## Set the WIFI Credentials

Edit the main/main.c and define your WIFI SSID and password. You can optionally
set the hostname as well.

## Registration

When an Ioto enabled device starts, it registers for cloud-based management
with the EmbedThis Builder. To identify your device type, you need to define
the product ID in the **device.json5** config file so your device can be
recognized.

During evalution, it is easiest to register your divice using the pre-existing
**Eval cloud** and **Eval Device App**. The Eval cloud is a multi-tenant,
shared cloud for evaluating Ioto. The Eval Device App is a developer device web
app suitable for examining and monitoring device data. You can use the eval
product token **"01H4R15D3478JD26YDYK408XE6"** when registering your device
with the eval cloud. Later, you can change this to be the token for a Builder
product definition of your own when you wish to use your own device cloud.

```javascript
{
    product: '01H4R15D3478JD26YDYK408XE6',
    name: 'YOUR-PRODUCT-NAME',
    description: 'YOUR-PRODUCT-DESCRIPTION',
    model: 'YOUR-PRODUCT-MODEL'
}
```

## Building with idf.py

Creating the firmware that includes the ESP framework, Ioto and your selected
app is performed via the ESP-IDF **idf.py** command instead of using the normal
Ioto Makefiles which are used when building natively on Linux or MacOS.

To build for ESP32, run:

    idf.py build

## Update the Board Flash 

To update your device with the built application:

    idf.py -p PORT flash

Where PORT is set to the USB/TTY serial port connected to your device.

## Monitoring Output

You can view the ESP32 and Ioto trace via the ESP32 monitor:

    idf.py monitor

When running, the blink and demo apps will turn the LED On/Off every 2 seconds
and trace the LED status to the monitor.

## Local Management

If the selected app enables the embedded web server, files will be served from
the **state/site** directory. The Ioto embedded web server is configured via
the **web.json5** configuration file which is then copied to the **state**
directory.

## Tech Notes

The stack size is configured to be 32K for the main app task and for spawned
fiber tasks. Observationally, the minimum stack for the core Ioto is ~14K.

Ioto uses its own optimized printf implementation which uses less stack (<1K)
and is more secure, being tolerant of errant NULL arguments.

The PlatformIO and Arduino build frameworks are not (yet) supported.

