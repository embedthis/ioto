/*
    main.c -- Main for demo app on ESP32

    This app demonstrates Ioto data synchronization to the cloud
 */
#include "ioto.h"

#define WIFI_SSID       "wifi-ssid"
#define WIFI_PASSWORD   "wifi-password"
#define HOSTNAME        "hostname"

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
        Run Ioto services and the demoApp.c via ioStart() and continue until commanded to exit
     */
    ioRun(ioStart);

    ioStopRuntime();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
