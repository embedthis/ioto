/*
    main.c - Ioto blink example app

    This is a trivial app to blink a GPIO (2) LED and test that Ioto is built and running correctly. The app sets up WIFI and does connect to the cloud to register the device.
 */
#include "ioto.h"
#include "driver/gpio.h"
#include "rom/gpio.h"
#include "sdkconfig.h"

#define GPIO            2
#define WIFI_SSID       "wifi-ssid"
#define WIFI_PASSWORD   "wifi-password"
#define HOSTNAME        "hostname"

/*
    ESP32 app main
 */
void app_main(void)
{
    /*
        Initialize the runtime and setup the ESP32 file system, WIFI and time daemon.
        If your app does these steps independently, just omit the relevant call here.
     */
    if (ioStartRuntime(IOTO_VERBOSE) < 0) {
        return;
    }
    if (ioStorage("/state", "storage") < 0 || 
            ioWIFI(WIFI_SSID, WIFI_PASSWORD, HOSTNAME) < 0 || ioSetTime(0) < 0) {
        ioStopRuntime();
        return;
    }
    /*
        Run Ioto services and continue until commanded to exit
     */
    ioRun(ioStart);

    ioStopRuntime();
}

/*
    Called when Ioto is fully initialized
 */
int ioStart(void) 
{
    //  Read settings from the ioto.json5 config file
    int delay = svalue(jsonGet(ioto->config, 0, "demo.delay", "2sec")) * TPS;
    int count = jsonGetInt(ioto->config, 0, "demo.count", 30);

    rInfo("blink", "IoStart - ready\n");
    gpio_reset_pin(GPIO);
    gpio_set_direction(GPIO, GPIO_MODE_OUTPUT);

    for (int i = 0, level = 1; i < count; i++) {
        rInfo("blink", "Turn LED %s", level ? "on" : "off");
        gpio_set_level(GPIO, level);
        level = !level;
        rSleep(delay);
    }
    gpio_set_direction(GPIO, 0);
    rInfo("blink", "Demo complete");
    return 0;
}

//  Called when Ioto is shutting down
void ioStop(void)
{
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */