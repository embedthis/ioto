/**
    esp32.c - Initialization on ESP32 microcontrollers

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

#if ESP32
    #include "esp_sntp.h"

/********************************** Locals ************************************/

#define TRACE_FILTER         "stderr:raw,error,info,!debug:all,!mbedtls"
#define TRACE_VERBOSE_FILTER "stdout:raw,error,info,trace,!debug:all,!mbedtls"
#define TRACE_DEBUG_FILTER   "stdout:all:all,!mbedtls"
#define TRACE_FORMAT         "%A: %M"

/*********************************** Code *************************************/

/*
    Initialize LittleFS file system
    Assumes configuration is in the ./config directory
 */
PUBLIC int ioStorage(cchar *path, cchar *storage)
{
    if (!path || *path != '/') {
        rError("ioto", "Invalid storage path. Must be a string starting with '/'");
        return R_ERR_BAD_ARGS;
    }
    if (!storage || *storage == '\0') {
        rError("ioto", "Invalid partition name");
        return R_ERR_BAD_ARGS;
    }
    if (rInitFlash() < 0) {
        return R_ERR_CANT_COMPLETE;
    }
    if (storage) {
        if (rInitFilesystem(path, storage) < 0) {
            return R_ERR_CANT_OPEN;
        }
    }
    rAddDirectory("state", path);
    return 0;
}

/*
    Initialize WIFI
 */
PUBLIC int ioWIFI(cchar *ssid, cchar *password, cchar *hostname)
{
    if (ssid && password) {
        if (smatch(ssid, "wifi-ssid") || smatch(password, "wifi-password")) {
            rError("ioto", "Must define the WIFI SSID and Password");
            return R_ERR_BAD_ARGS;
        }
        if (rInitWifi(ssid, password, hostname) < 0) {
            return R_ERR_CANT_COMPLETE;
        }
    }
    return 0;
}

PUBLIC int ioSetTime(bool wait)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    if (wait) {
        time_t    now = 0;
        struct tm timeinfo = { 0 };
        while (timeinfo.tm_year < (2023 - 1900)) {
            time(&now);
            localtime_r(&now, &timeinfo);
            rPrintf("Waiting for system time to be set...\n");
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
    return 0;
}

#endif /* ESP32 */
/*
    Copyright (c) EmbedThis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
