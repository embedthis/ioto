/*
    main.c - Ioto http server example app

    This app demonstrates the use of the Ioto web server with web server authentication
 */
#include "ioto.h"

#define WIFI_SSID       "wifi-ssid"
#define WIFI_PASSWORD   "wifi-password"
#define HOSTNAME        "hostname"

void app_main(void)
{
    /*
        Initialize runtime, filesystem, Wi-Fi and time as needed. Wait for the clock before TLS certificate date
        checks.
     */
    if (ioStartRuntime(IOTO_VERBOSE) < 0) {
        return;
    }
    if (ioStorage("/state", "storage") < 0 ||
            ioWIFI(WIFI_SSID, WIFI_PASSWORD, HOSTNAME) < 0 || ioSetTime(1) < 0) {
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
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */