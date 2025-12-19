/*
 * R Runtime Library Source
 */

#include "r.h"

#if ME_COM_R



/********* Start of file src/r.c ************/

/*
    r.c - Portable Runtime

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/*********************************** Globals **********************************/

PUBLIC char *rCopyright = "Copyright (c) Michael O'Brien. All Rights Reserved.";
PUBLIC int  rState = R_STARTED;

/*********************************** Locals ***********************************/

static char *rAppName = ME_NAME;

/************************************* Code ***********************************/
/*
    Create and initialize the R service
 */
PUBLIC int rInit(RFiberProc fn, cvoid *arg)
{
    int rc;

    srand((uint) rGetHiResTicks());
    rc = rInitOs();
#if R_USE_FILE
    if (rc == 0) rc = rInitFile();
#endif
#if R_USE_LOG
    if (rc == 0) rc = rInitLog();
#endif
#if R_USE_THREAD
    if (rc == 0) rc = rInitThread();
#endif
#if R_USE_EVENT
    if (rc == 0) rc = rInitEvents();
#endif
#if R_USE_FIBER
    if (rc == 0) rc = rInitFibers();
#endif
#if R_USE_WAIT
    if (rc == 0) rc = rInitWait();
#endif
#if ME_COM_SSL && R_USE_TLS
    if (rc == 0) rc = rInitTls();
#endif
#if R_USE_FIBER
    if (rc == 0) {
        rSetState(R_INITIALIZED);
        if (fn) {
            return rSpawnFiber("init-main", fn, (void*) arg);
        }
    }
#endif
    return rc;
}

PUBLIC void rTerm(void)
{
#if ME_COM_SSL && R_USE_TLS
    rTermTls();
#endif
#if R_USE_WAIT
    rTermWait();
#endif
#if R_USE_LOG
    rTermLog();
#endif
#if R_USE_FILE
    rTermFile();
#endif
#if R_USE_FIBER
    rTermFibers();
#endif
#if R_USE_EVENT
    rTermEvents();
#endif
    rTermOs();
}

PUBLIC cchar *rGetAppName(void)
{
    return rAppName;
}

PUBLIC void rGracefulStop(void)
{
    rSetState(R_STOPPING);
}

PUBLIC void rStop(void)
{
    rSetState(R_STOPPED);
}

PUBLIC int rGetState(void)
{
    return rState;
}

/*
    Async thread safe
 */
PUBLIC void rSetState(int state)
{
    rState = state;
#if R_USE_WAIT
    if (state >= R_STOPPING) {
        rWakeup();
    }
#endif
}

#if ME_UNIX_LIKE
PUBLIC int rDaemonize(void)
{
    pid_t pid;

    if ((pid = fork()) < 0) {
        rError("run", "Fork failed for background operation");
        return R_ERR_CANT_COMPLETE;

    } else if (pid == 0) {
        setsid();
        rWritePid();
        return 0;
    }
    exit(0);
}

PUBLIC int rWritePid(void)
{
#if ME_UNIX_LIKE
    char  *buf, pidbuf[64], *path;
    pid_t pid;

    if (getuid() == 0) {
        path = "/var/run/" ME_NAME ".pid";
        if ((buf = rReadFile(path, NULL)) != 0) {
            //  SECURITY Acceptable: acceptable risk reading pid file
            pid = atoi(buf);
            if (kill(pid, 0) == 0) {
                rError("app", "Already running as PID %d", pid);
                rFree(buf);
                return R_ERR_ALREADY_EXISTS;
            }
            rFree(buf);
        }
        sfmtbuf(pidbuf, sizeof(pidbuf), "%d\n", getpid());
        if (rWriteFile(path, pidbuf, slen(pidbuf), 0600) < 0) {
            rError("app", "Could not create pid file %s", path);
            return R_ERR_CANT_OPEN;
        }
    } else {
        return R_ERR_CANT_WRITE;
    }
    return 0;
#else
    rError("app", "PID file not supported on this platform");
    return R_ERR_BAD_STATE;
#endif
}
#endif

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/buf.c ************/

/**
    buf.c - Dynamic buffer module

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



#if R_USE_BUF
/*********************************** Locals ***********************************/

#ifndef ME_R_MAX_BUF
    #define ME_R_MAX_BUF (8 * 1024 * 1024)
#endif

#define BUF_MIN_GROW     64

/*********************************** Forwards *********************************/

static size_t roundBufSize(size_t size);

/************************************ Code ************************************/

PUBLIC int rInitBuf(RBuf *bp, size_t size)
{
    if (!bp || size <= 0) {
        return R_ERR_BAD_ARGS;
    }
    if (size > SIZE_MAX) {
        rAllocException(R_MEM_FAIL, size);
        return R_ERR_MEMORY;
    }
    memset(bp, 0, sizeof(*bp));
    if ((bp->buf = rAlloc(size)) == 0) {
        return R_ERR_MEMORY;
    }
    memset(bp->buf, 0, size);
    bp->buflen = size;
    bp->endbuf = &bp->buf[bp->buflen];
    bp->start = bp->buf;
    bp->end = bp->buf;
    *bp->start = '\0';
    return 0;
}

PUBLIC void rTermBuf(RBuf *bp)
{
    if (bp && bp->buf) {
        rFree(bp->buf);
        bp->buf = 0;
    }
}

PUBLIC RBuf *rAllocBuf(size_t initialSize)
{
    RBuf *bp;

    if (initialSize > INT_MAX) {
        rAllocException(R_MEM_FAIL, initialSize);
        return NULL;
    }
    if (initialSize <= 0) {
        initialSize = ME_BUFSIZE;
    }
    if ((bp = rAllocType(RBuf)) == 0) {
        return 0;
    }
    if (rInitBuf(bp, initialSize) < 0) {
        rFree(bp);
        return 0;
    }
    return bp;
}

PUBLIC void rFreeBuf(RBuf *bp)
{
    if (bp) {
        rTermBuf(bp);
        rFree(bp);
    }
}

/*
    Round up buffer size to power-of-2.
    Minimum size of BUF_MIN_GROW bytes to avoid frequent small reallocations.
 */
static size_t roundBufSize(size_t size)
{
    if (size < BUF_MIN_GROW) {
        size = BUF_MIN_GROW;
    }
    // Round up to next power of 2
    size--;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
#if SIZE_MAX > 0xFFFFFFFF
    size |= size >> 32;
#endif
    size++;
    return size;
}

/*
    Grow the buffer to a specific size. Return 0 if successful.
    Does not shrink buffers.
 */
PUBLIC int rGrowBufSize(RBuf *bp, size_t size)
{
    char   *newbuf;
    size_t newSize;

    if (size <= 0 || size > ME_R_MAX_BUF) {
        return R_ERR_BAD_ARGS;
    }
    if (size <= bp->buflen) {
        return 0;
    }
    newSize = roundBufSize(size);
    if (newSize > ME_R_MAX_BUF) {
        return R_ERR_MEMORY;
    }
    if (bp->start > bp->buf) {
        rCompactBuf(bp);
    }
    if ((newbuf = rAlloc(newSize)) == 0) {
        return R_ERR_MEMORY;
    }
    if (bp->buf) {
        memcpy(newbuf, bp->buf, bp->buflen);
    }
    memset(&newbuf[bp->buflen], 0, newSize - bp->buflen);
    bp->end = newbuf + (bp->end - bp->buf);
    bp->start = newbuf + (bp->start - bp->buf);
    bp->buflen = newSize;
    bp->endbuf = &newbuf[bp->buflen];

    rFree(bp->buf);
    bp->buf = newbuf;
    return 0;
}

/*
    Grow the buffer by a specified amount. Return 0 if the buffer grows.
 */
PUBLIC int rGrowBuf(RBuf *bp, size_t need)
{
    size_t newSize;

    if (need <= 0 || need > ME_R_MAX_BUF) {
        return R_ERR_BAD_ARGS;
    }
    if (need > MAXSSIZE - bp->buflen) {
        return R_ERR_MEMORY;
    }
    newSize = bp->buflen + need;
    return rGrowBufSize(bp, newSize);
}

PUBLIC int rReserveBufSpace(RBuf *bp, size_t need)
{
    if (rGetBufSpace(bp) >= need) {
        return 0;
    }
    if (rGrowBuf(bp, need) < 0) {
        return R_ERR_MEMORY;
    }
    return 0;
}

/*
    This appends a silent null. It does not count as one of the actual bytes in the buffer
 */
PUBLIC void rAddNullToBuf(RBuf *bp)
{
    size_t space;

    if (!bp) {
        return;
    }
    space = (size_t) (bp->endbuf - bp->end);
    if (space < sizeof(char)) {
        if (rGrowBuf(bp, 1) < 0) {
            if (bp->end > bp->start) {
                bp->end--;
            } else {
                return;
            }
        }
    }
    if (bp->end < bp->endbuf) {
        *((char*) bp->end) = (char) '\0';
    }
}

PUBLIC void rAdjustBufEnd(RBuf *bp, ssize size)
{
    char *end;

    if (!bp) {
        return;
    }
    end = bp->end + size;
    if (end < bp->start || end > bp->endbuf) {
        return;
    }
    bp->end = end;
}

/*
    Adjust the star pointer after a user copy. Note: size can be negative.
 */
PUBLIC void rAdjustBufStart(RBuf *bp, ssize size)
{
    if (!bp || size < 0 || (bp->start + size > bp->end)) {
        return;
    }
    bp->start += size;
    if (bp->start > bp->end) {
        bp->start = bp->end;
    }
    if (bp->start <= bp->buf) {
        bp->start = bp->buf;
    }
}

PUBLIC void rFlushBuf(RBuf *bp)
{
    if (bp) {
        bp->start = bp->buf;
        bp->end = bp->buf;
        if (bp->buflen > 0) {
            bp->start[0] = bp->start[bp->buflen - 1] = '\0';
        }
    }
}

PUBLIC int rGetCharFromBuf(RBuf *bp)
{
    if (!bp || bp->start == bp->end) {
        return -1;
    }
    return (uchar) * bp->start++;
}

PUBLIC ssize rGetBlockFromBuf(RBuf *bp, char *buf, size_t size)
{
    size_t thisLen, bytesRead;

    if (!bp || !buf || (size > SIZE_MAX - 8)) {
        return R_ERR_BAD_ARGS;
    }
    if (bp->buflen != (size_t) (bp->endbuf - bp->buf)) {
        return R_ERR_BAD_STATE;
    }

    /*
        Get the max bytes in a straight copy
     */
    bytesRead = 0;
    while (size > 0) {
        thisLen = rGetBufLength(bp);
        thisLen = min(thisLen, size);
        if (thisLen <= 0) {
            break;
        }

        memcpy(buf, bp->start, thisLen);
        buf += thisLen;
        bp->start += thisLen;
        size -= thisLen;
        bytesRead += thisLen;
    }
    return (ssize) bytesRead;
}

#ifndef rGetBufLength
PUBLIC size_t rGetBufLength(RBuf *bp)
{
    return bp ? bp->end - bp->start : 0;
}
#endif

#ifndef rGetBufSize
PUBLIC size_t rGetBufSize(RBuf *bp)
{
    return bp ? bp->buflen : 0;
}
#endif

#ifndef rGetBufSpace
PUBLIC size_t rGetBufSpace(RBuf *bp)
{
    return bp ? bp->endbuf - bp->end : 0;
}
#endif

#ifndef rGetBuf
PUBLIC char *rGetBuf(RBuf *bp)
{
    return bp ? bp->buf : 0;
}
#endif

#ifndef rGetBufStart
PUBLIC cchar *rGetBufStart(RBuf *bp)
{
    return bp ? bp->start : 0;
}
#endif

#ifndef rGetBufEnd
PUBLIC cchar *rGetBufEnd(RBuf *bp)
{
    return bp ? (char*) bp->end : 0;
}
#endif

PUBLIC int rInserCharToBuf(RBuf *bp, int c)
{
    if (!bp) {
        return R_ERR_BAD_ARGS;
    }
    if (bp->start == bp->buf) {
        return R_ERR_BAD_STATE;
    }
    *--bp->start = c;
    return 0;
}

PUBLIC int rLookAtNextCharInBuf(RBuf *bp)
{
    if (!bp || bp->start == bp->end) {
        return -1;
    }
    return *bp->start;
}

PUBLIC int rLookAtLastCharInBuf(RBuf *bp)
{
    if (!bp || bp->start == bp->end) {
        return -1;
    }
    return bp->end[-1];
}

PUBLIC int rPutCharToBuf(RBuf *bp, int c)
{
    char   *cp;
    size_t space;

    if (!bp) {
        return R_ERR_BAD_ARGS;
    }
    assert(bp->buflen == (size_t) (bp->endbuf - bp->buf));
    space = rGetBufSpace(bp);
    if (space < sizeof(char)) {
        if (rGrowBuf(bp, 1) < 0) {
            return R_ERR_MEMORY;
        }
    }
    cp = (char*) bp->end;
    *cp++ = (char) c;
    bp->end = (char*) cp;

    if (bp->end < bp->endbuf) {
        *((char*) bp->end) = (char) '\0';
    }
    return 0;
}

/*
    Return the number of bytes written to the buffer. If no more bytes will fit, may return less than size.
    Never returns < 0.
 */
PUBLIC ssize rPutBlockToBuf(RBuf *bp, cchar *str, size_t size)
{
    size_t thisLen, bytes, space;

    if (!bp || !str || size < 0 || size > MAXINT) {
        return R_ERR_BAD_ARGS;
    }
    bytes = 0;
    while (size > 0) {
        space = rGetBufSpace(bp);
        thisLen = min(space, size);
        if (thisLen <= 0) {
            if (rGrowBuf(bp, size) < 0) {
                break;
            }
            space = rGetBufSpace(bp);
            thisLen = min(space, size);
        }
        memcpy(bp->end, str, thisLen);
        str += thisLen;
        bp->end += thisLen;
        size -= thisLen;
        bytes += thisLen;
    }
    if (bp && bp->end < bp->endbuf) {
        *((char*) bp->end) = (char) '\0';
    }
    return (ssize) bytes;
}

PUBLIC ssize rPutStringToBuf(RBuf *bp, cchar *str)
{
    if (str) {
        return rPutBlockToBuf(bp, str, slen(str));
    }
    return 0;
}

PUBLIC ssize rPutSubToBuf(RBuf *bp, cchar *str, size_t count)
{
    size_t len;

    if (str) {
        len = slen(str);
        len = min(len, count);
        if (len > 0) {
            return rPutBlockToBuf(bp, str, len);
        }
    }
    return 0;
}

PUBLIC ssize rPutToBuf(RBuf *bp, cchar *fmt, ...)
{
    va_list ap;
    char    *s;
    ssize   count;

    if (fmt == 0) {
        return 0;
    }
    va_start(ap, fmt);
    s = sfmtv(fmt, ap);
    va_end(ap);
    count = rPutStringToBuf(bp, s);
    rFree(s);
    return count;
}

/*
    Add a number to the buffer (always null terminated).
 */
PUBLIC ssize rPutIntToBuf(RBuf *bp, int64 i)
{
    ssize rc;
    char  num[32];

    rc = rPutStringToBuf(bp, sitosbuf(num, sizeof(num), i, 10));
    if (bp->end < bp->endbuf) {
        *((char*) bp->end) = (char) '\0';
    }
    return rc;
}

PUBLIC void rCompactBuf(RBuf *bp)
{
    if (rGetBufLength(bp) == 0) {
        rFlushBuf(bp);
        return;
    }
    if (bp->start > bp->buf) {
        memmove(bp->buf, bp->start, (size_t) (bp->end - bp->start));
        bp->end -= (bp->start - bp->buf);
        bp->start = bp->buf;
    }
}

PUBLIC void rResetBufIfEmpty(RBuf *bp)
{
    if (rGetBufLength(bp) == 0) {
        rFlushBuf(bp);
    }
}

PUBLIC cchar *rBufToString(RBuf *bp)
{
    rAddNullToBuf(bp);
    return rGetBufStart(bp);
}

/*
    Transfers ownership of the buffer contents to the caller.
    The buffer is freed and the user must not reference it again.
 */
PUBLIC char *rBufToStringAndFree(RBuf *bp)
{
    char *s;

    if (!bp) {
        return NULL;
    }
    rAddNullToBuf(bp);
    if (rGetBufLength(bp) > 0) {
        rCompactBuf(bp);
    }
    s = bp->buf;
    bp->buf = 0;
    rFree(bp);
    // SECURITY: Acceptable - transfer ownership of the buffer to the caller
    return s;
}

#endif /* R_USE_BUF */
/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/esp32.c ************/

/**
    esp32.c - ESP32 specific adaptions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



#if ESP32
/********************************** Locals ************************************/

#define ETAG           ME_NAME
#define WIFI_MAX_RETRY 5

#define WIFI_SUCCESS   0x1
#define WIFI_FAILURE   0x2

static char *wifiIP;
static EventGroupHandle_t      wifiEvent;
static esp_netif_t             *sta_netif;
static esp_vfs_littlefs_conf_t fsconf;

/********************************** Forwards **********************************/

#ifndef R_USE_PLATFORM_REPORT
    #define R_USE_PLATFORM_REPORT 0
#endif

#if R_USE_TLS
static void customTls(RSocket *sp, int cmd, void *arg, int flags);
#endif

/*********************************** Code *************************************/

PUBLIC int rInitOs(void)
{
#if R_USE_TLS
    /*
        Register a custom TLS callback to define the MbedTLS certificate bundle
     */
    rSetSocketCustom(customTls);
#endif
    return 0;
}

PUBLIC void rTermOs(void)
{
    if (fsconf.partition_label) {
        esp_vfs_littlefs_unregister(fsconf.partition_label);
    }
    rFree(wifiIP);
    wifiIP = NULL;
}

#if R_USE_TLS
static void customTls(RSocket *sp, int cmd, void *arg, int flags)
{
    if (cmd == R_SOCKET_CONFIG_TLS) {
        if (!(flags & R_TLS_HAS_AUTHORITY)) {
            /*
                Attach the MbedTLS certificate bundle
             */
            mbedtls_ssl_config *conf = (mbedtls_ssl_config*) arg;
            if (esp_crt_bundle_attach(conf) != ESP_OK) {
                rError(ETAG, "Failed to attach certificate bundle");
            }
        }
    }
}
#endif

/*
    Initialize the LittleFS file system from "storage" to the nominated path
 */
PUBLIC int rInitFilesystem(cchar *path, cchar *storage)
{
    esp_err_t ret;

    fsconf.base_path = path;
    fsconf.partition_label = storage;
    fsconf.format_if_mount_failed = true;
    fsconf.dont_mount = false;

    ret = esp_vfs_littlefs_register(&fsconf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            rError(ETAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            rError(ETAG, "Failed to find LittleFS partition");
        } else {
            rError(ETAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return R_ERR_CANT_INITIALIZE;
    }
#if SHOW_USAGE
    size_t total, used;
    total = used = 0;
    ret = esp_littlefs_info(fsconf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        rError(ETAG, "Failed to get LittleFS partition (%s)", esp_err_to_name(ret));
        return R_ERR_CANT_INITIALIZE;
    }
    rInfo(ETAG, "FS size: total: %d, used: %d\n", total, used);
#endif
    return 0;
}

/*
    Initialize NVM flash
 */
PUBLIC int rInitFlash(void)
{
    esp_err_t rc;

    rc = nvs_flash_init();
    if (rc == ESP_ERR_NVS_NO_FREE_PAGES || rc == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        rc = nvs_flash_init();
        if (rc != ESP_OK) {
            return R_ERR_CANT_INITIALIZE;
        }
    }
    return 0;
}

/*
    WIFI handler progress callback
 */
static void wifiHandler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    static int wifiRetries = 0;

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *dp = (wifi_event_sta_disconnected_t*) event_data;
        rError(ETAG, "WIFI connection error for ssid %s, reason %d\n", dp->ssid, (int) dp->reason);
        if (wifiRetries < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            wifiRetries++;
            rInfo(ETAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(wifiEvent, WIFI_FAILURE);
            rError(ETAG, "WIFI connect failed");
        }

    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
        rFree(wifiIP);
        wifiIP = sfmt(IPSTR, IP2STR(&event->ip_info.ip));
        wifiRetries = 0;
        xEventGroupSetBits(wifiEvent, WIFI_SUCCESS);
    }
}

PUBLIC cchar *rGetIP(void)
{
    return wifiIP;
}

/*
    Initialize WIFI networking
 */
PUBLIC int rInitWifi(cchar *ssid, cchar *password, cchar *hostname)
{
    EventBits_t                  bits;
    wifi_config_t                config = { 0 };
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_netif_set_hostname(sta_netif, hostname));

    wifiEvent = xEventGroupCreate();
    wifi_init_config_t icfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&icfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiHandler, NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiHandler, NULL,
                                                        &instance_got_ip));

    strlcpy((char*) config.sta.ssid, ssid, sizeof(config.sta.ssid));
    strlcpy((char*) config.sta.password, password, sizeof(config.sta.password));
    config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());

    bits = xEventGroupWaitBits(wifiEvent, WIFI_SUCCESS | WIFI_FAILURE, pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_SUCCESS) {
        rInfo(ETAG, "WIFI connected with SSID:%s", ssid);
    } else if (bits & WIFI_FAILURE) {
        rInfo(ETAG, "Failed to connect to SSID:%s", ssid);
    } else {
        rInfo(ETAG, "Unexpected WIFI error %x", (uint) bits);
    }
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
    vEventGroupDelete(wifiEvent);
    return 0;
}

#if R_USE_PLATFORM_REPORT
/*
    Just for debug to trace memory usage
 */
PUBLIC void rPlatformReport(char *label)
{
    static char reportBuf[1024];
    char        *base;
    int         hiw, stackSize;
    ptrdiff_t   current;

    //  GetStackHighWaterMark  is the minimum stack that was available in the past in words
    hiw = (int) uxTaskGetStackHighWaterMark(NULL) * sizeof(int);
    stackSize = (int) rGetFiberStackSize();
    base = (char*) rGetFiberStack();
    current = base - (char*) &base;

    size_t intern = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t free = esp_get_free_heap_size();
    size_t total = heap_caps_get_total_size(MALLOC_CAP_8BIT);

    vTaskList(reportBuf);

    rPrintf("\n%s\nTask List:\n%s", label, reportBuf);
    rPrintf("Free internal: %d bytes\n", intern);
    rPrintf("Free heap size: %d of %d bytes\n", free, total);
    rPrintf("Stack current %d, max %d, size %d\n\n", current, stackSize - hiw, stackSize);
}
#endif

PUBLIC int gethostname(char *name, size_t namelen)
{
    cchar *buf;

    if (sta_netif && esp_netif_get_hostname(sta_netif, &buf) == 0) {
        scopy(name, namelen, buf);
        return 0;
    }
    return -1;
}


#else
void freeEspDummy(void)
{
}
#endif /* ESP32 */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

/********* Start of file src/event.c ************/

/*
    event.c - Event Loop Service

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



#if R_USE_EVENT
/*********************************** Locals ***********************************/

typedef struct Event {
    RFiber *fiber;
    REventProc proc;
    void *arg;
    struct Event *next;
    Ticks when;
    REvent id;
    int fast;
} Event;

/*
    Event queue. Note: events are not stored in list order
 */
static Event *events = 0;

/*
    Event lock so rStartEvent can be thread safe
 */
static RLock eventLock;

/*
    Event names to watch
 */
typedef struct Watch {
    RWatchProc proc;
    cvoid *data;
} Watch;

static RHash *watches;

/********************************** Forwards **********************************/

static void freeEvent(Event *ep);
static REvent getNextID(void);
static void linkEvent(Event *ep);
static Event *lookupEvent(REvent id, Event **priorp);

/************************************ Code ************************************/

PUBLIC int rInitEvents(void)
{
    events = 0;
    watches = rAllocHash(0, R_TEMPORAL_NAME | R_STATIC_VALUE);
    if (!watches) {
        return R_ERR_MEMORY;
    }
    rInitLock(&eventLock);
    return 0;
}

PUBLIC void rTermEvents(void)
{
    Event *ep, *np;
    Watch *watch;
    RList *list;
    RName *name;
    uint  next;

    for (ep = events; ep; ep = np) {
        np = ep->next;
        freeEvent(ep);
    }
    for (ITERATE_NAME_DATA(watches, name, list)) {
        for (ITERATE_ITEMS(list, watch, next)) {
            rFree(watch);
        }
        rFreeList(list);
    }
    rFreeHash(watches);
    rTermLock(&eventLock);
    watches = 0;
    events = 0;
}

/*
    Allocate an event. Events are run and respect the order of scheduling.
    If a fiber is supplied, the proc is run on that fiber. Otherwise a new fiber is allocated
    to run the proc. This routine is THREAD SAFE and is the only safe way to interact with R
    services from foreign threads. Returns an event ID that may be used with rStopEvent to
    deschedule and event if it has not already run.
 */
PUBLIC REvent rAllocEvent(RFiber *fiber, REventProc proc, void *arg, Ticks delay, int flags)
{
    Event *ep;
    Ticks now;

    if ((ep = rAlloc(sizeof(Event))) == 0) {
        return 0;
    }
    if (proc) {
        assert(!fiber);
        ep->proc = proc;
    } else if (!fiber) {
        //  This path must be called from within Ioto
        fiber = rGetFiber();
    }
    ep->arg = arg;
    now = rGetTicks();
    if (delay > 0 && now > MAXINT64 - delay) {
        ep->when = MAXINT64;
    } else {
        ep->when = now + delay;
    }
    ep->id = getNextID();
    ep->fiber = fiber;
    ep->fast = (!fiber && flags & R_EVENT_FAST) ? 1 : 0;
    linkEvent(ep);
    rWakeup();
    return ep->id;
}

static void freeEvent(Event *ep)
{
    if (ep->fiber) {
        rFreeFiber(ep->fiber);
        ep->fiber = 0;
    }
    rFree(ep);
}

PUBLIC REvent rStartEvent(REventProc proc, void *arg, Ticks delay)
{
    return rAllocEvent(NULL, proc, arg, delay, 0);
}

PUBLIC REvent rStartFastEvent(REventProc proc, void *arg, Ticks delay)
{
    return rAllocEvent(NULL, proc, arg, delay, R_EVENT_FAST);
}

PUBLIC int rStopEvent(REvent id)
{
    Event *ep, *prior;

    if (id == 0) {
        return R_ERR_CANT_FIND;
    }
    rLock(&eventLock);
    if ((ep = lookupEvent(id, &prior)) != 0) {
        if (ep == events) {
            events = ep->next;
        } else if (prior) {
            prior->next = ep->next;
        }
        rUnlock(&eventLock);
        freeEvent(ep);
        return 0;
    }
    rUnlock(&eventLock);
    return R_ERR_CANT_FIND;
}

PUBLIC int rRunEvent(REvent id)
{
    Event *ep;

    rLock(&eventLock);
    if ((ep = lookupEvent(id, NULL)) != 0) {
        ep->when = rGetTicks();
        rUnlock(&eventLock);
        rWakeup();
        return 0;
    }
    rUnlock(&eventLock);
    return R_ERR_CANT_FIND;
}

PUBLIC int rServiceEvents(void)
{
    while (rState < R_STOPPING) {
        rWait(rRunEvents());
    }
    if (rState == R_RESTART) {
        rInfo("runtime", "Restarting...");
    }
    return rState;
}

PUBLIC bool rLookupEvent(REvent id)
{
    Event *ep;

    rLock(&eventLock);
    ep = lookupEvent(id, NULL);
    rUnlock(&eventLock);
    return ep ? 1 : 0;
}

PUBLIC Ticks rRunEvents(void)
{
    Event      *ep, *next, *prior;
    Event      *dueList, *dueTail;
    Ticks      now, deadline;
    REventProc proc;
    RFiber     *fiber;
    void       *arg;

    assert(rIsMain());
    now = rGetTicks();
    deadline = MAXINT64;

    /*
        Build a list of due events while holding the lock
     */
    rLock(&eventLock);
    dueList = NULL;
    dueTail = NULL;
    prior = NULL;

    for (ep = events; ep; ep = next) {
        next = ep->next;
        if (ep->when <= now && rState < R_STOPPING) {
            //  Unlink from main list
            if (ep == events) {
                events = ep->next;
            } else if (prior) {
                prior->next = ep->next;
            }
            //  Add to due list
            ep->next = NULL;
            if (dueTail) {
                dueTail->next = ep;
                dueTail = ep;
            } else {
                dueList = dueTail = ep;
            }
        } else {
            deadline = min(ep->when, deadline);
            prior = ep;
        }
    }
    rUnlock(&eventLock);

    /*
        Execute due events without holding lock
     */
    for (ep = dueList; ep; ep = next) {
        next = ep->next;
        arg = ep->arg;

        if (ep->fast) {
            assert(!ep->fiber);
            proc = ep->proc;
            freeEvent(ep);
            (proc) (arg);
        } else {
            fiber = ep->fiber;
            if (!fiber) {
                fiber = rAllocFiber(NULL, (RFiberProc) ep->proc, arg);
                if (!fiber) {
                    // Put back event until we have a fiber to run it on
                    ep->when = rGetTicks() + 1;
                    linkEvent(ep);
                    continue;
                }
            }
            ep->fiber = 0;
            freeEvent(ep);
            rResumeFiber(fiber, arg);
        }
    }

    return deadline;
}

PUBLIC Time rGetNextDueEvent(void)
{
    Ticks when;

    if (rState >= R_STOPPING) {
        return 0;
    }
    rLock(&eventLock);
    when = events ? events->when : MAXINT64;
    rUnlock(&eventLock);
    return when;
}

/*
    Event IDs are 64 bits and should never wrap in our lifetime, but we do handle wrapping just incase. In that case,
       events with IDs starting from 1 should have long since run.
    Regardless, we check for collisions with existing events.
    Event ID == 0 is invalid.
 */
static REvent getNextID(void)
{
    static REvent nextID = 1;
    REvent        id;
    int           attempts = 0;

    rLock(&eventLock);
    //  Will always find an ID on an embedded system, but prevent infinite loop in pathological case
    while (rLookupEvent(nextID) && attempts++ < 10000) {
        nextID++;
        if (nextID >= MAXINT64) {
            nextID = 1;
        }
    }
    id = nextID++;
    rUnlock(&eventLock);
    return id;
}

static Event *lookupEvent(REvent id, Event **priorp)
{
    Event *ep, *prior;

    for (prior = 0, ep = events; ep; ep = ep->next) {
        if (ep->id == id) {
            if (priorp) {
                *priorp = prior;
            }
            return ep;
        }
        prior = ep;
    }
    return 0;
}

/*
    THREAD SAFE
    Note: do an in-order insertion. This inserts after the last event of the same time.
 */
static void linkEvent(Event *event)
{
    Event *ep, *prior;

    rLock(&eventLock);
    if (events) {
        prior = 0;
        for (ep = events; ep; ep = ep->next) {
            if (ep->when > event->when) {
                if (ep == events) {
                    // Insert at the head
                    event->next = events;
                    events = event;
                } else {
                    event->next = prior->next;
                    prior->next = event;
                }
                break;
            }
            prior = ep;
        }
        // If loop completed without break, append at end
        if (ep == NULL) {
            prior->next = event;
            event->next = NULL;
        }
    } else {
        // Add to the head
        event->next = events;
        events = event;
    }
    rUnlock(&eventLock);
}

PUBLIC void rWatch(cchar *name, RWatchProc proc, void *data)
{
    Watch *watch;
    RList *list;
    uint  next;

    if ((list = rLookupName(watches, name)) == 0) {
        list = rAllocList(0, 0);
        if (!list) {
            return;
        }
        if (!rAddName(watches, name, list, 0)) {
            rFreeList(list);
            return;
        }
    } else {
        //  Check for duplicates
        for (ITERATE_ITEMS(list, watch, next)) {
            if (watch->proc == proc && watch->data == data) {
                return;
            }
        }
    }
#ifndef __clang_analyzer__
    watch = rAllocType(Watch);
    watch->proc = proc;
    watch->data = data;
    rPushItem(list, watch);
#endif
}

PUBLIC void rWatchOff(cchar *name, RWatchProc proc, void *data)
{
    RList *list;
    Watch *watch;
    uint  next;

    if ((list = rLookupName(watches, name)) != 0) {
        for (ITERATE_ITEMS(list, watch, next)) {
            if (watch->proc == proc && watch->data == data) {
                rRemoveItemAt(list, (int) next);
#ifndef __clang_analyzer__
                // Clang analyzer incorrectly complains about this
                rFree(watch);
#endif
                break;
            }
        }
    }
}

static void signalFiber(Watch *watch)
{
    watch->proc(watch->data, NULL);
}

/*
    Signal watchers of an event
    This spawns a fiber for each watcher and is difficult to reliably take an argument that needs freeing.
 */
PUBLIC void rSignal(cchar *name)
{
    RList *list;
    Watch *watch;
    uint  next;

    if ((list = rLookupName(watches, name)) != 0) {
        for (ITERATE_ITEMS(list, watch, next)) {
            rStartEvent((RFiberProc) signalFiber, watch, 0);
        }
    }
}

/*
    Invoke watchers synchronously
 */
PUBLIC void rSignalSync(cchar *name, cvoid *arg)
{
    RList *list;
    Watch *watch;
    uint  next;

    if ((list = rLookupName(watches, name)) != 0) {
        for (ITERATE_ITEMS(list, watch, next)) {
            watch->proc(watch->data, arg);
        }
    }
}

#endif /* R_USE_EVENT */
/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/fiber.c ************/

/**
    fiber.c - Fiber coroutines

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



#if R_USE_FIBER
/*********************************** Locals ***********************************/

static RFiber mainFiberState;
static RFiber *mainFiber;
static RFiber *currentFiber;

static size_t fiberInitialStack = ME_FIBER_DEFAULT_STACK;

#if ME_FIBER_GROWABLE_STACK
// Runtime-configurable initial stack size (single source of truth)
static size_t fiberMaxStack = ME_FIBER_MAX_STACK;
static size_t fiberStackGrowSize = ME_FIBER_STACK_GROW_SIZE;
static size_t fiberStackResetLimit = ME_FIBER_STACK_RESET_LIMIT;
#endif

/*
    Fiber pool for reusing fiber allocations
 */
typedef struct FiberPool {
    RFiber *free;         // Head of free list
    int active;           // Current active fiber count
    int peak;             // High water mark
    int max;              // Max fibers (0 = unlimited)
    int pooled;           // Current pooled count
    int poolMax;          // Maximum pool size
    int poolMin;          // Minimum pool size (prune target)
    uint64 poolHits;      // Acquisitions from pool
    uint64 poolMisses;    // Allocations from heap
    REvent pruneEvent;    // Periodic pruning timer
} FiberPool;

static FiberPool fiberPool = { 0 };

/*********************************** Forwards *********************************/

static RFiber *acquireFromPool(void);
static RFiber *allocNewFiber(void);
static int initFiberContext(RFiber *fiber, RFiberProc function, cvoid *data);
static bool releaseToPool(RFiber *fiber);
static void freeFiberMemory(RFiber *fiber);
static void pruneFibers(void *data);
static void setupFiberSignalHandlers(void);

#if ME_FIBER_GROWABLE_STACK
static int allocGuardedStack(RFiberStack *info, size_t initialSize, size_t maxSize);
static void freeGuardedStack(RFiberStack *info);
static void resetGuardedStack(RFiberStack *info);
static int growFiberStack(RFiber *fiber);
#endif

/************************************ Code ************************************/

PUBLIC int rInitFibers(void)
{
    uctx_t *context;
    int    base;

    mainFiber = &mainFiberState;
    currentFiber = mainFiber;
    context = &mainFiber->context;

    fiberPool.poolMin = ME_FIBER_POOL_MIN;
    fiberPool.poolMax = ME_FIBER_POOL_LIMIT;
    fiberPool.pruneEvent = rStartEvent(pruneFibers, NULL, ME_FIBER_PRUNE_INTERVAL);

    if (uctx_init(NULL) < 0) {
        rError("runtime", "Cannot initialize UCTX subsystem");
        return R_ERR_CANT_ALLOCATE;
    }
    // Main fiber uses OS-managed thread stack - this just sets uctx bounds
    if (uctx_setstack(context, ((char*) &base) + 64 - ME_FIBER_DEFAULT_STACK, ME_FIBER_DEFAULT_STACK) < 0) {
        rError("runtime", "Cannot set main fiber stack, size %d", (int) ME_FIBER_DEFAULT_STACK);
        return R_ERR_CANT_ALLOCATE;
    }
#if ME_WIN_LIKE || ESP32 || FREERTOS
    if (uctx_makecontext(context, NULL, 0) < 0) {
        rError("runtime", "Cannot allocate main fiber context");
        return R_ERR_CANT_ALLOCATE;
    }
#endif
    setupFiberSignalHandlers();
    return 0;
}

PUBLIC void rTermFibers(void)
{
    RFiber *fiber, *next;

    if (fiberPool.pruneEvent) {
        rStopEvent(fiberPool.pruneEvent);
        fiberPool.pruneEvent = 0;
    }
    for (fiber = fiberPool.free; fiber; fiber = next) {
        next = fiber->next;
        freeFiberMemory(fiber);
    }
    fiberPool.free = NULL;
    fiberPool.pooled = 0;

    uctx_freecontext(&mainFiber->context);
    uctx_term();

    mainFiber = NULL;
    currentFiber = NULL;
}

/*
    Top level fiber entry point.
    Loops to accept new work when fiber is reused from pool.
    Reads func/data from fiber struct - set by initFiberContext.
 */
static void fiberEntry(RFiber *fiber)
{
    currentFiber = fiber;

    while (fiber->func) {
        fiber->func(fiber->data);
        //  Signal fiber is ready for possible reuse and yield back to main
        fiber->pooled = 1;
        rYieldFiber(0);
        //  Resumed - loop continues if fiber->func is set, exits if NULL
        fiber->pooled = 0;
    }
    //  For pthreads, the thread exits here. For native contexts, we never reach here.
}

PUBLIC RFiber *rAllocFiber(cchar *name, RFiberProc function, cvoid *data)
{
    RFiber *fiber;

    // Check fiber limit
    if (fiberPool.max && fiberPool.active >= fiberPool.max) {
        rDebug("fiber", "Exceeded fiber limit %d", fiberPool.max);
#if ME_FIBER_HARD_FAIL
        rAllocException(R_MEM_STACK, (size_t) fiberPool.max);
#endif
        return NULL;
    }
    fiberPool.active++;
    if (fiberPool.active > fiberPool.peak) {
        rDebug("fiber", "Peak fibers %d", fiberPool.active);
        fiberPool.peak = fiberPool.active;
    }
    fiber = acquireFromPool();
    if (!fiber) {
        fiber = allocNewFiber();
        if (!fiber) {
            fiberPool.active--;
            return NULL;
        }
    }
    if (initFiberContext(fiber, function, data) < 0) {
        freeFiberMemory(fiber);
        fiberPool.active--;
        return NULL;
    }
    return fiber;
}

void rFreeFiber(RFiber *fiber)
{
    assert(fiber);
    fiberPool.active--;

    if (!releaseToPool(fiber)) {
        freeFiberMemory(fiber);
    }
}

/*
    Allocate a new fiber from the heap
 */
static RFiber *allocNewFiber(void)
{
    RFiber *fiber;
    size_t size;

    fiberPool.poolMisses++;

    size = sizeof(RFiber);
#if ME_FIBER_GROWABLE_STACK
    // Allocate fiber structure (stack allocated separately with guard pages)
    if ((fiber = rAllocMem(size)) == 0) {
        rAllocException(R_MEM_FAIL, size);
        return NULL;
    }
    memset(fiber, 0, size);
    if (uctx_needstack()) {
        // Allocate guarded stack using runtime-configurable limits
        if (allocGuardedStack(&fiber->stackInfo, fiberInitialStack, fiberMaxStack) < 0) {
            rFree(fiber);
            rAllocException(R_MEM_STACK, fiberInitialStack);
            return NULL;
        }
    }
#elif ME_FIBER_VM_STACK
    // Stack is allocated separately via VM allocation (mmap)
    if ((fiber = rAllocMem(size)) == 0) {
        rAllocException(R_MEM_FAIL, size);
        return NULL;
    }
    memset(fiber, 0, size);
    if (uctx_needstack()) {
        fiber->stack = rAllocVirt(fiberInitialStack);
        if (!fiber->stack) {
            rFree(fiber);
            rAllocException(R_MEM_STACK, fiberInitialStack);
            return NULL;
        }
    }
#else
    // Stack is allocated inline with fiber struct
    if (uctx_needstack()) {
        size += fiberInitialStack;
        if (size > MAXINT) {
            rAllocException(R_MEM_STACK, size);
            return NULL;
        }
    }
    if ((fiber = rAllocMem(size)) == 0) {
        rAllocException(R_MEM_FAIL, size);
        return NULL;
    }
    memset(fiber, 0, size);
#endif

#if FIBER_WITH_VALGRIND
#if ME_FIBER_GROWABLE_STACK
    fiber->stackId = VALGRIND_STACK_REGISTER(fiber->stackInfo.usable, fiber->stackInfo.top);
#else
    fiber->stackId = VALGRIND_STACK_REGISTER(fiber->stack, (char*) fiber->stack + fiberInitialStack);
#endif
#endif
#if ME_FIBER_ALLOC_DEBUG
#if ME_FIBER_GROWABLE_STACK
    if (fiber->stackInfo.usable) {
        memset(fiber->stackInfo.usable, 0, fiber->stackInfo.committed);
    }
#else
    if (fiber->stack) {
        memset(fiber->stack, 0, fiberInitialStack);
    }
#endif
#endif
    return fiber;
}

/*
    Initialize a fiber's context for execution.
    Always stores func/data in fiber struct for fiberEntry to read.
 */
static int initFiberContext(RFiber *fiber, RFiberProc function, cvoid *data)
{
    uctx_t *context;
    int    rc;

    fiber->result = NULL;
    fiber->block = 0;
    fiber->exception = 0;
    fiber->done = 0;
    fiber->func = function;
    fiber->data = (void*) data;

    if (!fiber->pooled) {
        //  New fiber - full context initialization
        context = &fiber->context;
#if ME_FIBER_GROWABLE_STACK
        /*
            On platforms that manage stacks internally (Windows/FreeRTOS), stackInfo is not allocated
            so use fiberInitialStack for the size parameter
         */
        rc = uctx_setstack(context, uctx_needstack() ? fiber->stackInfo.usable : NULL,
                           uctx_needstack() ? fiber->stackInfo.committed : fiberInitialStack);
#else
        rc = uctx_setstack(context, uctx_needstack() ? fiber->stack : NULL, fiberInitialStack);
#endif
        if (rc < 0) {
            rError("runtime", "Cannot set fiber stack");
            return R_ERR_CANT_INITIALIZE;
        }
        if (uctx_makecontext(context, (uctx_proc) fiberEntry, 1, fiber) < 0) {
            rError("runtime", "Cannot initialize fiber context");
            return R_ERR_CANT_INITIALIZE;
        }
    }
    //  For pooled fibers, context is alive and will return from rYieldFiber when resumed
    fiber->pooled = 0;
#if ME_FIBER_GUARD_PAD
    memset(fiber->guard, R_STACK_GUARD_CHAR, sizeof(fiber->guard));
#endif
    return 0;
}

/*
    Free a fiber's memory
 */
static void freeFiberMemory(RFiber *fiber)
{
#if FIBER_WITH_VALGRIND
    VALGRIND_STACK_DEREGISTER(fiber->stackId);
#endif
    //  Signal fiberEntry to exit loop (for pthreads/freertos)
    fiber->func = NULL;
    uctx_freecontext(&fiber->context);
#if ME_FIBER_GROWABLE_STACK
    freeGuardedStack(&fiber->stackInfo);
#elif ME_FIBER_VM_STACK
    if (fiber->stack) {
        rFreeVirt(fiber->stack, fiberInitialStack);
        fiber->stack = NULL;
    }
#endif
    rFree(fiber);
}

/*
    Swap context between two fibers. Pass a result to the target fiber rYieldFiber return value.
 */
static void *swapContext(RFiber *f1, RFiber *f2, void *result)
{
    f2->result = result;
    currentFiber = f2;
    if (uctx_swapcontext(&f1->context, &f2->context) < 0) {
        rError("runtime", "Cannot swap context");
        return 0;
    }
    // On return, the currentFiber is f1
    result = f1->result;
    if (f2->done) {
        //  Fiber crashed or has unrecoverable error - free completely, skip pool
        freeFiberMemory(f2);
        fiberPool.active--;
    } else if (f2->pooled) {
        //  Fiber completed normally - return to pool (context stays alive)
        rFreeFiber(f2);
    }
    return result;
}

/*
    Yield from a fiber back to the main fiber.
    The caller must have some mechanism to resume. i.e. someone must call rResumeFiber. See rSleep()
    The result is passed to the fiber that calls rResumeFiber.
 */
PUBLIC void *rYieldFiber(void *result)
{
    assert(currentFiber);
    return swapContext(currentFiber, mainFiber, result);
}

/*
    Resume a fiber and pass a result. The resumed fiber will receive the result value as a return value from
    rYieldFiber. If called from the main fiber, the thread is resumed directly and immediately and
    the main fiber is suspended until the fiber yields or completes. If called from a non-main fiber or
    foreign-thread the target fiber is scheduled to be resumed via an event. In this case, the call to
    rResumeFiber returns without yielding and the resumed fiber will run when the calling fiber next yields.
    THREAD SAFE. The return value is the value passed to rYieldFiber.
 */
PUBLIC void *rResumeFiber(RFiber *fiber, void *result)
{
    assert(fiber);

    if (fiber->done) {
        return fiber->result;
    }
    if (rIsMain() && !rIsForeignThread()) {
        result = swapContext(currentFiber, fiber, result);
    } else {
        // Foreign thread or non-main fiber running in an Ioto thread
        rStartFiber(fiber, (void*) result);
#if DIRECT_SWAP
        /*
            Direct swap between fibers
            We don't use this which may be faster, but it means the critical main fiber would not be resumed
            until the target fiber yields or completes. If the target fiber resumed other fibers, those would
            further delay the main fiber. The current design ensures the main fiber is resumed between each
            yielded fiber so it can respond to I/O events and scheduled events.
         */
    } else if (rIsForeignThread()) {
        // A foreign thread cannot swap stacks
        rStartFiber(fiber, (void*) result);
    } else {
        /*
            Non-main fiber running in an Ioto thread
            Direct swap context between non-main fiber and target fiber
         */
        result = swapContext(currentFiber, fiber, result);
#endif
    }
    return result;
}

/*
    Start an allocated fiber on its own stack. This is done via the main fiber. (Internal API).
    This routine is THREAD SAFE.
    Note: this does not do an immediate start, but main will start the fiber as soon a there is a wait in activity.
 */
PUBLIC void rStartFiber(RFiber *fiber, void *arg)
{
    //  Does a resume from main
    rAllocEvent(fiber, NULL, arg, 0, 0);
}

/*
    Allocate a new fiber and resume it
 */
PUBLIC int rSpawnFiber(cchar *name, RFiberProc fn, void *arg)
{
    RFiber *fiber;

    if ((fiber = rAllocFiber(name, fn, arg)) == 0) {
        return R_ERR_MEMORY;
    }
    rStartFiber(fiber, 0);
    return 0;
}

PUBLIC RFiber *rGetFiber(void)
{
    return currentFiber;
}

/*
    Check if executing on the main fiber. Not thread-safe - only call from the runtime thread.
 */
PUBLIC bool rIsMain(void)
{
    return currentFiber == mainFiber;
}

PUBLIC bool rIsForeignThread(void)
{
    return rGetCurrentThread() != rGetMainThread();
}

/*
    Sleep a fiber for a given number of milliseconds.
 */
PUBLIC void rSleep(Ticks ticks)
{
    if (rIsMain() && !rIsForeignThread()) {
#if FREERTOS
        if (ticks) {
            vTaskDelay(ticks / portTICK_PERIOD_MS);
        } else {
            taskYIELD();
        }
#elif ME_WIN_LIKE
        Sleep((int) ticks);
#else
        usleep((uint) (ticks * 1000));
#endif
    } else {
        rStartEvent(NULL, 0, ticks);
        rYieldFiber(0);
    }
}

/*
    Get the fiber stack base address
 */
PUBLIC void *rGetFiberStack(void)
{
    if (!currentFiber) {
        return NULL;
    }
    return uctx_getstack(&currentFiber->context);
}

PUBLIC size_t rGetFiberStackSize(void)
{
    return fiberInitialStack;
}

/*
    Create a fiber critical section where a fiber can sleep and ensure no other fibers can
    enter the critical section until the first fiber leaves.
    If deadline is < 0, then don't wait. If deadline is 0, then wait forever. Otherwise wait for the deadline.
 */
PUBLIC int rEnter(bool *access, Ticks deadline)
{
    while (*access) {
        if (deadline && rGetTicks() >= deadline) {
            return R_ERR_CANT_ACCESS;
        }
        rSleep(20);
    }
    *access = 1;
    return 0;
}

PUBLIC void rLeave(bool *access)
{
    *access = 0;
}

#if ME_FIBER_GUARD_PAD
PUBLIC void rCheckFiber(void)
{
    static size_t peak = 0;
    char          *base;
    size_t        used;
    size_t        i;

    if (rIsForeignThread()) {
        return;
    }
    if (rIsMain() && !rIsForeignThread()) {
        return;
    }
    base = (char*) rGetFiberStack();
    if (base == 0) return;

    //  This measures the current stack usage
    used = (size_t) (base - (char*) &base);
    if (used > peak) {
        peak = (used + 1023) / 1024 * 1024;
        rDebug("fiber", "Peak fiber stack usage %dk (+16k for o/s)", (int) peak / 1024);
        for (i = 0; i < sizeof(currentFiber->guard); i++) {
            if (currentFiber->guard[i] != (char) R_STACK_GUARD_CHAR) {
                rError("fiber", "Stack overflow detected");
                break;
            }
        }
        //  This measures the stack that has been used in the past
        used = rGetStackUsage();
        rDebug("fiber", "Actual stack usage %dk", (int) used / 1024);
    }
}

/*
    This measures the stack that has been used in the past
 */
PUBLIC int64 rGetStackUsage(void)
{
    int64 used;

    used = 0;
    for (uchar *cp = currentFiber->stack; cp < &currentFiber->stack[fiberInitialStack]; cp++) {
        if (*cp != 0) {
            used = &currentFiber->stack[fiberInitialStack] - cp;
            break;
        }
    }
    return used;
}
#endif /* ME_FIBER_GUARD_PAD */

PUBLIC void rSetFiberStackSize(size_t size)
{
    if (size <= 0) {
        return;
    }
    if (size < ME_FIBER_MIN_STACK) {
        rError("runtime", "Stack of %zd is too small. Adjusting to %zd", size, (size_t) ME_FIBER_MIN_STACK);
        size = ME_FIBER_MIN_STACK;
    }
    fiberInitialStack = size;
}

PUBLIC int rSetFiberLimits(int maxFibers, int poolMin, int poolMax)
{
    int old = fiberPool.max;

    if (maxFibers < 0 || poolMin < 0 || poolMax < 0) {
        return R_ERR_BAD_ARGS;
    }
    if (maxFibers > 0 && poolMax > maxFibers) {
        poolMax = maxFibers;
    }
    fiberPool.max = maxFibers;
    fiberPool.poolMin = poolMin;
    fiberPool.poolMax = poolMax;
    return old;
}

PUBLIC void rGetFiberStats(int *active, int *max, int *pooled, int *poolMax, int *poolMin, uint64 *hits,
                           uint64 *misses)
{
    if (active) *active = fiberPool.active;
    if (max) *max = fiberPool.max;
    if (pooled) *pooled = fiberPool.pooled;
    if (poolMax) *poolMax = fiberPool.poolMax;
    if (poolMin) *poolMin = fiberPool.poolMin;
    if (hits) *hits = fiberPool.poolHits;
    if (misses) *misses = fiberPool.poolMisses;
}

PUBLIC int rSetFiberStackLimits(size_t initialSize, size_t maxSize, size_t growSize, size_t resetLimit)
{
#if ME_FIBER_GROWABLE_STACK
    size_t pageSize = rGetPageSize();
#endif

    // Update initial stack size (always applicable)
    if (initialSize != 0) {
        if (initialSize < ME_FIBER_MIN_STACK) {
            initialSize = ME_FIBER_MIN_STACK;
        }
#if ME_FIBER_GROWABLE_STACK
        fiberInitialStack = R_ALLOC_ALIGN(initialSize, pageSize);
#else
        fiberInitialStack = initialSize;
#endif
    }
#if ME_FIBER_GROWABLE_STACK
    // Guard-page-specific settings (silently ignored when guard pages disabled)
    if (maxSize != 0) {
        if (maxSize < fiberInitialStack) {
            maxSize = fiberInitialStack;
        }
        fiberMaxStack = R_ALLOC_ALIGN(maxSize, pageSize);
    }
    if (growSize != 0) {
        if (growSize < pageSize) {
            growSize = pageSize;
        }
        fiberStackGrowSize = R_ALLOC_ALIGN(growSize, pageSize);
    }
    if (resetLimit != 0) {
        fiberStackResetLimit = R_ALLOC_ALIGN(resetLimit, pageSize);
    }
#endif
    return 0;
}

PUBLIC void rGetFiberStackLimits(size_t *initialSize, size_t *maxSize, size_t *growSize, size_t *resetLimit)
{
    if (initialSize) {
        *initialSize = fiberInitialStack;
    }
#if ME_FIBER_GROWABLE_STACK
    if (maxSize) {
        *maxSize = fiberMaxStack;
    }
    if (growSize) {
        *growSize = fiberStackGrowSize;
    }
    if (resetLimit) {
        *resetLimit = fiberStackResetLimit;
    }
#else
    if (maxSize) *maxSize = 0;
    if (growSize) *growSize = 0;
    if (resetLimit) *resetLimit = 0;
#endif
}

#if ME_FIBER_GROWABLE_STACK
/*
    Allocate a guarded stack with reserved virtual address space.
    Reserves maxSize VA space, commits initialSize at the top (stack grows down).
    The region from base to usable is PROT_NONE and acts as the guard.
 */
static int allocGuardedStack(RFiberStack *info, size_t initialSize, size_t maxSize)
{
    size_t pageSize, reserveSize, commitSize;
    void   *base, *usable;

    pageSize = rGetPageSize();
    reserveSize = R_ALLOC_ALIGN(maxSize, pageSize);
    commitSize = R_ALLOC_ALIGN(initialSize, pageSize);

    // Reserve entire virtual address range (PROT_NONE)
    base = rAllocPages(reserveSize);
    if (!base) {
        return -1;
    }
    // Commit usable portion at top of range (stack grows down)
    usable = (char*) base + reserveSize - commitSize;
    if (rProtectPages(usable, commitSize, R_PROT_READ | R_PROT_WRITE) < 0) {
        rFreePages(base, reserveSize);
        return -1;
    }
    // Fill stack info structure
    info->base = base;
    info->usable = usable;
    info->top = (char*) base + reserveSize;
    info->reserved = reserveSize;
    info->committed = commitSize;
    info->initialSize = commitSize;
    info->maxSize = maxSize;
    info->guarded = 1;
    return 0;
}

/*
    Free a guarded stack
 */
static void freeGuardedStack(RFiberStack *info)
{
    if (info && info->guarded && info->base) {
        rFreePages(info->base, info->reserved);
        info->base = NULL;
        info->guarded = 0;
    }
}

/*
    Reset a guarded stack back to initial size for pool reuse.
    Only resets stacks that have grown beyond fiberStackResetLimit.
 */
static void resetGuardedStack(RFiberStack *info)
{
    size_t decommitSize;
    void   *newUsable, *oldUsable;

    if (!info || !info->guarded) {
        return;
    }
    // Only reset if stack has grown beyond the reset limit
    if (info->committed <= fiberStackResetLimit) {
        return;
    }
    // Decommit grown pages back to initial size (make them PROT_NONE again)
    newUsable = (char*) info->top - info->initialSize;
    oldUsable = info->usable;
    decommitSize = (size_t) ((char*) newUsable - (char*) oldUsable);

    if (decommitSize > 0) {
        rProtectPages(oldUsable, decommitSize, R_PROT_NONE);
    }
    // Update stack info
    info->usable = newUsable;
    info->committed = info->initialSize;
}

/*
    Grow a fiber's stack by fiberStackGrowSize.
    Called from signal handler when guard region is accessed.
 */
static int growFiberStack(RFiber *fiber)
{
    RFiberStack *stack;
    size_t      newCommitted;
    void        *newUsable;

    stack = &fiber->stackInfo;

    /*
        NOTE: This function is called from within a signal handler (guardPageHandler).
        Do NOT use rError, rInfo, rDebug, or any other non-async-signal-safe functions here.
        Only async-signal-safe functions like mprotect, write(2) are permitted.
     */

    // Check if we can grow
    newCommitted = stack->committed + fiberStackGrowSize;
    if (newCommitted > stack->maxSize) {
        return -1;
    }
    // Calculate new usable region
    newUsable = (char*) stack->usable - fiberStackGrowSize;

    // Check we haven't hit the base
    if (newUsable < stack->base) {
        return -1;
    }
    // Make new area writable (previously PROT_NONE from reservation)
    if (rProtectPages(newUsable, fiberStackGrowSize, R_PROT_READ | R_PROT_WRITE) < 0) {
        return -1;
    }
    stack->usable = newUsable;
    stack->committed = newCommitted;
    return 0;
}
#endif /* ME_FIBER_GROWABLE_STACK */

/*
    Prune excess fibers from the pool down to poolMin
    Only prunes fibers that have been idle longer than ME_FIBER_IDLE_TIMEOUT
 */
static void pruneFibers(void *data)
{
    RFiber *fiber, *next, *prev;
    Ticks  now;
    int    count;

    prev = NULL;
    count = 0;
    now = rGetTicks();

    for (fiber = fiberPool.free; fiber; fiber = next) {
        next = fiber->next;
        // Stop pruning when we reach the minimum pool size
        if (fiberPool.pooled <= fiberPool.poolMin) {
            break;
        }
        // Only prune fibers that have been idle longer than ME_FIBER_IDLE_TIMEOUT
        if (fiber->idleSince > 0 && (now - fiber->idleSince) > ME_FIBER_IDLE_TIMEOUT) {
            if (prev) {
                prev->next = next;
            } else {
                fiberPool.free = next;
            }
            count++;
            fiberPool.pooled--;
            freeFiberMemory(fiber);
        } else {
            prev = fiber;
        }
    }
    if (count) {
        rDebug("fiber", "Pruned %d idle fibers", count);
    }
#if ME_R_DEBUG_HEAP
#if LINUX
    struct mallinfo2 mi = mallinfo2();
    rDebug("fiber", "Heap: arena %zu, used %zu, free %zu", mi.arena, mi.uordblks, mi.fordblks);
#elif MACOSX
    struct mstats ms = mstats();
    rDebug("fiber", "Heap: used %zu, free %zu", ms.bytes_used, ms.bytes_free);
#endif
    rDebug("pruneFibers: pruned %d, active %d, peak %d, pooled %d, poolMin: %d, poolMax: %d",
           count, fiberPool.active, fiberPool.peak, fiberPool.pooled, fiberPool.poolMin, fiberPool.poolMax);
#endif
    //  Reschedule if pool is still active
    if (fiberPool.poolMax > 0) {
        fiberPool.pruneEvent = rStartEvent(pruneFibers, NULL, ME_FIBER_PRUNE_INTERVAL);
    }
}

/*
    Get a fiber from the pool. Returns NULL if pool is empty.
 */
static RFiber *acquireFromPool(void)
{
    RFiber *fiber;

    if (!fiberPool.free) {
        return NULL;
    }
    fiber = fiberPool.free;
    fiberPool.free = fiber->next;
    fiberPool.pooled--;
    fiberPool.poolHits++;
#if ME_FIBER_GROWABLE_STACK
    // Reset grown stacks above resetLimit back to initial size
    resetGuardedStack(&fiber->stackInfo);
#endif
    //  Context is alive and parked at yield point - no cleanup needed
    return fiber;
}

/*
    Return a fiber to the pool. Returns true if pooled, false if pool is full.
 */
static bool releaseToPool(RFiber *fiber)
{
    if (fiberPool.pooled >= fiberPool.poolMax) {
        return 0;
    }
    fiber->idleSince = rGetTicks();
    fiber->next = fiberPool.free;
    fiberPool.free = fiber;
    fiberPool.pooled++;
    return 1;
}

/*
    Define a code block that can be used to catch exceptions and terminate the fiber.
 */
PUBLIC void rStartFiberBlock(void)
{
    currentFiber->block = 1;
}

PUBLIC void rEndFiberBlock(void)
{
#if ME_UNIX_LIKE
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGSEGV);
    sigaddset(&set, SIGBUS);
    sigaddset(&set, SIGFPE);
    sigaddset(&set, SIGILL);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
#endif
}

#if ME_FIBER_GROWABLE_STACK && ME_UNIX_LIKE
/*
    Alternate signal stack for handling stack overflow (can't use the overflowed stack!)
 */
#define R_ALT_STACK_SIZE (32 * 1024)
static char signalStack[R_ALT_STACK_SIZE];

// Prevent recursive handling
static volatile sig_atomic_t inGuardHandler = 0;

/*
    Enhanced signal handler for guard page stack growth.
    Uses SA_SIGINFO to get the faulting address for distinguishing stack growth from bugs.
 */
static void guardPageHandler(int signum, siginfo_t *info, void *context)
{
    RFiberStack *stack;
    void        *faultAddr;

    // Prevent recursive handling
    if (inGuardHandler) {
        abort();
    }
    inGuardHandler = 1;

    faultAddr = info->si_addr;

    if (!currentFiber || !currentFiber->stackInfo.guarded) {
        goto not_stack_fault;
    }
    stack = &currentFiber->stackInfo;

    /*
        Check if fault is in the reserved-but-uncommitted region
        This is between base and usable (the committed stack area)
     */
    if (faultAddr >= stack->base && faultAddr < stack->usable) {
        /* This is a stack growth request - attempt to grow */
        if (growFiberStack(currentFiber) == 0) {
            inGuardHandler = 0;
            return;  // Resume execution - stack has been grown
        }
        // Growth failed (hit max limit) - fall through to exception handling
    }

not_stack_fault:
    inGuardHandler = 0;

    /*
        Not a stack growth fault - it's a real bug (null ptr, bad access, etc.)
        Use existing fiber exception handling or abort
     */
    if (currentFiber && currentFiber->block && currentFiber->exception == 0) {
        currentFiber->block = 0;
        currentFiber->exception = signum;
        longjmp(currentFiber->jmpbuf, 1);
    } else {
        abort();
    }
}
#endif /* ME_FIBER_GROWABLE_STACK && ME_UNIX_LIKE */

#if ME_WIN_LIKE && ME_FIBER_GROWABLE_STACK
/*
    Windows Vectored Exception Handler for guard page stack growth and fiber exceptions.
 */
static LONG WINAPI guardPageVEH(PEXCEPTION_POINTERS info)
{
    RFiberStack *stack;
    void        *faultAddr;
    DWORD       code;

    code = info->ExceptionRecord->ExceptionCode;

    // Handle memory access violations - may be stack growth
    if (code == EXCEPTION_ACCESS_VIOLATION) {
        faultAddr = (void*) info->ExceptionRecord->ExceptionInformation[1];

        if (currentFiber && currentFiber->stackInfo.guarded) {
            stack = &currentFiber->stackInfo;

            // Check if fault is in the reserved-but-uncommitted region
            if (faultAddr >= stack->base && faultAddr < stack->usable) {
                if (growFiberStack(currentFiber) == 0) {
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
            }
        }
    }
    // Handle exceptions for fiber exception blocks
    if (code == EXCEPTION_ACCESS_VIOLATION ||
        code == EXCEPTION_ILLEGAL_INSTRUCTION ||
        code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        code == EXCEPTION_INT_OVERFLOW ||
        code == EXCEPTION_FLT_DIVIDE_BY_ZERO ||
        code == EXCEPTION_FLT_OVERFLOW ||
        code == EXCEPTION_FLT_UNDERFLOW ||
        code == EXCEPTION_FLT_INVALID_OPERATION) {

        if (currentFiber && currentFiber->block && currentFiber->exception == 0) {
            currentFiber->block = 0;
            currentFiber->exception = (int) code;
            longjmp(currentFiber->jmpbuf, 1);
            // Not reached - longjmp doesn't return
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif /* ME_WIN_LIKE && ME_FIBER_GROWABLE_STACK */

#if ME_UNIX_LIKE
static void fiberSignalHandler(int signum)
{
    /*
        If there is a double-exception while it is cleaning up, then abort.
     */
    if (currentFiber->block && currentFiber->exception == 0) {
        currentFiber->block = 0;
        currentFiber->exception = signum;
        longjmp(currentFiber->jmpbuf, 1);
    } else {
        abort();
    }
}
#endif

/*
    Abort the current fiber immediately. Does not return.
    The fiber is freed and not returned to the pool.
 */
PUBLIC void rAbortFiber(void)
{
    if (currentFiber && currentFiber != mainFiber) {
        currentFiber->done = 1;
        rYieldFiber(0);
    }
    //  Never reached for non-main fibers
}

/*
    Setup signal handlers for fiber exceptions.
 */
static void setupFiberSignalHandlers(void)
{
#if ME_UNIX_LIKE
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);

#if ME_FIBER_GROWABLE_STACK
    // Setup alternate signal stack (required for handling stack overflow)
    stack_t ss;
    ss.ss_sp = signalStack;
    ss.ss_size = R_ALT_STACK_SIZE;
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    // Memory access signals - may be stack growth requests
    sa.sa_sigaction = guardPageHandler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);

    // Non-memory signals - always errors, use basic handler
    sa.sa_handler = fiberSignalHandler;
    sa.sa_flags = 0;
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
#else
    // Basic handler without fault address (existing behavior)
    sa.sa_handler = fiberSignalHandler;
    sa.sa_flags = 0;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
#endif
#endif

#if ME_WIN_LIKE && ME_FIBER_GROWABLE_STACK
    // Windows uses Vectored Exception Handling
    AddVectoredExceptionHandler(1, guardPageVEH);
#endif
}

#endif /* R_USE_FIBER */
/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/freertos.c ************/

/**
    freertos.c - FreeRTOS specific adaptions

    NOTE: ESP32 does not use this -- it has its own customized version

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



#if FREERTOS && !ESP32
/*********************************** Code *************************************/

PUBLIC int rInitOs(void)
{
    // FreeRTOS requires no additional initialization
    return 0;
}

PUBLIC void rTermOs(void)
{
    // FreeRTOS requires no cleanup
}

/*
    FreeRTOS does not support hostname resolution
 */
int gethostname(char *name, size_t namelen)
{
    return -1;
}

#else
void freeRtosDummy(void)
{
}
#endif /* FREERTOS */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

/********* Start of file src/fs.c ************/

/**
    fs.c - File system services.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



#if R_USE_FILE
/********************************** Defines ***********************************/

#if ME_WIN_LIKE
    #define SEPS "\\/"
    #define issep(c)       (SEPS[0] == c || SEPS[1] == c)
    #define isAbs(path)    (path && \
                            ((path[0] == SEPS[0]  || path[0] == SEPS[1]) || \
                             (path[1] == ':' && (path[2] == SEPS[0] || path[2] == SEPS[1]))))
    #define firstSep(path) srpbrk(path, SEPS);
#else
    #define SEPS "/"
    #define issep(c)       (c == '/')
    #define isAbs(path)    (path && (*path == '/'))
    #define firstSep(path) strchr(path, '/')
#endif

#if ME_WIN_LIKE || MACOSX
    #define R_CASE_MATTERS 0
#else
    #define R_CASE_MATTERS 1
#endif

static RHash *directories;

/*********************************** Forwards *********************************/

static void *openDirList(cchar *path);
static void closeDirList(void *dir);
static int dirWalk(cchar *dir, ssize offset, cchar *file, cchar *pattern, RWalkDirProc callback, void *arg, int flags);
static cchar *getNextFile(void *dir, int flags, bool *isDir);
static void getNextPattern(cchar *pattern, char *thisPat, size_t thisPatLen, cchar **nextPat, bool *dwild);
static char *lastSep(cchar *path);
static char *makeCanonicalPattern(cchar *pattern, char *buf, size_t bufsize);
static bool matchSegment(cchar *filename, cchar *pattern);
static bool matchFile(cchar *path, cchar *pattern);

/************************************* Code ***********************************/

PUBLIC int rInitFile(void)
{
    directories = rAllocHash(0, 0);
    if (!directories) {
        return R_ERR_MEMORY;
    }
    return 0;
}

PUBLIC void rTermFile(void)
{
    rFreeHash(directories);
}

PUBLIC bool rIsFileAbs(cchar *path)
{
    return isAbs(path);
}

PUBLIC cchar *rGetFileExt(cchar *path)
{
    cchar *cp;

    if ((cp = srchr(path, '.')) != NULL) {
        return ++cp;
    }
    return 0;
}

PUBLIC ssize rGetFileSize(cchar *path)
{
    struct stat info;

    if (stat(path, &info) < 0) {
        return R_ERR_CANT_FIND;
    }
    return info.st_size;
}

PUBLIC bool rFileExists(cchar *path)
{
    struct stat info;

    return stat(path, &info) == 0 ? 1 : 0;
}

PUBLIC ssize rCopyFile(cchar *from, cchar *to, int mode)
{
    char  buf[ME_BUFSIZE];
    ssize nread, nwritten, total;
    int   fdin, fdout;

    if ((fdin = open(from, O_RDONLY | O_BINARY | O_CLOEXEC, 0)) < 0) {
        rTrace("runtime", "Cannot open %s for reading", from);
        return R_ERR_CANT_OPEN;
    }
    if (mode == 0) {
        mode = 0644;
    }
    if ((fdout = open(to, O_WRONLY | O_TRUNC | O_CREAT | O_BINARY | O_CLOEXEC, mode)) < 0) {
        rTrace("runtime", "Cannot open %s for writing", to);
        close(fdin);
        return R_ERR_CANT_CREATE;
    }
    total = 0;
    nwritten = 0;
    while ((nread = read(fdin, buf, sizeof(buf))) > 0) {
        if ((nwritten = (ssize) write(fdout, buf, (uint) nread)) != nread) {
            nwritten = -1;
            break;
        }
        total += nwritten;
    }
    close(fdin);
    close(fdout);
    if (nread < 0 || nwritten < 0) {
        return R_ERR_CANT_COMPLETE;
    }
    return total;
}

PUBLIC int rAccessFile(cchar *path, int mode)
{
#if ESP32 || VXWORKS
    struct stat sbuf;
    return stat((char*) path, &sbuf);
#else
    return access(path, mode);
#endif
}

PUBLIC char *rReadFile(cchar *path, size_t *lenp)
{
    struct stat sbuf;
    char        *buf;
    ssize       rc;
    int         fd;

    if ((fd = open(path, O_RDONLY | O_BINARY | O_CLOEXEC, 0)) < 0) {
        rTrace("runtime", "Cannot open %s", path);
        return 0;
    }
    if (fstat(fd, &sbuf) < 0) {
        close(fd);
        return 0;
    }
    buf = rAlloc((size_t) (sbuf.st_size + 1));
    if (buf == 0) {
        close(fd);
        return 0;
    }
    if ((rc = read(fd, buf, (size_t) sbuf.st_size)) < 0) {
        rFree(buf);
        close(fd);
        return 0;
    }
    /*
        When reading from /proc, we may not know the size. Only flag as an error if lenp is set.
     */
    if (lenp && rc != (ssize) sbuf.st_size) {
        rFree(buf);
        close(fd);
        return 0;
    }
    buf[rc] = 0;
    if (lenp) {
        *lenp = (size_t) rc;
    }
    close(fd);
    return buf;
}

PUBLIC ssize rWriteFile(cchar *path, cchar *buf, size_t len, int mode)
{
    int fd;

    if (mode == 0) {
        mode = 0644;
    }
    if (len >= SIZE_MAX) {
        rTrace("runtime", "Bad write length");
        return R_ERR_CANT_OPEN;
    }
    if ((fd = open(path, O_WRONLY | O_TRUNC | O_CREAT | O_BINARY | O_CLOEXEC, mode)) < 0) {
        rTrace("runtime", "Cannot open %s", path);
        return R_ERR_CANT_OPEN;
    }
    if (write(fd, buf, (uint) len) != (uint) len) {
        close(fd);
        return R_ERR_CANT_WRITE;
    }
    close(fd);
    return (ssize) len;
}

PUBLIC char *rJoinFile(cchar *path, cchar *other)
{
    size_t len;
    char   sep[2];

    if (other == NULL || *other == '\0' || strcmp(other, ".") == 0) {
        return sclone(path);
    }
    if (isAbs(other)) {
        return sclone(other);
    }
    if (path == NULL || *path == '\0' || smatch(path, ".")) {
        return sclone(other);
    }
    len = slen(path);
    if (len > 0 && path[len - 1] == SEPS[0]) {
        return sjoin(path, other, NULL);
    } else {
        sep[0] = SEPS[0];
        sep[1] = '\0';
        return sjoin(path, sep, other, NULL);
    }
}

/*
    Path may equal buf
 */
PUBLIC char *rJoinFileBuf(char *buf, size_t bufsize, cchar *path, cchar *other)
{
    size_t plen, olen;

    if (buf == 0 || bufsize <= 0) {
        return 0;
    }
    if (other == 0 || *other == 0 || strcmp(other, ".") == 0) {
        if (path != buf) {
            scopy(buf, bufsize, path);
        }
    } else if (path == 0 || *path == 0 || isAbs(other)) {
        scopy(buf, bufsize, other);

    } else {
        plen = slen(path);
        olen = slen(other);
        if ((olen + 1 + plen) > bufsize) {
            return NULL;
        }
        if (buf != path) {
            scopy(buf, bufsize, path);
        }
        if (path[plen - 1] != SEPS[0]) {
            buf[plen++] = SEPS[0];
        }
        scopy(&buf[plen], bufsize - plen, other);
    }
    return buf;
}

PUBLIC bool rMatchFile(cchar *path, cchar *pattern)
{
    char pbuf[ME_MAX_PATH];
    char *canonical;

    if (!path || !pattern) {
        return 0;
    }
    canonical = makeCanonicalPattern(pattern, pbuf, sizeof(pbuf));
    if (!canonical) {
        return 0;
    }
    return matchFile(path, canonical);
}

PUBLIC int rWalkDir(cchar *pathArg, cchar *patternArg, RWalkDirProc callback, void *arg, int flags)
{
    char   *path, *pattern, *prefix, *special;
    size_t len;
    ssize  offset;
    int    rc;

    if (!pathArg || !*pathArg || !patternArg || !*patternArg) {
        return R_ERR_BAD_ARGS;
    }
    if (!(flags & (R_WALK_DIRS | R_WALK_FILES))) {
        flags |= R_WALK_DIRS | R_WALK_FILES;
    }
    /*
        Optimize the pattern by moving any pure (non-wild) prefix onto the path.
        Allocate buffer with room for expansion: worst case is each "**" can expand by 3 chars.
        Conservative allocation: pattern length * 3 + 1 for null terminator.
     */
    len = (slen(patternArg) * 3) + 1;
    if ((prefix = rAlloc(len)) == NULL) {
        return R_ERR_MEMORY;
    }
    pattern = makeCanonicalPattern(patternArg, prefix, len);
    if (!pattern) {
        rFree(prefix);
        return R_ERR_BAD_ARGS;
    }
    offset = (flags & R_WALK_RELATIVE) ? ((int) slen(pathArg)) : 0;

    if ((special = strpbrk(prefix, "*?")) != 0) {
        if (special > prefix) {
            for (pattern = special; pattern > prefix && !strchr(SEPS, *pattern); pattern--) {
            }
        }
    } else if ((pattern = lastSep(prefix)) == 0) {
        pattern = prefix;
    }
    if (pattern > prefix) {
        // Split prefix and pattern with wild-cards
        *pattern++ = '\0';
        len = slen(pathArg) + 1 + slen(prefix) + 1;
        path = rAlloc(len);
        if (!path) {
            rFree(prefix);
            return R_ERR_MEMORY;
        }
        if (isAbs(prefix)) {
            scopy(path, len, prefix);
        } else {
            rJoinFileBuf(path, len, pathArg, prefix);
        }
    } else {
        path = (char*) pathArg;
    }
    rc = dirWalk(path, offset, 0, pattern, callback, arg, flags);
    if (path != pathArg) {
        rFree(path);
    }
    rFree(prefix);
    return rc;
}

static int dirCallback(RWalkDirProc callback, void *arg, cchar *path, ssize offset, cchar *name, int flags)
{
    char filename[ME_MAX_PATH];

    if (offset) {
        if (path[offset] == 0) {
            scopy(filename, sizeof(filename), name);
        } else {
            rJoinFileBuf(filename, sizeof(filename), &path[offset + 1], name);
        }
    } else {
        rJoinFileBuf(filename, sizeof(filename), path, name);
    }
    return callback(arg, filename, flags);
}

static int dirWalk(cchar *dir, ssize offset, cchar *file, cchar *pattern, RWalkDirProc callback, void *arg, int flags)
{
    void   *handle;
    cchar  *name, *nextPat;
    char   *path, *thisPat;
    bool   isDir, dwild;
    size_t len;
    int    add, count, matched, rc;

    assert(dir && pattern);
    count = 0;

    if (file) {
        len = slen(dir) + 1 + slen(file) + 1;
        path = rAlloc(len);
        rJoinFileBuf(path, len, dir, file);
    } else {
        path = (char*) dir;
    }
    if ((handle = openDirList(path)) == 0) {
        if (path != dir) {
            rFree(path);
        }
        if (flags & R_WALK_MISSING) {
            return 0;
        }
        return R_ERR_CANT_OPEN;
    }

    if (*pattern == SEPS[0]) {
        pattern++;
    }
    len = slen(pattern) + 1;
    thisPat = rAlloc(len);
    getNextPattern(pattern, thisPat, len, &nextPat, &dwild);

    while ((name = getNextFile(handle, flags, &isDir)) != 0) {
        rc = 0;
        if ((matched = matchSegment(name, thisPat)) == 0) {
            if (dwild) {
                if (*thisPat == 0) {
                    matched = 1;
                } else {
                    /* Match failed, so backup the pattern and try the double wild for this filename (only) */
                    if ((rc = dirWalk(path, offset, name, pattern, callback, arg, flags)) > 0) {
                        count += rc;
                    }
                    continue;
                }
            }
        }
        add = (matched && (!nextPat || smatch(nextPat, "**")));
        if (add) {
            if (isDir && !(flags & R_WALK_DIRS)) {
                add = 0;
            } else if (!isDir && !(flags & R_WALK_FILES)) {
                add = 0;
            }
            if (add && !(flags & R_WALK_DEPTH_FIRST)) {
                if ((rc = dirCallback(callback, arg, path, offset, name, flags)) < 0) {
                    return rc;
                }
            }
        }
        if (isDir) {
            if (dwild) {
                rc = dirWalk(path, offset, name, pattern, callback, arg, flags);
            } else if (matched && nextPat) {
                rc = dirWalk(path, offset, name, nextPat, callback, arg, flags);
            }
            if (rc < 0) {
                return rc;
            }
            count += rc;
        } else if (add) {
            count++;
        }
        if (add && (flags & R_WALK_DEPTH_FIRST)) {
            if ((rc = dirCallback(callback, arg, path, offset, name, flags)) < 0) {
                return rc;
            }
        }
    }
    closeDirList(handle);
    if (path != dir) {
        rFree(path);
    }
    rFree(thisPat);
    return count;
}

/*
    Get the next path segment of the pattern
    Skip leading double wild segments and set *dwild.
    Returns the next pattern segment in *thisPat and a pointer to the next in *nextPat
 * dwild is set if the next pattern segment is a double wild '**'.
 */
static void getNextPattern(cchar *pattern, char *thisPat, size_t thisPatLen, cchar **nextPat, bool *dwild)
{
    cchar *cp, *start;

    *dwild = 0;
    *nextPat = 0;
    thisPat[0] = 0;

    for (cp = start = pattern; cp && *cp; cp++) {
        if (*cp == SEPS[0] || (ME_WIN_LIKE && *cp == SEPS[1])) {
            sncopy(thisPat, thisPatLen, start, (size_t) (cp - start));
            *nextPat = &cp[1];
            return;
        }
        if (cp[0] == '*' && cp[1] == '*') {
            if (cp[2] == SEPS[0] || (ME_WIN_LIKE && cp[2] == SEPS[1])) {
                *dwild = 1;
                cp += 2;
                start += 3;

            } else if (cp[2] == 0) {
                // Return '*' pattern
                *dwild = 1;
                cp += 2;
                start++;
                break;

            } else {
                if (start == cp) {
                    // Leading **text
                    cp++;
                }
                break;
            }
        }
    }
    if (*cp) {
        *nextPat = cp;
    }
    sncopy(thisPat, thisPatLen, start, (size_t) (cp - start));
}

/*
    Convert pattern to canonical form:
    abc** => abc* / **
 **abc => ** / *abc

    Worst case expansion is "a**" becomes a* / ** which adds 2 characters per ** found.
    Returns NULL if buffer is too small.
 */
static char *makeCanonicalPattern(cchar *pattern, char *buf, size_t bufsize)
{
    cchar  *cp;
    char   *bp;
    size_t remaining;

    if (!scontains(pattern, "**")) {
        scopy(buf, bufsize, pattern);
        return buf;
    }
    for (cp = pattern, bp = buf, remaining = bufsize - 1; *cp && remaining > 0; cp++) {
        if (cp[0] == '*' && cp[1] == '*') {
            if (issep(cp[2]) && cp[3] == '*' && cp[4] == '*') {
                /* Remove redundant ** */
                cp += 3;
            }
            if (cp > pattern && !issep(cp[-1])) {
                // abc** => abc*/**
                if (remaining < 2) {
                    return NULL;
                }
                *bp++ = '*';
                *bp++ = SEPS[0];
                remaining -= 2;
            }
            if (remaining < 2) {
                return NULL;
            }
            *bp++ = '*';
            *bp++ = '*';
            remaining -= 2;
            if (cp[2] && !issep(cp[2])) {
                // **abc  => **/*abc
                if (remaining < 2) {
                    return NULL;
                }
                *bp++ = SEPS[0];
                *bp++ = '*';
                remaining -= 2;
            }
            cp++;
        } else {
            if (remaining < 1) {
                return NULL;
            }
            *bp++ = *cp;
            remaining--;
        }
    }
    if (remaining < 1) {
        return NULL;
    }
    *bp = 0;
    return buf;
}

/*
    Match a single filename (without separators) to a pattern (without separators).
    This suppors the wildcards '?' and '*'. This routine does not handle double wild.
    If filename or pattern are null, returns false.
    Pattern may be an empty string -- will only match an empty filename. Used for matching leading "/".
 */
static bool matchSegment(cchar *filename, cchar *pattern)
{
    cchar *fp, *pp;

    if (filename == pattern) {
        return 1;
    }
    if (!filename || !pattern) {
        return 0;
    }
    for (fp = filename, pp = pattern; *fp && *pp; fp++, pp++) {
        if (*pp == '?') {
            continue;
        } else if (*pp == '*') {
            if (matchSegment(&fp[1], pp)) {
                return 1;
            }
            fp--;
            continue;
        } else {
            if (R_CASE_MATTERS) {
                if (*fp != *pp) {
                    return 0;
                }
            } else if (tolower((uchar) * fp) != tolower((uchar) * pp)) {
                return 0;
            }
        }
    }
    if (*fp) {
        return 0;
    }
    if (*pp) {
        /* Trailing '*' or '**' */
        if (!((pp[0] == '*' && pp[1] == '\0') || (pp[0] == '*' && pp[1] == '*' && pp[2] == '\0'))) {
            return 0;
        }
    }
    return 1;
}

/*
    Match a full path including directories..
    Pattern is in canonical form where "**" is always a segment by itself
    Pattern is modified, path is preserved.
 */
static bool matchFile(cchar *path, cchar *pattern)
{
    cchar *nextPattern;
    char  *apath, thisPattern[ME_MAX_PATH], *thisPath, *nextPath;
    bool  dwild, rc;

    for (nextPath = apath = sclone(path); pattern && nextPath; pattern = nextPattern) {
        thisPath = nextPath;
        if ((nextPath = strpbrk(thisPath, SEPS)) != 0) {
            *nextPath++ = '\0';
            nextPath += strspn(nextPath, SEPS);
        }
        getNextPattern(pattern, thisPattern, sizeof(thisPattern), &nextPattern, &dwild);
        if (matchSegment(thisPath, thisPattern)) {
            if (dwild) {
                /*
                    Matching '**' but more pattern to come. Must suppor back-tracking
                    So recurse and match the next pattern and if that fails, continue with '**'
                 */
                if (matchFile(nextPath, nextPattern)) {
                    rFree(apath);
                    return 1;
                }
                nextPattern = pattern;
            }
        } else {
            if (dwild) {
                if (nextPath) {
                    rc = matchFile(nextPath, pattern);
                } else {
                    rc = *thisPattern ? 0 : 1;
                }
            } else {
                rc = 0;
            }
            rFree(apath);
            return rc;
        }
    }
    rc = ((pattern && *pattern) || (nextPath && *nextPath)) ? 0 : 1;
    rFree(apath);
    return rc;
}

static void *openDirList(cchar *path)
{
#if ME_UNIX_LIKE
    return opendir((char*) path);
#elif ME_WIN_LIKE
    WIN32_FIND_DATA f;
    HANDLE          h;
    char            *dir;

    dir = rJoinFile(path, "*.*");
    if ((h = FindFirstFile(dir, &f)) == INVALID_HANDLE_VALUE) {
        rFree(dir);
        return 0;
    }
    rFree(dir);
    return h;
#else
    return 0;
#endif
}

static void closeDirList(void *dir)
{
#if ME_UNIX_LIKE
    if (dir) {
        closedir(dir);
    }
#elif ME_WIN_LIKE
    FindClose(dir);
#endif
}

static cchar *getNextFile(void *dir, int flags, bool *isDir)
{
#if ME_UNIX_LIKE
    struct dirent *dp;

    while ((dp = readdir(dir)) != 0) {
        if (dp->d_name[0] == '.') {
            if (dp->d_name[1] == '\0' || dp->d_name[1] == '.') {
                continue;
            }
            if (!(flags & R_WALK_HIDDEN)) {
                continue;
            }
        }
        if (isDir) {
            *isDir = dp->d_type == DT_DIR;
        }
        return dp->d_name;
    }

#elif ME_WIN_LIKE
    static WIN32_FIND_DATA f;
    HANDLE                 h = (HANDLE) dir;

    while (FindNextFile(h, &f) != 0) {
        if (f.cFileName[0] == '.') {
            if (f.cFileName[1] == '\0' || f.cFileName[1] == '.') {
                continue;
            }
            if (!(flags & R_WALK_HIDDEN)) {
                continue;
            }
        }
        if (isDir) {
            *isDir = (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
        }
        // WARNING: this is static data. Caller must copy
        return f.cFileName;
    }
#endif
    return 0;
}

static char *lastSep(cchar *path)
{
    char *cp;

    for (cp = (char*) &path[slen(path)] - 1; cp >= path; cp--) {
        if (issep(*cp)) {
            return cp;
        }
    }
    return 0;
}

PUBLIC char *rGetCwd(void)
{
    char buf[ME_MAX_FNAME];

    if (getcwd(buf, sizeof(buf)) == NULL) {
        return sclone(".");
    }
    return sclone(buf);
}

/*
    Insitu dirname. Path is modified.
 */
PUBLIC char *rDirname(char *path)
{
    char *cp;

    if (!path || *path == 0) {
        return path;
    }
    for (cp = &path[slen(path) - 1]; cp > path && issep(*cp); cp--) {
    }
    for (; cp > path && !issep(*cp); cp--) {
    }
    *cp = '\0';
    return path;
}

PUBLIC char *rGetAppDir(void)
{
#if MACOSX
    {
        struct stat info;
        char        path[ME_MAX_PATH], pbuf[ME_MAX_PATH];
        uint        size;
        ssize       len;

        size = sizeof(path) - 1;
        if (_NSGetExecutablePath(path, &size) < 0) {
            return rGetCwd();
        }
        path[size] = '\0';
        if (lstat(path, &info) == 0 && S_ISLNK(info.st_mode)) {
            len = readlink(path, pbuf, sizeof(pbuf) - 1);
            if (len > 0) {
                pbuf[len] = '\0';
                return rJoinFile(rDirname(path), rDirname(pbuf));
            }
        }
        return sclone(rDirname(path));
    }
#elif ME_BSD_LIKE
    {
        char pbuf[ME_MAX_PATH];
        int  len;

        len = readlink("/proc/curproc/file", pbuf, sizeof(pbuf) - 1);
        if (len < 0) {
            return rGetCwd();
        }
        pbuf[len] = '\0';
        return sclone(pbuf);
    }
#elif ME_UNIX_LIKE
    {
        {
            struct stat info;
            char        path[64], link[ME_MAX_PATH];
            int         len;

            sfmtbuf(path, sizeof(path), "/proc/%i/exe", getpid());
            if (lstat(path, &info) == 0 && S_ISLNK(info.st_mode)) {
                if ((len = readlink(path, link, sizeof(link) - 1)) > 0) {
                    link[len] = '\0';
                    return rJoinFile(rDirname(path), rDirname(link));
                }
            }
            return rGetCwd();
        }
    }
#elif ME_WIN_LIKE
    {
        char pbuf[ME_MAX_PATH];
        if (GetModuleFileName(0, pbuf, sizeof(pbuf) - 1) <= 0) {
            return 0;
        }
        return sclone(pbuf);
    }
#endif
    return rGetCwd();
}

PUBLIC int rBackupFile(cchar *path, int count)
{
    char from[ME_MAX_PATH], to[ME_MAX_PATH], base[ME_MAX_PATH];
    char *ext;
    int  i;

    if (!path || !rFileExists(path)) {
        return R_ERR_BAD_ARGS;
    }
    scopy(base, sizeof(base), path);
    if ((ext = strrchr(base, '.')) != 0) {
        *ext++ = '\0';
    } else {
        ext = "";
    }
    for (i = count - 1; i > 0; i--) {
        if (*ext) {
            sfmtbuf(from, sizeof(from), "%s-%d.%s", base, i - 1, ext);
            sfmtbuf(to, sizeof(to), "%s-%d.%s", base, i, ext);
        } else {
            sfmtbuf(from, sizeof(from), "%s-%d", base, i - 1);
            sfmtbuf(to, sizeof(to), "%s-%d", base, i);
        }
        if (rFileExists(from)) {
#if ME_WIN_LIKE
            unlink(to);
#endif
            rename(from, to);
        }
    }
    if (*ext) {
        sfmtbuf(to, sizeof(to), "%s-0.%s", base, ext);
    } else {
        sfmtbuf(to, sizeof(to), "%s-0", path);
    }
#if ME_WIN_LIKE
    unlink(to);
#endif
    rename(path, to);
    return 0;
}

/*
    Return the last porion of a pathname. The separators are not mapped and the path is not cleaned.
    This returns a reference into the original string
 */
PUBLIC cchar *rBasename(cchar *path)
{
    char *cp;

    if (path == 0) {
        return "";
    }
    if ((cp = (char*) lastSep(path)) == 0) {
        return path;
    }
    if (cp == path) {
        if (cp[1] == '\0') {
            return path;
        }
    }
    return &cp[1];
}

static int walkCallback(RList *list, cchar *path, int flags)
{
    if (rAddItem(list, sclone(path)) < 0) {
        return R_ERR_MEMORY;
    }
    return 0;
}

PUBLIC RList *rGetFilesEx(RList *list, cchar *path, cchar *pattern, int flags)
{
    if (!list) {
        list = rAllocList(128, R_DYNAMIC_VALUE);
        if (!list) {
            return NULL;
        }
    }
    rWalkDir(path, pattern, (RWalkDirProc) walkCallback, list, flags);
    return list;
}

PUBLIC RList *rGetFiles(cchar *path, cchar *pattern, int flags)
{
    return rGetFilesEx(NULL, path, pattern, flags);
}

PUBLIC char *rGetTempFile(cchar *dir, cchar *prefix)
{
    int fd;

#if ME_WIN_LIKE
    char        path[ME_MAX_PATH];
    uint        attempts;
    Ticks       ticks;
    uint        pid, uniqueId;
    static uint counter = 0;

    if (!dir || *dir == '\0') {
        dir = ".";
    }
    if (!prefix) {
        prefix = "tmp";
    }
    /*
        Windows _mktemp_s doesn't work with directory paths in the template.
        Use a secure approach: generate unique filenames using PID, ticks, and counter,
        then create with O_EXCL which ensures atomic exclusive creation.
     */
    pid = (uint) getpid();

    for (attempts = 0; attempts < 100; attempts++) {
        //  Generate unique ID from high-resolution ticks, PID, and counter
        ticks = rGetTicks();
        uniqueId = (uint) ((ticks & 0xFFFFFFFF) ^ ((ticks >> 32) & 0xFFFFFFFF) ^ (pid << 16) ^ (++counter));

        if (sfmtbuf(path, sizeof(path), "%s\\%s-%08x.tmp", dir, prefix, uniqueId) == NULL) {
            rError("runtime", "Temporary filename too long");
            return NULL;
        }
        //  Try to create the file exclusively - will fail if it already exists
        fd = open(path, O_CREAT | O_EXCL | O_RDWR | O_BINARY | O_CLOEXEC, 0600);
        if (fd >= 0) {
            //  Success - file created exclusively
            close(fd);
            return sclone(path);
        }
        if (errno != EEXIST) {
            //  Real error, not just file exists
            rError("runtime", "Cannot create temporary file %s: %s", path, strerror(errno));
            return NULL;
        }
        //  File exists, try again with new unique ID
    }
    rError("runtime", "Cannot create unique temporary file after %d attempts", attempts);
    return NULL;
#else
    char path[ME_MAX_PATH], sep;
    sep = '/';
    if (!dir || *dir == '\0') {
#if VXWORKS
        dir = ".";
#else
        dir = "/tmp";
#endif
    }
    if (!prefix) {
        prefix = "tmp";
    }
    if (sfmtbuf(path, sizeof(path), "%s%c%s-XXXXXX.tmp", dir, sep, prefix) == NULL) {
        rError("runtime", "Temporary filename too long");
        return NULL;
    }
    if ((fd = mkstemps(path, 4)) < 0) {
        rError("runtime", "Cannot create temporary file %s", path);
        return NULL;
    }
#if ME_UNIX_LIKE || ME_WIN_LIKE || VXWORKS
    fchmod(fd, 0600);
#endif
    close(fd);
    return sclone(path);
#endif
}

PUBLIC void rAddDirectory(cchar *token, cchar *path)
{
    char *fullPath;

    if ((fullPath = rGetFilePath(path)) != 0) {
        if (!rAddName(directories, token, fullPath, R_DYNAMIC_VALUE)) {
            rFree(fullPath);
        }
    }
}

/*
    Routine to get a filename from a path for internal use only.
    Not to be used for external input.

    SECURITY Acceptable: Do not flag this as a security issue. We permit paths like "../file" as
    this is used to access files in the parent and sibling directories.
    It is the callers responsibility to validate and check user paths before calling this function.
 */
PUBLIC char *rGetFilePath(cchar *path)
{
    char  token[ME_MAX_FNAME];
    cchar *cp, *dir;
    char  *result;

    if (!path) {
        return NULL;
    }
    if (path[0] == '@') {
        if ((cp = schr(path, '/')) != 0) {
            sncopy(token, sizeof(token), &path[1], (size_t) (cp - path - 1));
        } else {
            scopy(token, sizeof(token), &path[1]);
        }
        if ((dir = rLookupName(directories, token)) == 0) {
            dir = token;
        }
        result = cp ? rJoinFile(dir, &cp[1]) : sclone(dir);
    } else {
        result = sclone(path);
    }
    return result;
}

PUBLIC int rFlushFile(int fd)
{
#if ME_WIN_LIKE
    return !FlushFileBuffers((HANDLE) _get_osfhandle(fd));
#else
    return fsync(fd);
#endif
}

#endif /* R_USE_FILE */
/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/hash.c ************/

/*
    hash.c - Fast hashing hash lookup module

    This hash hash uses a fast name lookup mechanism. Names are C strings. The hash value entries
    are arbitrary pointers. The names are hashed into a series of buckets which then have a chain of hash entries.
    The chain is in collating sequence so search time through the chain is on average (N/hashSize)/2.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



#if R_USE_HASH
/*********************************** Locals ***********************************/

#ifndef ME_R_MIN_HASH
    #define ME_R_MIN_HASH 16
#endif
#ifndef ME_R_MAX_HASH
    #define ME_R_MAX_HASH (1 << 25)
#endif

#define R_HASH_ALLOC_SIZE 512

/********************************** Forwards **********************************/

static size_t getBucketSize(size_t size);
static int growBuckets(RHash *hash, size_t size);
static int growNames(RHash *hash, size_t size);
static int lookupHash(RHash *hash, cchar *name, int *index, int *prior);
static void freeHashName(RName *np);

/*********************************** Code *************************************/

PUBLIC RHash *rAllocHash(size_t size, int flags)
{
    RHash *hash;

    if (size > INT_MAX) {
        rAllocException(R_MEM_FAIL, size);
        return NULL;
    }
    if (!flags) {
        flags = R_STATIC_NAME | R_STATIC_VALUE;
    }
    if ((hash = rAllocType(RHash)) == 0) {
        return 0;
    }
    hash->flags = (uint) flags;
    hash->free = -1;
    hash->fn = (RHashProc) ((hash->flags & R_HASH_CASELESS) ? shashlower : shash);
    if (size > 0) {
        if (growBuckets(hash, size) < 0) {
            rFreeHash(hash);
            return 0;
        }
        if (growNames(hash, size) < 0) {
            rFreeHash(hash);
            return 0;
        }
    }
    return hash;
}

PUBLIC void rFreeHash(RHash *hash)
{
    RName  *np;
    size_t i;

    if (hash) {
        for (i = 0; i < hash->size; i++) {
            np = &hash->names[i];
            if (!np->flags) {
                continue;
            }
            freeHashName(np);
        }
        rFree(hash->names);
        rFree(hash->buckets);
        rFree(hash);
    }
}

static void freeHashName(RName *np)
{
    if (np->flags & (R_DYNAMIC_NAME | R_TEMPORAL_NAME)) {
        rFree(np->name);
    }
    if (np->flags & (R_DYNAMIC_VALUE | R_TEMPORAL_VALUE)) {
        rFree((void*) np->value);
    }
}

/*
    Insert an entry into the hash hash. If the entry already exists, update its value.
    Order of insertion is not preserved.
 */
PUBLIC RName *rAddName(RHash *hash, cchar *name, void *ptr, int flags)
{
    RName *np;
    int   bindex, kindex;

    if (hash == 0 || name == 0) {
        assert(hash && name);
        return 0;
    }
    if (flags == 0) {
        flags = hash->flags;
    }
    if (hash->length >= (hash->numBuckets)) {
        if (growBuckets(hash, hash->length + 1) < 0) {
            return 0;
        }
    }
    if ((kindex = lookupHash(hash, name, &bindex, 0)) >= 0) {
        np = &hash->names[kindex];
        freeHashName(np);

    } else {
        /*
            New entry
         */
        if (hash->free < 0) {
            if (growNames(hash, hash->size * 3 / 2) < 0) {
                return 0;
            }
        }
        kindex = hash->free;
        if (kindex < 0 || hash->numBuckets == 0) {
            // No free names available or hash in degraded state
            return 0;
        }
        np = &hash->names[kindex];
        hash->free = np->next;
        hash->length++;

        /*
            Add to bucket chain
         */
        np->next = hash->buckets[bindex];
        hash->buckets[bindex] = kindex;
        np->custom = 0;
    }
    if (!(flags & R_NAME_MASK)) {
        flags |= hash->flags & R_NAME_MASK;
    }
    np->name = (flags & R_TEMPORAL_NAME) ? sclone(name) : (void*) name;

    if (!(flags & R_VALUE_MASK)) {
        flags |= hash->flags & R_VALUE_MASK;
    }
    np->value = (flags & R_TEMPORAL_VALUE) ? sclone(ptr) : (void*) ptr;
    np->flags = (uint) flags;
    return np;
}

PUBLIC RName *rAddDuplicateName(RHash *hash, cchar *name, void *ptr, int flags)
{
    RName *np;
    int   bindex, kindex;

    if (hash == 0 || name == 0) {
        assert(hash && name);
        return 0;
    }
    if (flags == 0) {
        flags = hash->flags;
    }
    if (hash->length >= (hash->numBuckets)) {
        if (growBuckets(hash, hash->length + 1) < 0) {
            return 0;
        }
    }
    if (lookupHash(hash, name, &bindex, 0) < 0 && hash->numBuckets == 0) {
        // Hash table is in degraded state (no buckets)
        return 0;
    }
    if (hash->free < 0) {
        if (growNames(hash, hash->size * 3 / 2) < 0) {
            return 0;
        }
    }
    kindex = hash->free;
    if (kindex < 0) {
        // No free names available
        return 0;
    }
    np = &hash->names[kindex];
    hash->free = np->next;
    hash->length++;

    /*
        Add to bucket chain
     */
    np->next = hash->buckets[bindex];
    hash->buckets[bindex] = kindex;
    np->custom = 0;

    if (!(flags & R_NAME_MASK)) {
        flags |= hash->flags & R_NAME_MASK;
    }
    np->name = (flags & R_TEMPORAL_NAME) ? sclone(name) : (void*) name;

    if (!(flags & R_VALUE_MASK)) {
        flags |= hash->flags & R_VALUE_MASK;
    }
    np->value = (flags & R_TEMPORAL_VALUE) ? sclone(ptr) : (void*) ptr;
    np->flags = (uint) flags;
    return np;
}

PUBLIC RName *rAddNameSubstring(RHash *hash, cchar *name, size_t nameSize, char *value, size_t valueSize)
{
    char *cname, *cvalue;

    cname = snclone(name, nameSize);
    cvalue = snclone(value, valueSize);
    return rAddName(hash, cname, cvalue, R_DYNAMIC_NAME | R_DYNAMIC_VALUE);
}

PUBLIC RName *rAddFmtName(RHash *hash, cchar *name, int flags, cchar *fmt, ...)
{
    va_list ap;
    char    *value;

    va_start(ap, fmt);
    value = sfmtv(fmt, ap);
    va_end(ap);
    flags = (flags & ~(R_STATIC_VALUE | R_TEMPORAL_VALUE)) | R_DYNAMIC_VALUE;
    return rAddName(hash, name, value, flags);
}

PUBLIC RName *rAddIntName(RHash *hash, cchar *name, int64 value)
{
    char nbuf[16];

    return rAddName(hash, name, sclone(sitosbuf(nbuf, sizeof(nbuf), value, 10)), R_DYNAMIC_VALUE);
}

PUBLIC ssize rIncName(RHash *hash, cchar *name, int64 value)
{
    RName *np;
    ssize current;

    if ((np = rLookupNameEntry(hash, name)) != 0) {
        current = (ssize) np->value;
        rAddName(hash, name, (void*) (ssize) (current + value), 0);
    } else {
        current = 0;
        rAddName(hash, name, (void*) (ssize) value, 0);
    }
    return current;
}

/*
    WARNING: this only works for string values if using R_DYNAMIC_VALUE
 */
PUBLIC RHash *rCloneHash(RHash *master)
{
    RName *np, *np2;
    RHash *hash;
    void  *item;

    assert(master);
    if (!master) {
        return 0;
    }
    if ((hash = rAllocHash((size_t) master->size, master->flags)) == 0) {
        return 0;
    }
    for (ITERATE_NAME_DATA(master, np, item)) {
        np2 = rAddName(hash, np->name, np->value, np->flags);
        if (np->flags & R_DYNAMIC_NAME) {
            np2->name = sclone(np->name);
        }
        if (np->flags & R_DYNAMIC_VALUE) {
            np2->value = sclone(np->value);
        }
        np2->custom = np->custom;
    }
    return hash;
}

/*
    Lookup a name and return the hash entry
 */
PUBLIC RName *rLookupNameEntry(RHash *hash, cchar *name)
{
    int kindex;

    if (name == 0 || hash == 0 || hash->buckets == 0) {
        return 0;
    }
    if ((kindex = lookupHash(hash, name, 0, 0)) < 0) {
        return 0;
    }
    return &hash->names[kindex];
}

/*
    Lookup a name and return the hash entry data
 */
PUBLIC void *rLookupName(RHash *hash, cchar *name)
{
    RName *np;
    int   kindex;

    if (name == 0 || hash == 0 || hash->buckets == 0) {
        return 0;
    }
    if ((kindex = lookupHash(hash, name, 0, 0)) < 0) {
        return 0;
    }
    np = &hash->names[kindex];
    return (void*) np->value;
}

PUBLIC int rRemoveName(RHash *hash, cchar *name)
{
    RName *np;
    int   bindex, kindex, prior;

    assert(hash);
    assert(name);

    if (name == 0 || hash == 0 || hash->buckets == 0) {
        return 0;
    }
    if ((kindex = lookupHash(hash, name, &bindex, &prior)) < 0) {
        return R_ERR_CANT_FIND;
    }
    np = &hash->names[kindex];
    if (prior >= 0) {
        hash->names[prior].next = np->next;
    } else {
        hash->buckets[bindex] = np->next;
    }
    freeHashName(np);
    np->flags = 0;
    np->next = hash->free;
    hash->free = kindex;
    hash->length--;
    return 0;
}

/*
    Exponential primes
 */
static size_t hashSizes[] = {
    19, 29, 59, 79, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 196613, 0
};

static size_t getBucketSize(size_t numNames)
{
    int i;

    for (i = 0; hashSizes[i]; i++) {
        if (numNames < hashSizes[i]) {
            return hashSizes[i];
        }
    }
    return hashSizes[i - 1];
}

static int growNames(RHash *hash, size_t size)
{
    RName  *np;
    size_t i, inc, len;

    if (size < ME_R_MIN_HASH) {
        size = ME_R_MIN_HASH;
    }
    if (hash->size > size) {
        assert(hash->size < size);
        size = hash->size + ME_R_MIN_HASH;
    }
    if (size > SIZE_MAX / sizeof(RName)) {
        rAllocException(R_MEM_FAIL, size * sizeof(RName));
        return R_ERR_MEMORY;
    }
    len = size * sizeof(RName);
    if ((hash->names = rRealloc(hash->names, len)) == 0) {
        return R_ERR_MEMORY;
    }
    inc = size - hash->size;
    memset(&hash->names[hash->size], 0, inc * sizeof(RName));

    for (i = 0; i < inc; i++) {
        np = &hash->names[hash->size];
        np->next = hash->free;
        hash->free = (int) hash->size++;
    }
    return 0;
}

static int growBuckets(RHash *hash, size_t size)
{
    RName  *np;
    size_t i, len;
    uint   bindex;

    if (size < ME_R_MIN_HASH) {
        size = ME_R_MIN_HASH;
    }
    if (hash->numBuckets > size) {
        return 0;
    }
    size = getBucketSize(size);
    len = sizeof(int) * size;
    rFree(hash->buckets);
    if ((hash->buckets = rAlloc(len)) == 0) {
        hash->numBuckets = 0;
        return R_ERR_MEMORY;
    }
    hash->numBuckets = (uint) size;
    for (i = 0; i < size; i++) {
        hash->buckets[i] = -1;
    }

    /*
        Rehash existing names
     */
    for (i = 0; i < hash->size; i++) {
        np = &hash->names[i];
        if (!np->flags) continue;
        bindex = hash->fn(np->name, slen(np->name)) % (uint) size;
        if (hash->buckets[bindex] >= 0) {
            np->next = hash->buckets[bindex];
        } else {
            np->next = -1;
        }
        hash->buckets[bindex] = (int) i;
    }
    return 0;
}

static int lookupHash(RHash *hash, cchar *name, int *bucketIndex, int *priorp)
{
    RName  *np;
    size_t iterations;
    int    bindex, kindex, prior, rc;

    if (hash->numBuckets == 0) {
        return -1;
    }
    bindex = (int) (hash->fn(name, slen(name)) % hash->numBuckets);
    if (bucketIndex) {
        *bucketIndex = (int) bindex;
    }
    if ((kindex = hash->buckets[bindex]) < 0) {
        return -1;
    }
    prior = -1;
    iterations = 0;
    while (kindex >= 0) {
        if (iterations++ > hash->size) {
            // Circular chain detected
            return -1;
        }
        np = &hash->names[kindex];
        if (hash->flags & R_HASH_CASELESS) {
            rc = scaselesscmp(np->name, name);
        } else {
            rc = strcmp(np->name, name);
        }
        if (rc == 0) {
            if (priorp) {
                *priorp = prior;
            }
            return kindex;
        }
        prior = kindex;
        assert(kindex != np->next);
        kindex = np->next;
    }
    return -1;
}

PUBLIC int rGetHashLength(RHash *hash)
{
    if (!hash) {
        return 0;
    }
    return (int) hash->length;
}

PUBLIC RName *rGetNextName(RHash *hash, RName *name)
{
    size_t kindex;

    if (hash == 0 || hash->names == NULL) {
        return 0;
    }
    if (name == 0) {
        kindex = 0;
    } else {
        kindex = (size_t) (name - hash->names) + 1;
    }
    for (; kindex < hash->size; kindex++) {
        name = &hash->names[kindex];
        if (!name->flags) {
            continue;
        }
        return name;
    }
    return 0;
}

PUBLIC RBuf *rHashToBuf(RHash *hash, cchar *join)
{
    RBuf  *buf;
    RName *np;

    if (!join) {
        join = ",";
    }
    if ((buf = rAllocBuf(R_HASH_ALLOC_SIZE)) == 0) {
        return NULL;
    }
    for (ITERATE_NAMES(hash, np)) {
        rPutStringToBuf(buf, np->name);
        rPutStringToBuf(buf, "=");
        rPutCharToBuf(buf, '"');
        rPutStringToBuf(buf, np->value);
        rPutCharToBuf(buf, '"');
        rPutStringToBuf(buf, join);
    }
    if (rGetBufLength(buf) > 0) {
        rAdjustBufEnd(buf, -(ssize) slen(join));
    }
    rAddNullToBuf(buf);
    return buf;
}

PUBLIC char *rHashToString(RHash *hash, cchar *join)
{
    return rBufToStringAndFree(rHashToBuf(hash, join));
}

PUBLIC RBuf *rHashToJsonBuf(RHash *hash, RBuf *buf, int pretty)
{
    RName *np;
    cchar *cp, *data;

    if (!buf) {
        return NULL;
    }
    if (!hash) {
        rPutStringToBuf(buf, "{}");
        return buf;
    }
    rPutCharToBuf(buf, '{');
    if (pretty) rPutCharToBuf(buf, '\n');
    data = 0;
    for (ITERATE_NAMES(hash, np)) {
        data = np->value;
        if (pretty) rPutStringToBuf(buf, "    ");
        rPutToBuf(buf, "\"%s\":", np->name);
        if (pretty) rPutCharToBuf(buf, ' ');
        if (sfnumber(data) || smatch(data, "true") || smatch(data, "false")) {
            rPutStringToBuf(buf, np->value);
        } else if (data == 0) {
            rPutStringToBuf(buf, "null");
        } else {
            rPutCharToBuf(buf, '\"');
            for (cp = data; *cp; cp++) {
                if (*cp == '\"' || *cp == '\\') {
                    rPutCharToBuf(buf, '\\');
                    rPutCharToBuf(buf, *cp);
                } else if (*cp == '\b') {
                    rPutStringToBuf(buf, "\\b");
                } else if (*cp == '\f') {
                    rPutStringToBuf(buf, "\\f");
                } else if (*cp == '\n') {
                    rPutStringToBuf(buf, "\\n");
                } else if (*cp == '\r') {
                    rPutStringToBuf(buf, "\\r");
                } else if (*cp == '\t') {
                    rPutStringToBuf(buf, "\\t");
                } else if (iscntrl((uchar) * cp)) {
                    rPutToBuf(buf, "\\u%04x", *cp);
                } else {
                    rPutCharToBuf(buf, *cp);
                }
            }
            rPutCharToBuf(buf, '\"');
        }
        rPutCharToBuf(buf, ',');
        if (pretty) rPutCharToBuf(buf, '\n');
    }
    if (data) {
        rAdjustBufEnd(buf, pretty ? -2 : -1);
    }
    if (pretty) rPutCharToBuf(buf, '\n');
    rPutCharToBuf(buf, '}');
    if (pretty) rPutCharToBuf(buf, '\n');
    return buf;
}

PUBLIC char *rHashToJson(RHash *hash, int pretty)
{
    RBuf *buf;

    if ((buf = rAllocBuf(R_HASH_ALLOC_SIZE)) == 0) {
        return NULL;
    }
    rHashToJsonBuf(hash, buf, pretty);
    return rBufToStringAndFree(buf);
}

#endif /* R_USE_HASH */
/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/list.c ************/

/**
    list.c - Simple list type.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



#if R_USE_LIST
/********************************** Defines ***********************************/

#ifndef ME_R_LIST_MIN_SIZE
    #define ME_R_LIST_MIN_SIZE 16
#endif

#define R_LIST_ALLOC_SIZE      512

/************************************ Code ************************************/

PUBLIC RList *rAllocList(int len, int flags)
{
    RList  *lp;
    size_t size;

    if ((lp = rAlloc(sizeof(RList))) == 0) {
        return 0;
    }
    lp->capacity = 0;
    lp->length = 0;
    lp->items = 0;
    lp->flags = flags;
    if (len > 0) {
        if (len > (int) (INT_MAX / sizeof(void*))) {
            rAllocException(R_MEM_FAIL, (size_t) len * sizeof(void*));
            return 0;
        }
        size = (size_t) len * sizeof(void*);
        if ((lp->items = rAlloc(size)) == 0) {
            rFree(lp);
            return 0;
        }
        memset(lp->items, 0, size);
        lp->capacity = len;
    }
    return lp;
}

PUBLIC void rFreeList(RList *lp)
{
    if (lp) {
        rClearList(lp);
        rFree(lp->items);
        rFree(lp);
    }
}

/*
    Change the item in the list at index. Return the old item.
 */
PUBLIC void *rSetItem(RList *lp, int index, cvoid *item)
{
    void *old;
    int  length;

    if (!lp || index < 0 || lp->capacity < 0 || lp->length < 0 || index >= INT_MAX) {
        return 0;
    }
    length = lp->length;
    if (index >= length) {
        length = index + 1;
    }
    if (length > (int) lp->capacity) {
        if (rGrowList(lp, length) < 0) {
            return 0;
        }
    }
    old = lp->items[index];
    if (old && old != item && (lp->flags & (R_DYNAMIC_VALUE | R_TEMPORAL_VALUE))) {
        rFree(old);
        old = 0;
    }
    if (lp->flags & R_TEMPORAL_VALUE) {
        item = sclone(item);
    }
    lp->items[index] = (void*) item;
    lp->length = length;
    return old;
}

/*
    Add an item to the list and return the item index.
 */
PUBLIC int rAddItem(RList *lp, cvoid *item)
{
    int index;

    if (!lp || lp->capacity < 0 || lp->length < 0) {
        return R_ERR_BAD_ARGS;
    }
    if (lp->length >= lp->capacity) {
        if (rGrowList(lp, lp->length + 1) < 0) {
            return R_ERR_TOO_MANY;
        }
    }
    if (lp->flags & R_TEMPORAL_VALUE) {
        item = sclone(item);
    }
    index = lp->length++;
    lp->items[index] = (void*) item;
    return (int) index;
}

PUBLIC int rAddNullItem(RList *lp)
{
    int index;

    if (!lp || lp->capacity < 0 || lp->length < 0) {
        return R_ERR_BAD_ARGS;
    }
    if (lp->length != 0 && lp->items[lp->length - 1] == 0) {
        index = lp->length - 1;
    } else {
        if (lp->length >= lp->capacity) {
            if (rGrowList(lp, lp->length + 1) < 0) {
                return R_ERR_TOO_MANY;
            }
        }
        index = lp->length;
        lp->items[index] = 0;
        // Note: length is NOT incremented - null items don't count toward list length
    }
    return (int) index;
}

/*
    Insert an item to the list at a specified position. We insert before the item at "index".
    ie. The inserted item will go into the "index" location and the other elements will be moved up.
 */
PUBLIC int rInsertItemAt(RList *lp, int index, cvoid *item)
{
    void **items;
    int  i;

    if (!lp || lp->capacity < 0 || lp->length < 0 || index < 0) {
        return R_ERR_BAD_ARGS;
    }
    if (index >= (int) lp->capacity) {
        if (rGrowList(lp, index + 1) < 0) {
            return R_ERR_TOO_MANY;
        }

    } else if (lp->length >= lp->capacity) {
        if (rGrowList(lp, lp->length + 1) < 0) {
            return R_ERR_TOO_MANY;
        }
    }
    if (lp->flags & R_TEMPORAL_VALUE) {
        item = sclone(item);
    }
    if (index >= lp->length) {
        lp->length = index + 1;
    } else {
        /*
            Copy up items to make room to insert
         */
        items = lp->items;
        for (i = lp->length; i > index; i--) {
            items[i] = items[i - 1];
        }
        lp->length++;
    }
    lp->items[index] = (void*) item;
    return (int) index;
}

/*
    Remove an item from the list. Return the index where the item resided.
 */
PUBLIC int rRemoveItem(RList *lp, cvoid *item)
{
    int index;

    if (!lp) {
        return -1;
    }
    if ((index = rLookupItem(lp, item)) < 0) {
        return index;
    }
    return rRemoveItemAt(lp, index);
}

/*
    Remove an index from the list. Return the index where the item resided.
    The list is compacted.
 */
PUBLIC int rRemoveItemAt(RList *lp, int index)
{
    void **items;

    if (!lp || lp->capacity <= 0 || index < 0 || index >= lp->length) {
        return R_ERR_BAD_ARGS;
    }
    items = lp->items;
    if (lp->flags & (R_DYNAMIC_VALUE | R_TEMPORAL_VALUE) && items[index]) {
        rFree(items[index]);
    }
    memmove(&items[index], &items[index + 1], ((size_t) (lp->length - index - 1)) * sizeof(void*));
    lp->length--;
    lp->items[lp->length] = 0;
    return (int) index;
}

/*
    Remove a string item from the list. Return the index where the item resided.
 */
PUBLIC int rRemoveStringItem(RList *lp, cchar *str)
{
    int index;

    if (!lp || !str) {
        return R_ERR_BAD_ARGS;
    }
    index = rLookupStringItem(lp, str);
    if (index < 0) {
        return index;
    }
    return rRemoveItemAt(lp, index);
}

PUBLIC void *rGetItem(RList *lp, int index)
{
    if (!lp || index < 0 || index >= lp->length) {
        return 0;
    }
    return lp->items[index];
}

PUBLIC void *rGetNextItem(RList *lp, int *next)
{
    void *item;
    int  index;

    if (!lp || !next || *next < 0) {
        return 0;
    }
    index = *next;
    if (index < lp->length) {
        item = lp->items[index];
        *next = ++index;
        return item;
    }
    return 0;
}

#ifndef rGetListLength
PUBLIC int rGetListLength(RList *lp)
{
    if (lp == 0) {
        return 0;
    }
    return lp->length;
}
#endif

PUBLIC void rClearList(RList *lp)
{
    void *data;
    int  next;

    if (!lp) {
        return;
    }
    if (lp->flags & (R_DYNAMIC_VALUE | R_TEMPORAL_VALUE)) {
        for (ITERATE_ITEMS(lp, data, next)) {
            rFree(data);
        }
    }
    lp->length = 0;
}

PUBLIC int rLookupItem(RList *lp, cvoid *item)
{
    int i;

    if (!lp) {
        return R_ERR_BAD_ARGS;
    }
    for (i = 0; i < lp->length; i++) {
        if (lp->items[i] == item) {
            return (int) i;
        }
    }
    return R_ERR_CANT_FIND;
}

PUBLIC int rLookupStringItem(RList *lp, cchar *str)
{
    int i;

    if (!lp) {
        return R_ERR_BAD_ARGS;
    }
    for (i = 0; i < lp->length; i++) {
        if (smatch(lp->items[i], str)) {
            return (int) i;
        }
    }
    return R_ERR_CANT_FIND;
}

/*
    Grow the list to be at least the requested size.
 */
PUBLIC int rGrowList(RList *lp, int size)
{
    void   **newItems;
    size_t memsize;
    int    len;

    /*
        If growing by 1, then use the default increment which exponentially grows.
        Otherwise, assume the caller knows exactly how much the list needs to grow.
     */
    if (size <= (int) lp->capacity) {
        return 0;
    }
    if (size == (int) (lp->capacity + 1)) {
        /*
            Optimization for growing by 1
         */
        if (lp->capacity > (INT_MAX - ME_R_LIST_MIN_SIZE) / 2) {
            return R_ERR_MEMORY;  // Cannot grow safely
        }
        len = ME_R_LIST_MIN_SIZE + (int) (lp->capacity * 2);
    } else {
        len = max(ME_R_LIST_MIN_SIZE, size);
    }
    // Check for overflow in size calculation
    if (len > (int) (INT_MAX / sizeof(void*))) {
        return R_ERR_MEMORY;  // Cannot allocate safely
    }
    memsize = (size_t) len * sizeof(void*);

    newItems = rRealloc(lp->items, memsize);
    if (newItems == NULL) {
        lp->items = NULL;
        lp->capacity = 0;
        return R_ERR_MEMORY;
    }
    lp->items = newItems;
    memset(&lp->items[lp->capacity], 0, ((size_t) (len - lp->capacity)) * sizeof(void*));
    lp->capacity = len;
    return 0;
}

static int defaultSort(char **q1, char **q2, void *ctx)
{
    return scmp(*q1, *q2);
}

PUBLIC RList *rSortList(RList *lp, RSortProc cmp, void *ctx)
{
    if (!lp) {
        return 0;
    }
    if (cmp == 0) {
        cmp = (RSortProc) defaultSort;
    }
    rSort(lp->items, lp->length, sizeof(void*), cmp, ctx);
    return lp;
}

static void swapElt(char *a, char *b, int width)
{
    char tmp;

    if (a == b) {
        return;
    }
    while (width--) {
        tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

/*
    Quicksort
 */
PUBLIC void *rSort(void *base, int nelt, int esize, RSortProc cmp, void *ctx)
{
    char *array, *pivot, *left, *right, *end;

    if (nelt < 2 || esize <= 0) {
        return base;
    }
    if (!cmp) {
        cmp = (RSortProc) defaultSort;
    }
    array = base;
    end = array + (nelt * esize);
    left = array;
    right = array + ((nelt - 1) * esize);
    pivot = array;

    while (left < right) {
        while (left < end && cmp(left, pivot, ctx) <= 0) {
            left += esize;
        }
        while (right > array && cmp(right, pivot, ctx) > 0) {
            right -= esize;
        }
        if (left < right) {
            swapElt(left, right, esize);
        }
    }
    swapElt(pivot, right, esize);
    rSort(array, (int) ((right - array) / esize), esize, cmp, ctx);
    rSort(left, nelt - (int) ((left - array) / esize), esize, cmp, ctx);
    return base;
}

PUBLIC char *rListToString(RList *list, cchar *join)
{
    RBuf *buf;
    char *s;
    int  next;

    if (!join) {
        join = ",";
    }
    if ((buf = rAllocBuf(R_LIST_ALLOC_SIZE)) == 0) {
        return NULL;
    }
    for (ITERATE_ITEMS(list, s, next)) {
        rPutStringToBuf(buf, s);
        rPutStringToBuf(buf, join);
    }
    if (next > 0) {
        rAdjustBufEnd(buf, -(ssize) slen(join));
    }
    return rBufToStringAndFree(buf);
}

PUBLIC void *rPopItem(RList *list)
{
    void *item;

    if ((item = rGetItem(list, 0)) == 0) {
        return 0;
    }
    //  Incase list is dynamic, we need to prevent rRemoveItemAt from freeing.
    list->items[0] = 0;
    rRemoveItemAt(list, 0);
    return item;
}

PUBLIC void rPushItem(RList *list, void *item)
{
    // Note: Errors from rAddItem are silently ignored as this function returns void
    rAddItem(list, item);
}

#endif /* R_USE_LIST */
/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/log.c ************/

/**
    log.c - R Logging

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



#if R_USE_LOG
/********************************** Locals ************************************/

#ifndef ME_R_LOG_COUNT
    #define ME_R_LOG_COUNT 5
#endif
#ifndef ME_R_LOG_SIZE
    #define ME_R_LOG_SIZE  (2 * 1024 * 1024)
#endif

static RLogHandler rLogHandler = rDefaultLogHandler;

static RHash *logTypes;                /* Hash of message types */
static RHash *logSources;              /* Hash of modules sources */
static RBuf  *logBuf;                  /* Log line formation buffer */
static char  *logPath;                 /* Log file name */
static char  *logFormat;               /* Log line format */
static int   logFd = -1;               /* stdout */
static bool  rTimeouts = 1;            /* Enable timeouts */
static bool  sticky = 0;               /* Forced settings are sticky */

static const char *errors[] = {
    "R_ERR_OK",
    "R_ERR_BASE",
    "R_ERR",
    "R_ERR_ABORTED",
    "R_ERR_ALREADY_EXISTS",
    "R_ERR_BAD_ACK",
    "R_ERR_BAD_ARGS",
    "R_ERR_BAD_DATA",
    "R_ERR_BAD_FORMAT",
    "R_ERR_BAD_HANDLE",
    "R_ERR_BAD_NULL",
    "R_ERR_BAD_REQUEST",
    "R_ERR_BAD_RESPONSE",
    "R_ERR_BAD_SESSION",
    "R_ERR_BAD_STATE",
    "R_ERR_BAD_SYNTAX",
    "R_ERR_BAD_TYPE",
    "R_ERR_BAD_VALUE",
    "R_ERR_BUSY",
    "R_ERR_CANT_ACCESS",
    "R_ERR_CANT_ALLOCATE",
    "R_ERR_CANT_COMPLETE",
    "R_ERR_CANT_CONNECT",
    "R_ERR_CANT_CREATE",
    "R_ERR_CANT_DELETE",
    "R_ERR_CANT_FIND",
    "R_ERR_CANT_INITIALIZE",
    "R_ERR_CANT_LOAD",
    "R_ERR_CANT_OPEN",
    "R_ERR_CANT_READ",
    "R_ERR_CANT_WRITE",
    "R_ERR_DELETED",
    "R_ERR_MEMORY",
    "R_ERR_NETWORK",
    "R_ERR_NOT_CONNECTED",
    "R_ERR_NOT_INITIALIZED",
    "R_ERR_NOT_READY",
    "R_ERR_READ_ONLY",
    "R_ERR_TIMEOUT",
    "R_ERR_TOO_MANY",
    "R_ERR_WONT_FIT",
    "R_ERR_WOULD_BLOCK",
    "R_ERR_MAX",
};

/********************************** Forwards **********************************/

static void closeLog(void);
static int  allocLog(void);
static void freeLog(void);
static int  openLog(cchar *path);

/************************************ Code ************************************/

PUBLIC int rInitLog(void)
{
    cchar *filter, *format;
    bool  force;

    force = 0;
    sticky = 0;

    if (!logBuf && allocLog() < 0) {
        return R_ERR_MEMORY;
    }
    if ((filter = getenv("LOG_FILTER")) == 0) {
        filter = R_LOG_FILTER;
    } else {
        force = 1;
    }
    if ((format = getenv("LOG_FORMAT")) == 0) {
        format = R_LOG_FORMAT;
    } else {
        force = 1;
    }
    return rSetLog(filter, format, force);
}

PUBLIC void rTermLog(void)
{
    closeLog();
    freeLog();
}

static int allocLog(void)
{
    if ((logBuf = rAllocBuf(ME_MAX_LOG_LINE)) == 0) {
        return R_ERR_MEMORY;
    }
    return 0;
}

static void freeLog(void)
{
    rFreeBuf(logBuf);
    rFreeHash(logTypes);
    rFreeHash(logSources);
    rFree(logPath);
    rFree(logFormat);
    logBuf = 0;
    logTypes = 0;
    logSources = 0;
    logPath = 0;
    logFormat = 0;
}

/*
    This can be called before rInit()
 */
PUBLIC int rSetLog(cchar *path, cchar *format, bool force)
{
    char *filter, *sources, *types;
    char localPath[ME_MAX_FNAME];

    if (sticky && !force) {
        //  Silently ignore because command line has overridden
        return 0;
    }
    if (path == 0 || strcmp(path, "none") == 0) {
        return 0;
    }
    if (!logBuf) {
        if (allocLog() < 0) {
            return R_ERR_MEMORY;
        }
    }
    scopy(localPath, sizeof(localPath), path);
    stok(localPath, ":", &filter);

    if (filter) {
        types = stok(filter, ":", &sources);
        if (types && !sources) {
            sources = "all,!mbedtls";
        }
        rSetLogFilter(types, sources, force);
    }
    if (rSetLogPath(localPath, force) < 0) {
        return R_ERR_CANT_OPEN;
    }
    rSetLogFormat(format, force);
    if (force) {
        sticky = 1;
    }
    return 0;
}

PUBLIC void rSetLogFormat(cchar *format, bool force)
{
    if (sticky && !force) {
        //  Silently ignore because command line has overridden
        return;
    }
    if (format) {
        rFree(logFormat);
        logFormat = sclone(format);
    } else if (!logFormat) {
        logFormat = sclone(R_LOG_FORMAT);
    }
}

PUBLIC int rSetLogPath(cchar *path, bool force)
{
    if (sticky && !force) {
        //  Silently ignore because command line has overridden
        return 0;
    }
    closeLog();
    rFree(logPath);
    logPath = 0;

    if (path) {
        if (strcmp(path, "stdout") == 0) {
            logFd = 1;
        } else if (strcmp(path, "stderr") == 0) {
#if FREERTOS
            //  No stderr
            logFd = 1;
#else
            logFd = 2;
#endif
        } else if (openLog(path) < 0) {
            return R_ERR_CANT_OPEN;
        }
        logPath = sclone(path);
    }
    if (force) {
        sticky = 1;
    }
    return 0;
}

PUBLIC bool rIsLogSet(void)
{
    return logPath ? 1 : 0;
}

/*
    Define a new filter set of types and sources
 */
PUBLIC void rSetLogFilter(cchar *types, cchar *sources, bool force)
{
    char *buf, *enable, *source, *next, *seps, *type;

    if (!types && !sources) {
        return;
    }
    if (sticky && !force) {
        //  Silently ignore
        return;
    }
    /*
        Recreate new sets and define default values for debug, error, info, log
     */
    rFreeHash(logTypes);
    rFreeHash(logSources);
    logTypes = rAllocHash(0, R_HASH_CASELESS);
    logSources = rAllocHash(0, R_HASH_CASELESS);
    if (!logTypes || !logSources) {
        return;
    }
    rAddName(logTypes, "raw", "1", R_STATIC_NAME | R_STATIC_VALUE);

    seps = "[], \"\t";
    buf = sclone(types);

    next = 0;
    type = stok(buf, seps, &next);
    while (type) {
        enable = "1";
        if (*type == '!') {
            enable = "0";
            type++;
        }
        if (!rAddName(logTypes, type, enable, R_TEMPORAL_NAME | R_STATIC_VALUE)) {
            break;
        }
        type = stok(NULL, seps, &next);
    }
    rFree(buf);

    buf = sclone(sources);
    next = 0;
    source = stok(buf, seps, &next);
    while (source) {
        enable = "1";
        if (*source == '!') {
            enable = "0";
            source++;
        }
        if (!rAddName(logSources, source, enable, R_TEMPORAL_NAME | R_STATIC_VALUE)) {
            break;
        }
        source = stok(NULL, seps, &next);
    }
    rFree(buf);
    if (force) {
        sticky = 1;
    }
}

static int openLog(cchar *path)
{
    int prior;

    prior = logFd;
    if ((logFd = open(path, O_APPEND | O_CREAT | O_WRONLY | O_TEXT, 0600)) < 0) {
        logFd = prior;
        rError("runtime", "Cannot open log file %s, errno=%d", path, rGetOsError());
        return R_ERR_CANT_OPEN;
    }
#if VXWORKS
    // VxWorks does not implement O_APPEND
    lseek(logFd, 0, SEEK_END);
#endif
    return 0;
}

static void closeLog(void)
{
    if (logFd > 2) {
        close(logFd);
        logFd = -1;
    }
}

/*
    Test if a given type and source are enabled. If not defined, check if "all" is defined.
 */
PUBLIC bool rEmitLog(cchar *type, cchar *source)
{
    cchar *enable;

    if (!type) {
        type = "info";
    }
    if (!source) {
        source = "app";
    }
    if (logTypes) {
        if ((enable = rLookupName(logTypes, type)) == 0) {
            if ((enable = rLookupName(logTypes, "all")) == 0) {
                return 0;
            }
        }
        if (*enable != '1') {
            return 0;
        }
    }
    if (logSources) {
        if ((enable = rLookupName(logSources, source)) == 0) {
            if ((enable = rLookupName(logSources, "all")) == 0) {
                return 0;
            }
        }
        if (*enable != '1') {
            return 0;
        }
    }
    return 1;
}

PUBLIC RBuf *rFormatLog(RBuf *buf, cchar *type, cchar *source, cchar *msg)
{
    static char host[256] = { 0 };
    static int  pid = 0;
    char        *data;
    cchar       *cp, *name;

    name = rGetAppName();
    rFlushBuf(buf);

    if (smatch(type, "raw")) {
        rPutStringToBuf(buf, msg);

    } else {
        for (cp = logFormat; cp && *cp; cp++) {
            if (*cp != '%') {
                rPutCharToBuf(buf, *cp);
                continue;
            }
            switch (*++cp) {
            case 'A':
                rPutStringToBuf(buf, name);
                break;
            case 'C':
                //  Clock ticks
                rPutIntToBuf(buf, (int64) rGetTicks());
                break;
            case 'D':
                data = rFormatLocalTime(R_SYSLOG_DATE, rGetTime());
                rPutStringToBuf(buf, data);
                rFree(data);
                break;
            case 'S':   /* Alias for source */
                rPutStringToBuf(buf, source);
                break;
            case 'H':
                if (host[0] == '\0') {
                    if (gethostname(host, sizeof(host)) != 0) {
                        host[0] = '\0';
                    }
                    host[sizeof(host) - 1] = '\0';
                }
                rPutStringToBuf(buf, host);
                break;
            case 'M':
                rPutStringToBuf(buf, msg);
                if (*msg && msg[slen(msg) - 1] != '\n') {
                    rPutCharToBuf(buf, '\n');
                }
                break;
            case 'P':
                if (!pid) {
                    pid = getpid();
                }
                rPutIntToBuf(buf, pid);
                break;
            case 'T':
                rPutStringToBuf(buf, type);
                break;
            case '\0':
                break;
            default:
                rPutCharToBuf(buf, *cp);
                break;
            }
        }
    }
    return buf;
}

PUBLIC void rBackupLog(void)
{
    struct stat info;

    if (logFd > 2 && fstat(logFd, &info) == 0 && info.st_size >= ME_R_LOG_SIZE) {
        closeLog();
        rBackupFile(logPath, ME_R_LOG_COUNT);
        if (openLog(logPath) < 0) {
            // Failed to reopen log file after backup
            logFd = 2;  // Fall back to stderr
        }
    }
}

PUBLIC void rDefaultLogHandler(cchar *type, cchar *source, cchar *msg)
{
    rFormatLog(logBuf, type, source, msg);
    msg = rBufToString(logBuf);
    if (logFd > 1) {
        write(logFd, msg, (uint) rGetBufLength(logBuf));
    } else {
        rPrintf("%s", rBufToString(logBuf));
    }
#if ME_DEBUG
    if (smatch(type, "error") || smatch(type, "fatal")) {
        rBreakpoint();
    }
#endif
}

PUBLIC void rLogConfig(void)
{
    rTrace("app", ME_TITLE " Configuration");
    rTrace("app", "---------------------------");
#ifdef ME_VERSION
    rTrace("app", "Version:   %s", ME_VERSION);
#endif
    rTrace("app", "BuildType: %s", ME_DEBUG ? "Debug" : "Release");
#ifdef ME_CPU
    rTrace("app", "CPU:       %s", ME_CPU);
#endif
#ifdef ME_OS
    rTrace("app", "OS:        %s", ME_OS);
#endif
#ifdef ME_CONFIG_CMD
    rTrace("app", "Configure: %s", ME_CONFIG_CMD);
#endif
    rTrace("app", "---------------------------");
}

PUBLIC void rBreakpoint(void)
{
#if DEBUG_PAUSE
    {
        static int paused = 1;
        int        i;
        rPrintf("Paused to permit debugger to attach - will awake in 2 minutes\n");
        fflush(stdout);
        for (i = 0; i < 120 && paused; i++) {
            rSleep(1000);
        }
    }
#endif
}

PUBLIC void rLog(cchar *type, cchar *source, cchar *fmt, ...)
{
    char *buf;

    if (rEmitLog(type, source)) {
        va_list args;
        va_start(args, fmt);
        if (rVsaprintf(&buf, 0, fmt, args) >= 0) {
            (rLogHandler) (type, source, buf);
            rFree(buf);
        }
        va_end(args);
    }
}

PUBLIC void rLogv(cchar *type, cchar *source, cchar *fmt, va_list args)
{
    char *buf;

    if (rEmitLog(type, source)) {
        if (rVsaprintf(&buf, 0, fmt, args) >= 0) {
            (rLogHandler) (type, source, buf);
            rFree(buf);
        }
    }
}

/*
    Emit a log message to create an AWS metric via EMT log format
 */
PUBLIC void rMetrics(cchar *message, cchar *namespace, cchar *dimensions, cchar *values, ...)
{
    RBuf    *buf;
    va_list args;
    cchar   *key, *type, *value;
    int64   i64value;
    int     ivalue;

    if ((buf = rAllocBuf(1024)) == 0) {
        return;
    }
    rPutToBuf(buf,
              "%s\n\
        _aws: {\n\
            Timestamp: %lld,\n\
            CloudWatchMetrics: [{\n\
                Dimensions: [dimensions],\n\
                Namespace: %s,\n",
              message,
              rGetTime(), namespace);

    if (dimensions) {
        rPutToBuf(buf, "Dimensions: [[%s]]\n,", dimensions);
    }
    rPutToBuf(buf, "Metrics: [");

    va_start(args, values);
    do {
        key = va_arg(args, cchar*);
        (void) va_arg(args, cchar*);
        value = va_arg(args, void*);
        rPutToBuf(buf, "{\"Name\": \"%s\"},", key);
    } while (key && value);
    va_end(args);
    if (rGetBufLength(buf) > 0) {
        rAdjustBufEnd(buf, -1);
    }

    rPutStringToBuf(buf, "]}]},\n");

    va_start(args, values);
    do {
        key = va_arg(args, cchar*);
        type = va_arg(args, cchar*);
        if (smatch(type, "int")) {
            ivalue = va_arg(args, int);
            rPutToBuf(buf, "\"%s\": %d", key, ivalue);
            value = NULL;

        } else if (smatch(type, "int64")) {
            i64value = va_arg(args, int64);
            rPutToBuf(buf, "\"%s\": %lld", key, i64value);
            value = NULL;

        } else if (smatch(type, "boolean")) {
            value = va_arg(args, cchar*);
            rPutToBuf(buf, "\"%s\": %s", key, value);
        } else {
            value = va_arg(args, cchar*);
            rPutToBuf(buf, "\"%s\": \"%s\"", key, value);
        }
    } while (key && type);
    if (rGetBufLength(buf) > 0) {
        rAdjustBufEnd(buf, -1);
    }
    rPutStringToBuf(buf, "}\n");

    write(logFd, rBufToString(buf), (uint) rGetBufLength(buf));
    rFreeBuf(buf);
}

PUBLIC void rAssert(cchar *loc, cchar *msg)
{
    rBreakpoint();
#if ME_R_DEBUG_LOGGING
    if (loc) {
#if ME_UNIX_LIKE
        char path[ME_MAX_FNAME];
        scopy(path, sizeof(path), loc);
        loc = basename(path);
#endif
        rLog("error", "assert", "Assertion %s, failed at %s", msg, loc);
    } else {
        rLog("error", "assert", "Assertion %s", msg);
    }
#if ME_DEBUG_WATSON
    rPrintf("Pause for debugger to attach\n");
    rSleep(24 * 3600 * TPS);
#endif
#endif
}

PUBLIC cchar *rGetError(int rc)
{
    if (rc < 0 || rc >= (int) (sizeof(errors) / sizeof(char*))) {
        return "Unknown error";
    }
    return errors[rc];
}
/*
    Return the raw O/S error code
 */
PUBLIC int rGetRawOsError(void)
{
#if ME_WIN_LIKE
    int rc;
    rc = GetLastError();
    if (rc == ERROR_NO_DATA) {
        return EPIPE;
    }
    return rc;
#elif ME_UNIX_LIKE || VXWORKS || ESP32 || ME_SIMULATED
    return errno;
#else
    return 0;
#endif
}

PUBLIC void rSetOsError(int error)
{
#if ME_WIN_LIKE
    SetLastError(error);
#elif ME_UNIX_LIKE || VXWORKS || ESP32 || ME_SIMULATED
    errno = error;
#endif
}

/*
    Return the mapped (portable, Posix) error code
 */
PUBLIC int rGetOsError(void)
{
#if !ME_WIN_LIKE
    return rGetRawOsError();
#else /* ME_WIN_LIKE */
    int err = rGetRawOsError();
    switch (err) {
    case ERROR_SUCCESS:
        return 0;
    case ERROR_FILE_NOT_FOUND:
        return ENOENT;
    case ERROR_ACCESS_DENIED:
        return EPERM;
    case ERROR_INVALID_HANDLE:
        return EBADF;
    case ERROR_NOT_ENOUGH_MEMORY:
        return ENOMEM;
    case ERROR_PATH_BUSY:
    case ERROR_BUSY_DRIVE:
    case ERROR_NETWORK_BUSY:
    case ERROR_PIPE_BUSY:
    case ERROR_BUSY:
        return EBUSY;
    case ERROR_FILE_EXISTS:
        return EEXIST;
    case ERROR_BAD_PATHNAME:
    case ERROR_BAD_ARGUMENTS:
        return EINVAL;
    case WSAENOTSOCK:
        return ENOENT;
    case WSAEINTR:
        return EINTR;
    case WSAEBADF:
        return EBADF;
    case WSAEACCES:
        return EACCES;
    case WSAEINPROGRESS:
        return EINPROGRESS;
    case WSAEALREADY:
        return EALREADY;
    case WSAEADDRINUSE:
        return EADDRINUSE;
    case WSAEADDRNOTAVAIL:
        return EADDRNOTAVAIL;
    case WSAENETDOWN:
        return ENETDOWN;
    case WSAENETUNREACH:
        return ENETUNREACH;
    case WSAECONNABORTED:
        return ECONNABORTED;
    case WSAECONNRESET:
        return ECONNRESET;
    case WSAECONNREFUSED:
        return ECONNREFUSED;
    case WSAEWOULDBLOCK:
        return EAGAIN;
    }
    return R_ERR;
#endif
}

#if KEEP
/*
    Set the mapped (portable, Posix) error code
 */
PUBLIC void rSetError(error)
{
#if !ME_WIN_LIKE
    rSetOsError(error);
    return;
#else /* ME_WIN_LIKE */
    switch (error) {
    case ENOENT:
        error = ERROR_FILE_NOT_FOUND;
        break;

    case EPERM:
        error = ERROR_ACCESS_DENIED;
        break;

    case EBADF:
        error = ERROR_INVALID_HANDLE;
        break;

    case ENOMEM:
        error = ERROR_NOT_ENOUGH_MEMORY;
        break;

    case EBUSY:
        error = ERROR_BUSY;
        break;

    case EEXIST:
        error = ERROR_FILE_EXISTS;
        break;

    case EINVAL:
        error = ERROR_BAD_ARGUMENTS;
        break;

    case EINTR:
        error = WSAEINTR;
        break;

    case EACCES:
        error = WSAEACCES;
        break;

    case EINPROGRESS:
        error = WSAEINPROGRESS;
        break;

    case EALREADY:
        error = WSAEALREADY;
        break;

    case EADDRINUSE:
        error = WSAEADDRINUSE;
        break;

    case EADDRNOTAVAIL:
        error = WSAEADDRNOTAVAIL;
        break;

    case ENETDOWN:
        error = WSAENETDOWN;
        break;

    case ENETUNREACH:
        error = WSAENETUNREACH;
        break;

    case ECONNABORTED:
        error = WSAECONNABORTED;
        break;

    case ECONNRESET:
        error = WSAECONNRESET;
        break;

    case ECONNREFUSED:
        error = WSAECONNREFUSED;
        break;

    case EAGAIN:
        error = WSAEWOULDBLOCK;
        break;
    }
    rSetOsError(error);
#endif
}
#endif

PUBLIC RLogHandler rGetLogHandler(void)
{
    return rLogHandler;
}

PUBLIC int rGetLogFile(void)
{
    return logFd;
}

PUBLIC RLogHandler rSetLogHandler(RLogHandler handler)
{
    RLogHandler priorHandler;

    priorHandler = rLogHandler;
    rLogHandler = handler;
    return priorHandler;
}

PUBLIC void rSetTimeouts(bool on)
{
    rTimeouts = on;
}

PUBLIC bool rGetTimeouts(void)
{
    return rTimeouts;
}

#if ME_R_PRINT
PUBLIC void print(cchar *fmt, ...)
{
    va_list ap;
    char    *buf;

    va_start(ap, fmt);
    buf = sfmtv(fmt, ap);
    va_end(ap);
    rPrintf("%s\n", buf);
    rFree(buf);
}

PUBLIC void dump(cchar *msg, uchar *data, size_t len)
{
    size_t i;

    rPrintf("%s ", msg);
    for (i = 0; i < len; i++) {
        rPrintf("%02X ", data[i]);
    }
    rPrintf("\n");
}
#endif /* ME_R_PRINT */

#endif /* R_USE_LOG */
/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/mbedtls.c ************/

/**
    mbedtls.c - Transport Layer Security for mbedTLS

    To build MbedTLS, use:
        git checkout RELEASE-TAG
        cmake -DCMAKE_BUILD_TYPE=Debug .
        make VERBOSE=1

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



#if R_USE_TLS
#if ME_COM_MBEDTLS

    #if defined(MBEDTLS_CONFIG_FILE)
        #include MBEDTLS_CONFIG_FILE
    #else
        #include "mbedtls/mbedtls_config.h"
    #endif
    #include "mbedtls/ssl.h"
    #include "mbedtls/ssl_cache.h"
    #include "mbedtls/ssl_ticket.h"
    #include "mbedtls/ctr_drbg.h"
    #include "mbedtls/net_sockets.h"
    #include "psa/crypto.h"
    #include "mbedtls/debug.h"
    #include "mbedtls/error.h"
    #include "mbedtls/check_config.h"

/*********************************** Locals ***********************************/

#define R_MAX_CERT_SIZE                 (512 * 1024)

#ifndef MBEDTLS_SSL_MAX_CONTENT_LEN
    #define MBEDTLS_SSL_MAX_CONTENT_LEN 8192
#endif

typedef struct Rtls {
    RSocket *sock;                         /* Owning socket */
    Socket fd;                             /* Socket file descriptor */
    RList *alpnList;                       /* ALPN protocols as a list */
    char *alpn;                            /* ALPN protocols */
    char *caFile;                          /* Certificate verification file or bundle */
    char *certFile;                        /* Certificate filename */
    char *keyFile;                         /* Alternatively, locate the key in a file */
    char *revokeFile;                      /* Certificate revocation list */
    char *ciphers;                         /* Ciphers to use for connection */
    int *cipherSuite;                      /* Ciphersuite codes */
    uint connected : 1;                    /* Connection established */
    uint configured : 1;                   /* TLS configured -- requires a free */
    uint server : 1;
    int verifyPeer : 2;                    /* Verify the peer certificate */
    int verifyIssuer : 2;                  /* Verify issuer of peer cer. Set to 0 to permit self signed cers */
    mbedtls_ssl_context ctx;               /* SSL state */
    mbedtls_ssl_config conf;               /* SSL configuration */
    mbedtls_x509_crt ca;                   /* Certificate authority bundle to verify peer */
    mbedtls_x509_crt cert;                 /* Certificate (own) */
    mbedtls_x509_crl revoke;               /* Certificate revoke list */
    mbedtls_pk_context key;                /* Private key */
} Rtls;

static mbedtls_ssl_cache_context  cache;   /* Session cache context */
static mbedtls_ctr_drbg_context   ctr;     /* Counter random generator state */
static mbedtls_ssl_ticket_context tickets; /* Session tickets */
static mbedtls_entropy_context    entropy; /* Entropy context */

static char *defaultAlpn;                  /* Default ALPN protocols */
static char *defaultCaFile;                /* Default certificate verification cer file or bundle */
static char *defaultCertFile;              /* Default certificate filename */
static char *defaultKeyFile;               /* Default Alternatively, locate the key in a file */
static char *defaultRevokeFile;            /* Default certificate revocation list */
static char *defaultCiphers;               /* Default Ciphers to use for connection */

static int defaultVerifyPeer = 1;          /* Verify peer certificates */
static int defaultVerifyIssuer = 1;        /* Verify issuer of peer certificates */

/********************************** Forwards **********************************/

static int *getCipherSuite(char *ciphers);
static int handshake(Rtls *tp, Ticks deadline);
static int parseCert(Rtls *tp, mbedtls_x509_crt *cert, cchar *path);
static int parseKey(Rtls *tp, mbedtls_pk_context *key, cchar *path);
static int parseRevoke(Rtls *tp, mbedtls_x509_crl *crl, cchar *path);
static char *replaceHyphen(char *cipher, char from, char to);
static void logCiphers(Rtls *tp);
static void logMbedtls(void *context, int level, cchar *file, int line, cchar *str);

/************************************ Code ************************************/

PUBLIC int rInitTls(void)
{
    int rc;

    psa_crypto_init();
    mbedtls_ssl_cache_init(&cache);
    mbedtls_ctr_drbg_init(&ctr);
    mbedtls_ssl_ticket_init(&tickets);
    mbedtls_entropy_init(&entropy);

#if !defined(ESP32)
    mbedtls_debug_set_threshold(6);
#endif

    if ((rc = mbedtls_ctr_drbg_seed(&ctr, mbedtls_entropy_func, &entropy, 0, 0)) < 0) {
        rError("runtime", "Cannot seed TLS rng");
        return R_ERR_CANT_INITIALIZE;
    }
    return 0;
}

PUBLIC void rTermTls(void)
{
    mbedtls_ctr_drbg_free(&ctr);
    mbedtls_ssl_cache_free(&cache);
    mbedtls_ssl_ticket_free(&tickets);
    mbedtls_entropy_free(&entropy);
    mbedtls_psa_crypto_free();
    rFree(defaultAlpn);
    rFree(defaultCaFile);
    rFree(defaultCertFile);
    rFree(defaultCiphers);
    rFree(defaultKeyFile);
    rFree(defaultRevokeFile);
    defaultAlpn = 0;
    defaultCaFile = 0;
    defaultCertFile = 0;
    defaultCiphers = 0;
    defaultKeyFile = 0;
    defaultRevokeFile = 0;
}

PUBLIC Rtls *rAllocTls(RSocket *sock)
{
    Rtls *tp;

    if ((tp = rAllocType(Rtls)) == 0) {
        return 0;
    }
    tp->sock = sock;
    tp->verifyPeer = -1;
    tp->verifyIssuer = -1;
    return tp;
}

PUBLIC void rFreeTls(Rtls *tp)
{
    if (!tp) {
        return;
    }
    rFreeList(tp->alpnList);
    rFree(tp->alpn);
    rFree(tp->certFile);
    rFree(tp->caFile);
    rFree(tp->ciphers);
    rFree(tp->cipherSuite);
    rFree(tp->keyFile);
    rFree(tp->revokeFile);
    mbedtls_pk_free(&tp->key);
    mbedtls_x509_crt_free(&tp->cert);
    mbedtls_x509_crt_free(&tp->ca);
    mbedtls_x509_crl_free(&tp->revoke);
    if (tp->configured) {
        mbedtls_ssl_config_free(&tp->conf);
    }
    mbedtls_ssl_free(&tp->ctx);
    rFree(tp);
}

PUBLIC void rCloseTls(Rtls *tp)
{
    if (tp && tp->fd != INVALID_SOCKET) {
        mbedtls_ssl_close_notify(&tp->ctx);
        mbedtls_ssl_free(&tp->ctx);
    }
}

PUBLIC int rConfigTls(Rtls *tp, bool server)
{
    RSocketCustom custom;
    char          *alpn, *last, *token;
    int           flags, rc;

    if (tp->configured) {
        return 0;
    }
    tp->server = server;
    tp->configured = 1;

    mbedtls_ssl_config_init(&tp->conf);
    mbedtls_pk_init(&tp->key);
    mbedtls_x509_crt_init(&tp->cert);
    mbedtls_ssl_conf_dbg(&tp->conf, logMbedtls, NULL);

    if (tp->verifyIssuer < 0) {
        tp->verifyIssuer = defaultVerifyIssuer;
    }
    if (tp->verifyPeer < 0) {
        tp->verifyPeer = defaultVerifyPeer;
    }
    tp->alpn = tp->alpn ? tp->alpn : scloneNull(defaultAlpn);
    tp->caFile = tp->caFile ? tp->caFile : (server ? 0 : scloneNull(defaultCaFile));
    tp->certFile = tp->certFile ? tp->certFile : scloneNull(defaultCertFile);
    tp->keyFile = tp->keyFile ? tp->keyFile : scloneNull(defaultKeyFile);
    tp->revokeFile = tp->revokeFile ? tp->revokeFile : scloneNull(defaultRevokeFile);
    tp->ciphers = tp->ciphers ? tp->ciphers : scloneNull(defaultCiphers);

    if (tp->certFile) {
        if (parseCert(tp, &tp->cert, tp->certFile) != 0) {
            return R_ERR_CANT_INITIALIZE;
        }
        if (!tp->keyFile) {
            // Can include the private key with the cert file
            tp->keyFile = tp->certFile;
        }
    }
    if (tp->keyFile) {
        if (parseKey(tp, &tp->key, tp->keyFile) != 0) {
            return R_ERR_CANT_INITIALIZE;
        }
    }
    if (tp->caFile) {
        if (parseCert(tp, &tp->ca, tp->caFile) != 0) {
            return R_ERR_CANT_INITIALIZE;
        }
    }
    if (tp->revokeFile) {
        if (parseRevoke(tp, &tp->revoke, tp->revokeFile) != 0) {
            return R_ERR_CANT_INITIALIZE;
        }
    }
    if ((rc = mbedtls_ssl_config_defaults(&tp->conf,
                                          server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) < 0) {
        rSetSocketError(tp->sock, "Cannot set mbedtls defaults");
        return R_ERR_CANT_INITIALIZE;
    }
#if defined(MBEDTLS_SSL_MAJOR_VERSION_3) && defined(MBEDTLS_SSL_MINOR_VERSION_3)
    // Enforce TLS >= 1.2
    mbedtls_ssl_conf_min_version(&tp->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
#endif
    mbedtls_ssl_conf_rng(&tp->conf, mbedtls_ctr_drbg_random, &ctr);

    /*
        Verify optional means continue with handshake even if certificate verification fails.
        We handle verification here.
     */
    mbedtls_ssl_conf_authmode(&tp->conf,
                              tp->verifyPeer == 1 ? MBEDTLS_SSL_VERIFY_OPTIONAL : MBEDTLS_SSL_VERIFY_NONE);

    if (tp->ciphers) {
        tp->cipherSuite = getCipherSuite(tp->ciphers);
        //  MbedTLS does not store the cipherSuite array -- must persist
        mbedtls_ssl_conf_ciphersuites(&tp->conf, tp->cipherSuite);
    }
    if (tp->keyFile && tp->certFile) {
        if (mbedtls_ssl_conf_own_cert(&tp->conf, &tp->cert, &tp->key) < 0) {
            rSetSocketError(tp->sock, "Cannot define certificate and private key");
            return R_ERR_CANT_INITIALIZE;
        }
    }
    if (tp->caFile || tp->revokeFile) {
        mbedtls_ssl_conf_ca_chain(&tp->conf, tp->caFile ? &tp->ca : NULL,
                                  tp->revokeFile ? &tp->revoke : NULL);
    }
    if (tp->alpn) {
        //  Must be null terminated
        rFreeList(tp->alpnList);
        tp->alpnList = rAllocList(2, R_DYNAMIC_VALUE);
        if (!tp->alpnList) {
            return R_ERR_MEMORY;
        }
        alpn = sclone(tp->alpn);
        for (token = stok(alpn, ", \t", &last); token; token = stok(NULL, ", \t", &last)) {
            if (rAddItem(tp->alpnList, sclone(token)) < 0) {
                rFree(alpn);
                return R_ERR_MEMORY;
            }
        }
        rFree(alpn);
        mbedtls_ssl_conf_alpn_protocols(&tp->conf, (cchar**) tp->alpnList->items);
    }
    if ((custom = rGetSocketCustom()) != NULL) {
        flags = tp->caFile ? R_TLS_HAS_AUTHORITY : 0;
        custom(tp->sock, R_SOCKET_CONFIG_TLS, &tp->conf, flags);
    }
    if (rEmitLog("debug", "mbedtls")) {
        logCiphers(tp);
    }
    return 0;
}

PUBLIC Rtls *rAcceptTls(Rtls *tp, Rtls *listen)
{
    tp->verifyPeer = listen->verifyPeer;
    tp->verifyIssuer = listen->verifyIssuer;
    tp->conf = listen->conf;
    return tp;
}

PUBLIC int rUpgradeTls(Rtls *tp, Socket fd, cchar *peer, Ticks deadline)
{
    tp->fd = fd;
    mbedtls_ssl_init(&tp->ctx);
    mbedtls_ssl_setup(&tp->ctx, &tp->conf);
    mbedtls_ssl_set_bio(&tp->ctx, &tp->fd, mbedtls_net_send, mbedtls_net_recv, 0);

    if (peer && mbedtls_ssl_set_hostname(&tp->ctx, peer) < 0) {
        return R_ERR_BAD_ARGS;
    }
    if (handshake(tp, deadline) < 0) {
        return R_ERR_CANT_INITIALIZE;
    }
    return 0;
}

static int handshake(Rtls *tp, Ticks deadline)
{
    int      mask, rc;
    uint32_t vrc;

    rc = 0;
    mask = R_IO;
    while (rWaitForIO(tp->sock->wait, mask, deadline) >= 0) {
        if ((rc = mbedtls_ssl_handshake(&tp->ctx)) == 0) {
            break;
        }
        mask = 0;
        if (rc == MBEDTLS_ERR_SSL_WANT_READ) {
            mask |= R_READABLE;
        } else if (rc == MBEDTLS_ERR_SSL_WANT_WRITE) {
            mask |= R_WRITABLE;
        } else {
            break;
        }
    }
    if (rc < 0) {
        if (rc == MBEDTLS_ERR_SSL_PRIVATE_KEY_REQUIRED && !(tp->keyFile || tp->certFile)) {
            rSetSocketError(tp->sock, "Peer requires a certificate");
        } else if (rc == MBEDTLS_ERR_SSL_CA_CHAIN_REQUIRED) {
            rSetSocketError(tp->sock, "Server requires a client certificate");
        } else {
            char ebuf[256];
            mbedtls_strerror(-rc, ebuf, sizeof(ebuf));
            rSetSocketError(tp->sock, "Handshake failure: %s: error -0x%x", ebuf, -rc);
        }
        rSetOsError(EPROTO);
        return R_ERR_CANT_CONNECT;
    }
    if ((vrc = mbedtls_ssl_get_verify_result(&tp->ctx)) != 0) {
        if (vrc & MBEDTLS_X509_BADCERT_MISSING) {
            rSetSocketError(tp->sock, "Peer did not supply required certificate");
        }
        if (vrc & MBEDTLS_X509_BADCERT_EXPIRED) {
            rSetSocketError(tp->sock, "Certificate expired");
        } else if (vrc & MBEDTLS_X509_BADCERT_REVOKED) {
            rSetSocketError(tp->sock, "Certificate revoked");
        } else if (vrc & MBEDTLS_X509_BADCERT_CN_MISMATCH) {
            if (tp->verifyPeer) {
                rSetSocketError(tp->sock, "Certificate common name mismatch. Expected %s",
                                mbedtls_ssl_get_hostname(&tp->ctx));
            }
        } else if (vrc & MBEDTLS_X509_BADCERT_KEY_USAGE || vrc & MBEDTLS_X509_BADCERT_EXT_KEY_USAGE) {
            rSetSocketError(tp->sock, "Unauthorized key use in certificate");
        } else if (vrc & MBEDTLS_X509_BADCERT_NOT_TRUSTED) {
            if (tp->verifyIssuer != 1) {
                vrc = 0;
            } else {
                rSetSocketError(tp->sock, "Certificate not trusted");
            }
        } else if (vrc & MBEDTLS_X509_BADCERT_SKIP_VERIFY) {
            vrc = 0;
        } else {
            if (rc == MBEDTLS_ERR_NET_CONN_RESET) {
                rSetSocketError(tp->sock, "Peer disconnected");
            } else {
                char ebuf[256];
                mbedtls_x509_crt_verify_info(ebuf, sizeof(ebuf), "", vrc);
                strim(ebuf, "\n", 0);
                rSetSocketError(tp->sock, "Cannot handshake: %s, error -0x%x", ebuf, -rc);
            }
        }
    }
    if (vrc != 0 && tp->verifyPeer == 1) {
        if (mbedtls_ssl_get_peer_cert(&tp->ctx) == 0) {
            rSetSocketError(tp->sock, "Peer did not provide a certificate");
        }
        rSetOsError(EPROTO);
        return R_ERR_CANT_READ;
    }
    tp->connected = 1;
    rDebug("tls", "Handshake with %s and %s", mbedtls_ssl_get_version(&tp->ctx), mbedtls_ssl_get_ciphersuite(&tp->ctx));
    return 1;
}

PUBLIC bool rIsTlsConnected(Rtls *tp)
{
    return tp->connected;
}

PUBLIC ssize rReadTls(Rtls *tp, void *buf, size_t len)
{
    int    rc;
    size_t toRead;

    if (tp->fd == INVALID_SOCKET) {
        return R_ERR_CANT_READ;
    }
    while (1) {
        toRead = (len > (ssize) MBEDTLS_SSL_MAX_CONTENT_LEN) ? MBEDTLS_SSL_MAX_CONTENT_LEN : (size_t) len;
        rc = mbedtls_ssl_read(&tp->ctx, buf, toRead);
        if (rc < 0) {
            if (rc == MBEDTLS_ERR_SSL_WANT_READ ||
                rc == MBEDTLS_ERR_SSL_WANT_WRITE ||
                rc == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET ||
                rc == MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS ||
                rc == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS) {
                rc = 0;
                break;
            } else if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                return R_ERR_CANT_READ;
            } else {
                rDebug("tls", "readSSL: error -0x%x", -rc);
                return R_ERR_CANT_READ;
            }
        } else if (rc == 0) {
            return R_ERR_CANT_READ;
        }
        break;
    }
    return rc;
}

/*
    Write data. Return the number of bytes written or -1 on errors or socket closure.
 */
PUBLIC ssize rWriteTls(Rtls *tp, cvoid *buf, size_t len)
{
    ssize  totalWritten;
    int    rc;
    size_t toWrite;

    if (len <= 0) {
        return R_ERR_BAD_ARGS;
    }
    totalWritten = 0;
    rc = 0;
    do {
        toWrite = (len > (ssize) MBEDTLS_SSL_MAX_CONTENT_LEN) ? MBEDTLS_SSL_MAX_CONTENT_LEN : (size_t) len;
        rc = mbedtls_ssl_write(&tp->ctx, (uchar*) buf, toWrite);
        if (rc <= 0) {
            if (rc == MBEDTLS_ERR_SSL_WANT_READ ||
                rc == MBEDTLS_ERR_SSL_WANT_WRITE ||
                rc == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET ||
                rc == MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS ||
                rc == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS) {
                break;
            }
            if (rc == MBEDTLS_ERR_NET_CONN_RESET) {
                return R_ERR_CANT_WRITE;
            } else {
                rDebug("tls", "ssl_write failed rc -0x%x", -rc);
                return R_ERR_CANT_WRITE;
            }
        } else {
            totalWritten += rc;
            buf = (void*) ((char*) buf + rc);
            len -= (size_t) rc;
        }
    } while (len > 0);

    if (totalWritten == 0 && (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE)) {
        rSetOsError(EAGAIN);
    }
    return totalWritten;
}

/*
    Convert string of IANA ciphers into a list of mbedtls cipher codes
 */
static int *getCipherSuite(char *ciphers)
{
    char *cipher, *next;
    cint *cp;
    int  nciphers, i, *result, code;

    if (!ciphers || *ciphers == 0) {
        return 0;
    }
    /*
        Get all ciphers supported by MbedTLS
     */
    for (nciphers = 0, cp = mbedtls_ssl_list_ciphersuites(); cp && *cp; cp++, nciphers++) {
    }

    if (nciphers + 1 > MAXINT) {
        rError("runtime", "mbedtls getCipherSuite integer overflow");
        return NULL;
    }
    result = rAlloc((uint) (nciphers + 1) * sizeof(int));

    /*
        Locate required cipher and convert to an MbedTLS code
     */
    next = ciphers = sclone(ciphers);
    for (i = 0; (cipher = stok(next, ":, \t", &next)) != 0; ) {
        replaceHyphen(cipher, '_', '-');
        if ((code = mbedtls_ssl_get_ciphersuite_id(cipher)) <= 0) {
            cipher = sreplace(cipher, "TLS", "TLS1-3");
            if ((code = mbedtls_ssl_get_ciphersuite_id(cipher)) <= 0) {
                rError("mqtt", "Unsupported cipher \"%s\"", cipher);
                rFree(cipher);
                continue;
            }
            rFree(cipher);
        }
        result[i++] = code;
    }
    rFree(ciphers);
    result[i] = 0;
    return result;
}

static char *replaceHyphen(char *cipher, char from, char to)
{
    char *cp;

    for (cp = cipher; *cp; cp++) {
        if (*cp == from) {
            *cp = to;
        }
    }
    return cipher;
}

static int parseCert(Rtls *tp, mbedtls_x509_crt *cert, cchar *path)
{
    uchar  *buf, *cp;
    size_t len;

    if (path[0] == '@') {
        len = slen(&path[1]);
        cp = (uchar*) &path[1];
        buf = 0;
    } else {
        if (rGetFileSize(path) > R_MAX_CERT_SIZE) {
            rSetSocketError(tp->sock, "Certificate file is too large %s", path);
            return R_ERR_CANT_INITIALIZE;
        }
        if ((buf = (uchar*) rReadFile(path, &len)) == 0) {
            rSetSocketError(tp->sock, "Unable to read certificate %s", path);
            return R_ERR_CANT_INITIALIZE;
        }
        cp = buf;
    }
    if (scontains((char*) cp, "-----BEGIN ")) {
        /* Looks PEM encoded so count the null in the length */
        len++;
    }
    if (mbedtls_x509_crt_parse(cert, cp, len) != 0) {
        rSetSocketError(tp->sock, "Unable to parse certificate %s", path);
        if (buf) {
            memset(buf, 0, len);
            rFree(buf);
        }
        return R_ERR_CANT_INITIALIZE;
    }
    if (buf) {
        memset(buf, 0, len);
        rFree(buf);
    }
    return 0;
}

static int parseKey(Rtls *tp, mbedtls_pk_context *key, cchar *path)
{
    uchar  *buf, *cp;
    size_t len;

    if (path[0] == '@') {
        len = slen(&path[1]);
        cp = (uchar*) &path[1];
        buf = 0;
    } else {
        if (rGetFileSize(path) > R_MAX_CERT_SIZE) {
            rSetSocketError(tp->sock, "Key file is too large %s", path);
            return R_ERR_CANT_INITIALIZE;
        }
        if ((buf = (uchar*) rReadFile(path, &len)) == 0) {
            rSetSocketError(tp->sock, "Unable to read key %s", path);
            return R_ERR_CANT_INITIALIZE;
        }
        cp = (uchar*) buf;
    }
    if (scontains((char*) cp, "-----BEGIN ")) {
        len++;
    }
    if (mbedtls_pk_parse_key(key, cp, len, NULL, 0, mbedtls_ctr_drbg_random, &ctr) != 0) {
        rSetSocketError(tp->sock, "Unable to parse key %s", path);
        if (buf) {
            memset(buf, 0, len);
            rFree(buf);
        }
        return R_ERR_CANT_INITIALIZE;
    }
    if (buf) {
        memset(buf, 0, len);
        rFree(buf);
    }
    return 0;
}

static int parseRevoke(Rtls *tp, mbedtls_x509_crl *crl, cchar *path)
{
    uchar  *buf;
    size_t len;

    if (rGetFileSize(path) > R_MAX_CERT_SIZE) {
        rSetSocketError(tp->sock, "CRL file is too large %s", path);
        return R_ERR_CANT_INITIALIZE;
    }
    if ((buf = (uchar*) rReadFile(path, &len)) == 0) {
        rSetSocketError(tp->sock, "Unable to read crl %s", path);
        return R_ERR_CANT_INITIALIZE;
    }
    if (sstarts((char*) buf, "-----BEGIN ")) {
        len++;
    }
    if (mbedtls_x509_crl_parse(crl, buf, len) != 0) {
        memset(buf, 0, len);
        rSetSocketError(tp->sock, "Unable to parse crl %s", path);
        rFree(buf);
        return R_ERR_CANT_INITIALIZE;
    }
    memset(buf, 0, len);
    rFree(buf);
    return 0;
}

PUBLIC void rSetTlsCerts(Rtls *tp, cchar *ca, cchar *key, cchar *cert, cchar *revoke)
{
    if (ca) {
        rFree(tp->caFile);
        tp->caFile = sclone(ca);
    }
    if (key) {
        rFree(tp->keyFile);
        tp->keyFile = sclone(key);
    }
    if (cert) {
        rFree(tp->certFile);
        tp->certFile = sclone(cert);
    }
    if (revoke) {
        rFree(tp->revokeFile);
        tp->revokeFile = sclone(revoke);
    }
}

PUBLIC void rSetTlsDefaultCerts(cchar *ca, cchar *key, cchar *cert, cchar *revoke)
{
    if (ca) {
        rFree(defaultCaFile);
        defaultCaFile = sclone(ca);
    }
    if (key) {
        rFree(defaultKeyFile);
        defaultKeyFile = sclone(key);
    }
    if (cert) {
        rFree(defaultCertFile);
        defaultCertFile = sclone(cert);
    }
    if (revoke) {
        rFree(defaultRevokeFile);
        defaultRevokeFile = sclone(revoke);
    }
}

PUBLIC void rSetTlsCiphers(Rtls *tp, cchar *ciphers)
{
    rFree(tp->ciphers);
    tp->ciphers = 0;
    if (ciphers && *ciphers) {
        tp->ciphers = sclone(ciphers);
    }
}

PUBLIC void rSetTlsDefaultCiphers(cchar *ciphers)
{
    rFree(defaultCiphers);
    defaultCiphers = 0;
    if (ciphers && *ciphers) {
        defaultCiphers = sclone(ciphers);
    }
}

PUBLIC void rSetTlsAlpn(Rtls *tp, cchar *alpn)
{
    rFree(tp->alpn);
    tp->alpn = sclone(alpn);
}

PUBLIC void rSetTlsDefaultAlpn(cchar *alpn)
{
    rFree(defaultAlpn);
    defaultAlpn = sclone(alpn);
}

PUBLIC void rSetTlsVerify(Rtls *tp, int verifyPeer, int verifyIssuer)
{
    tp->verifyPeer = verifyPeer;
    tp->verifyIssuer = verifyIssuer;
}

PUBLIC void rSetTlsDefaultVerify(int verifyPeer, int verifyIssuer)
{
    defaultVerifyPeer = verifyPeer;
    defaultVerifyIssuer = verifyIssuer;
}

PUBLIC void rSetTlsEngine(Rtls *tp, cchar *engine)
{
    //  Not supported
}

static void logMbedtls(void *context, int level, cchar *file, int line, cchar *str)
{
    rDebug("mbedtls", "mbedtls: %s", str);
}

PUBLIC void *rGetTlsRng(void)
{
    return &ctr;
}

static void logCiphers(Rtls *tp)
{
    char cipher[80];
    cint *cp;

    rDebug("mbedtls", "Supported Ciphers");
    for (cp = mbedtls_ssl_list_ciphersuites(); *cp; cp++) {
        scopy(cipher, sizeof(cipher), (char*) mbedtls_ssl_get_ciphersuite_name(*cp));
        replaceHyphen(cipher, '-', '_');
        rDebug("mbedtls", "%s (0x%04X)", cipher, *cp);
    }
}

#else
PUBLIC void mbedDummy(void)
{
}
#endif /* ME_COM_MBEDTLS */
#endif /* R_USE_TLS */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/mem.c ************/

/**
    mem.c - Memory allocation

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



/*********************************** Locals **********************************/

static RMemProc memHandler;

/************************************ Code ************************************/

PUBLIC void *rAllocMem(size_t size)
{
    void   *ptr;
    size_t aligned;

    // Check for overflow before alignment
    if (size > SIZE_MAX - 8) {
        rAllocException(R_MEM_FAIL, size);
        return 0;
    }
    if (size == 0) {
        //  Ensure that we allocate at least 1 byte
        size = 1;
    }
    aligned = R_ALLOC_ALIGN(size, 8);
    if (aligned < size) {
        rAllocException(R_MEM_FAIL, size);
        return 0;
    }
    size = aligned;
#if ESP32
    //  Allocate memory from PSIRAM
    ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
#else
    ptr = malloc(size);
#endif
    if (ptr == 0) {
        rAllocException(R_MEM_FAIL, size);
        return 0;
    }
#if ME_FIBER_GUARD_PAD
    rCheckFiber();
#endif
    return ptr;
}

PUBLIC void rFreeMem(void *ptr)
{
    if (ptr) {
        free(ptr);
    }
}

PUBLIC void *rMemdup(cvoid *ptr, size_t usize)
{
    char *newp;

    if (ptr == NULL) {
        return NULL;
    }
    if ((newp = rAlloc(usize)) != 0) {
        memcpy(newp, ptr, usize);
    }
    return newp;
}

PUBLIC int rMemcmp(cvoid *s1, size_t s1Len, cvoid *s2, size_t s2Len)
{
    int rc;

    assert(s1);
    assert(s2);

    if (!s1 || !s2) {
        return s1 ? 1 : (s2 ? -1 : 0);
    }
    if ((rc = memcmp(s1, s2, min(s1Len, s2Len))) == 0) {
        if (s1Len < s2Len) {
            return -1;
        } else if (s1Len > s2Len) {
            return 1;
        }
    }
    return rc;
}

/*
    Support Insitu copy where src and destination overlap
 */
PUBLIC size_t rMemcpy(void *dest, size_t destMax, cvoid *src, size_t nbytes)
{
    if (!dest || !src) {
        return 0;
    }
    if (nbytes > destMax) {
        rAllocException(R_ERR_WONT_FIT, nbytes);
        return 0;
    }
    if (nbytes > 0) {
        memmove(dest, src, nbytes);
        return nbytes;
    } else {
        return 0;
    }
}

PUBLIC void *rReallocMem(void *mem, size_t size)
{
    void   *ptr;
    size_t aligned;

    if (size > SIZE_MAX - 8) {
        rAllocException(R_MEM_FAIL, size);
        return 0;
    }
    if (size == 0) {
        //  Ensure that we allocate at least 1 byte to avoid realloc(mem, 0) behavior
        size = 1;
    }
    aligned = R_ALLOC_ALIGN(size, 8);
    if (aligned < size) {
        rAllocException(R_MEM_FAIL, size);
        return 0;
    }
    size = aligned;
    if ((ptr = realloc(mem, size)) == 0) {
        rAllocException(R_MEM_FAIL, size);
        return 0;
    }
#if ME_FIBER_GUARD_PAD
    rCheckFiber();
#endif
    return ptr;
}

PUBLIC void rSetMemHandler(RMemProc handler)
{
    memHandler = handler;
}

PUBLIC void rAllocException(int cause, size_t size)
{
    if (memHandler) {
        (memHandler) (cause, size);
    } else {
        rFprintf(stderr, "Memory allocation error for %zd bytes", size);
        abort();
    }
}

/*
    Allocate memory via virtual memory allocation (mmap/VirtualAlloc).
    This keeps stack allocations separate from the heap to reduce fragmentation.
 */
PUBLIC void *rAllocVirt(size_t size)
{
#if MACOSX || LINUX || FREEBSD
    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        return NULL;
    }
    return ptr;
#elif WINDOWS
    void *ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return ptr;
#else
    return rAllocMem(size);
#endif
}

/*
    Free memory allocated via rAllocVirt
 */
PUBLIC void rFreeVirt(void *ptr, size_t size)
{
    if (!ptr) return;
#if MACOSX || LINUX || FREEBSD
    munmap(ptr, size);
#elif WINDOWS
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    rFree(ptr);
#endif
}

#if ME_FIBER_GROWABLE_STACK
/*
    Reserve virtual address space with PROT_NONE (uncommitted)
 */
PUBLIC void *rAllocPages(size_t size)
{
#if MACOSX || LINUX || FREEBSD
    void *ptr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        return NULL;
    }
    return ptr;
#elif WINDOWS
    void *ptr = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
    return ptr;
#else
    return NULL;
#endif
}

/*
    Free reserved virtual address space
 */
PUBLIC void rFreePages(void *ptr, size_t size)
{
    if (!ptr) return;
#if MACOSX || LINUX || FREEBSD
    munmap(ptr, size);
#elif WINDOWS
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    // No-op on unsupported platforms
#endif
}

/*
    Change memory protection on a region
 */
PUBLIC int rProtectPages(void *addr, size_t size, int prot)
{
#if MACOSX || LINUX || FREEBSD
    int mprot = PROT_NONE;
    if (prot & R_PROT_READ) mprot |= PROT_READ;
    if (prot & R_PROT_WRITE) mprot |= PROT_WRITE;
    if (prot & R_PROT_EXEC) mprot |= PROT_EXEC;
    return mprotect(addr, size, mprot);
#elif WINDOWS
    DWORD winProt = PAGE_NOACCESS;
    if (prot & R_PROT_WRITE) {
        winProt = PAGE_READWRITE;
    } else if (prot & R_PROT_READ) {
        winProt = PAGE_READONLY;
    }
    // Must commit before protecting
    if (!VirtualAlloc(addr, size, MEM_COMMIT, winProt)) {
        return -1;
    }
    return 0;
#else
    return -1;
#endif
}

/*
    Get system page size (cached)
 */
PUBLIC size_t rGetPageSize(void)
{
    static size_t pageSize = 0;

    if (pageSize == 0) {
#if MACOSX || LINUX || FREEBSD
        pageSize = (size_t) sysconf(_SC_PAGESIZE);
#elif WINDOWS
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        pageSize = si.dwPageSize;
#else
        pageSize = 4096;  // Default fallback
#endif
    }
    return pageSize;
}
#endif /* ME_FIBER_GROWABLE_STACK */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/openssl.c ************/

/*
    openssl.c - Transport Layer Security for OpenSSL

    This code expects OpenSSL version >= 1.1.1 (i.e. with TLSv1.3 support)

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



#if R_USE_TLS
#if ME_COM_OPENSSL

// Clashes with WinCrypt.h */
#undef OCSP_RESPONSE

/*
   Indent includes to bypass MakeMe dependencies
 */
 #include    <openssl/opensslv.h>
 #include    <openssl/ssl.h>
 #include    <openssl/evp.h>
 #include    <openssl/rand.h>
 #include    <openssl/err.h>
 #include    <openssl/dh.h>
 #include    <openssl/rsa.h>
 #include    <openssl/bio.h>

#if ME_R_TLS_ENGINE
    #include    <openssl/x509v3.h>
    #ifndef OPENSSL_NO_ENGINE
        #include    <openssl/engine.h>
        #define R_HAS_CRYPTO_ENGINE 1
    #endif
#endif

/*
    Define default OpenSSL options
    Ensure we generate a new private key for each connection
    Disable SSLv2, SSLv3 and TLSv1 by default -- they are insecure.
 */
#ifndef ME_R_TLS_SET_OPTIONS
    #define ME_R_TLS_SET_OPTIONS   ( \
                SSL_OP_ALL | \
                SSL_OP_SINGLE_DH_USE | \
                SSL_OP_SINGLE_ECDH_USE | \
                SSL_OP_NO_SSLv2 | \
                SSL_OP_NO_SSLv3 | \
                SSL_OP_NO_TLSv1 | \
                SSL_OP_NO_TLSv1_1)
#endif
#ifndef ME_R_TLS_CLEAR_OPTIONS
    #define ME_R_TLS_CLEAR_OPTIONS 0
#endif

/************************************ Locals **********************************/
#if ME_UNIX_LIKE
/*
    Mac OS X OpenSSL stack is deprecated. Suppress those warnings.
 */
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

typedef struct Rtls {
    RSocket *sock;                          /* Owning socket */
    Socket fd;                              /* Socket file descriptor */
    char *alpn;                             /* ALPN protocols */
    char *keyFile;                          /* Alternatively, locate the key in a file */
    char *certFile;                         /* Certificate filename */
    char *revokeFile;                       /* Certificate revocation list */
    char *caFile;                           /* Certificate verification cer file or bundle */
    char *ciphers;                          /* Cipher suite to use for connection */
    char *cipher;                           /* Cipher in use for connection */
    char *engine;                           /* Engine device */
    char *peer;                             /* Peer address */
    char *protocol;                         /* Cipher in use for connection */
    uint connected : 1;                     /* Connection established */
    uint freeCtx : 1;                       /* Ctx owned by this */
    uint server : 1;
    int verifyPeer : 2;                     /* Verify the peer certificate */
    int verifyIssuer : 2;                   /* Verify issuer of peer cert. Set to 0 to permit self signed certs */

    SSL_CTX *ctx;
    SSL *handle;
    BIO *bio;
    SSL_SESSION *session;                   /* Cached session for client resumption */
    int handshakes;
} Rtls;

/*
    Certificate and key formats
 */
#define FORMAT_PEM 1
#define FORMAT_DER 2

static char *defaultAlpn;                 /* Default ALPN protocols */
static char *defaultCaFile;               /* Default certificate verification cer file or bundle */
static char *defaultCertFile;             /* Default certificate filename */
static char *defaultKeyFile;              /* Default Alternatively, locate the key in a file */
static char *defaultRevokeFile;           /* Default certificate revocation list */
static char *defaultCiphers;              /* Default Ciphers to use for connection */
static int  defaultVerifyPeer = 1;        /* Verify peer certificates */
static int  defaultVerifyIssuer = 1;      /* Verify issuer of peer certificates */

/***************************** Forward Declarations ***************************/

static char *getTlsError(Rtls *tp, char *buf, size_t bufsize);
static int  handshake(Rtls *tp, Ticks deadline);
static int  initEngine(Rtls *tp);
static int  parseCert(Rtls *tp, cchar *path);
static int  parseKey(Rtls *tp, SSL_CTX *ctx, cchar *keyFile);
static int  selectAlpn(SSL *ssl, cuchar **out, uchar *outlen, cuchar *in, uint inlen, void *arg);
static int  setCiphers(SSL_CTX *ctx, cchar *ciphers);
static int  verifyPeerCertificate(int ok, X509_STORE_CTX *xctx);

/************************************* Code ***********************************/
/*
    Initialize the SSL layer
 */
PUBLIC int rInitTls(void)
{
    /*
        Configure the SSL library. Use the crypto ID as a one-time test. This allows
        users to configure the library and have their configuration used instead.
     */
    if (CRYPTO_get_id_callback() == 0) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
        // OpenSSL < 1.1.0 requires manual initialization
#if !ME_WIN_LIKE
        OpenSSL_add_all_algorithms();
#endif
        SSL_library_init();
        SSL_load_error_strings();
#endif
#if R_HAS_CRYPTO_ENGINE
        ENGINE_load_builtin_engines();
        ENGINE_add_conf_module();
        CONF_modules_load_file(NULL, NULL, 0);
#endif
    }
    return 0;
}

PUBLIC void rTermTls(void)
{
#if R_HAS_CRYPTO_ENGINE
    ENGINE_cleanup();
#endif
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    // OpenSSL < 1.1.0 requires manual cleanup
    ERR_free_strings();
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
#endif

    rFree(defaultAlpn);
    rFree(defaultCaFile);
    rFree(defaultCertFile);
    rFree(defaultCiphers);
    rFree(defaultKeyFile);
    rFree(defaultRevokeFile);

    defaultAlpn = 0;
    defaultCaFile = 0;
    defaultCertFile = 0;
    defaultCiphers = 0;
    defaultKeyFile = 0;
    defaultRevokeFile = 0;
}

PUBLIC Rtls *rAllocTls(RSocket *sock)
{
    Rtls *tp;

    if ((tp = rAllocType(Rtls)) == 0) {
        return 0;
    }
    tp->sock = sock;
    tp->verifyPeer = -1;
    tp->verifyIssuer = -1;
    return tp;
}

PUBLIC void rFreeTls(Rtls *tp)
{
    int ret;

    if (!tp) {
        return;
    }
    rFree(tp->alpn);
    rFree(tp->certFile);
    rFree(tp->caFile);
    rFree(tp->cipher);
    rFree(tp->ciphers);
    rFree(tp->keyFile);
    rFree(tp->engine);
    rFree(tp->peer);
    rFree(tp->protocol);

    if (tp->ctx && tp->freeCtx) {
        SSL_CTX_free(tp->ctx);
    }
    if (tp->handle) {
        //  Bidirectional shutdown: call twice if first returns 0
        ret = SSL_shutdown(tp->handle);
        if (ret == 0) {
            SSL_shutdown(tp->handle);
        }
        SSL_free(tp->handle);
        ERR_clear_error();
    }
    rFree(tp);
}

PUBLIC void rCloseTls(Rtls *tp)
{
    int ret;

    if (tp && tp->fd != INVALID_SOCKET) {
        if (tp->handle) {
            //  Bidirectional shutdown: call twice if first returns 0
            ret = SSL_shutdown(tp->handle);
            if (ret == 0) {
                SSL_shutdown(tp->handle);
            }
            ERR_clear_error();
        }
    }
}

PUBLIC int rConfigTls(Rtls *tp, bool server)
{
    X509_STORE *store;
    SSL_CTX    *ctx;
    uchar      resume[16];

    STACK_OF(X509_NAME) * certNames;
    char abuf[128];
    int  verifyMode;

    tp->server = server;

    if ((ctx = SSL_CTX_new(TLS_method())) == 0) {
        return rSetSocketError(tp->sock, "Unable to create SSL context");
    }
    tp->ctx = ctx;
    tp->freeCtx = 1;
    SSL_CTX_set_ex_data(ctx, 0, (void*) tp);

#if defined(TLS1_3_VERSION) && ME_ENFORCE_TLS1_3
    #if defined(SSL_CTX_set_min_proto_version)
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    #else
        #ifdef SSL_OP_NO_TLSv1
    SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1);
        #endif
        #ifdef SSL_OP_NO_TLSv1_1
    SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_1);
        #endif
    #endif
#endif

    if (tp->verifyIssuer < 0) {
        tp->verifyIssuer = defaultVerifyIssuer;
    }
    if (tp->verifyPeer < 0) {
        tp->verifyPeer = defaultVerifyPeer;
    }
    tp->alpn = tp->alpn ? tp->alpn : scloneNull(defaultAlpn);
    tp->caFile = tp->caFile ? tp->caFile : (server ? 0 : scloneNull(defaultCaFile));
    tp->certFile = tp->certFile ? tp->certFile : scloneNull(defaultCertFile);
    tp->keyFile = tp->keyFile ? tp->keyFile : scloneNull(defaultKeyFile);
    tp->revokeFile = tp->revokeFile ? tp->revokeFile : scloneNull(defaultRevokeFile);
    tp->ciphers = tp->ciphers ? tp->ciphers : scloneNull(defaultCiphers);

    /*
        Configure the certificates
     */
    if (tp->certFile) {
        if (parseCert(tp, tp->certFile) < 0) {
            return R_ERR_CANT_INITIALIZE;
        }
        tp->keyFile = (tp->keyFile == 0) ? tp->certFile : tp->keyFile;
        if (tp->keyFile) {
            if (parseKey(tp, ctx, tp->keyFile) < 0) {
                return R_ERR_CANT_INITIALIZE;
            }
            if (!SSL_CTX_check_private_key(ctx)) {
                return rSetSocketError(tp->sock, "Check of private key file failed: %s", tp->keyFile);
            }
        }
    }
    if (tp->ciphers) {
        if (setCiphers(ctx, tp->ciphers) < 0) {
            return rSetSocketError(tp->sock, "Unable to define ciphers \"%s\"", tp->ciphers);
        }
    }
    if (tp->verifyPeer == 1) {
        /*
            Use either the authority file or the default verify paths
            OpenSSL currently has issues where loading additional paths may (may not) invalidate the default paths
         */
        if (tp->caFile) {
            if (!SSL_CTX_load_verify_locations(ctx, (char*) tp->caFile, NULL)) {
                return rSetSocketError(tp->sock, "Unable to set certificate locations: %s", tp->caFile);
            }
            certNames = SSL_load_client_CA_file(tp->caFile);
            if (certNames) {
                // Define the list of CA certificates to send to the client before they send their client certificate
                // for validation
                SSL_CTX_set_client_CA_list(ctx, certNames);
            }
        } else if (!SSL_CTX_set_default_verify_paths(ctx)) {
            // OpenSSL listens to the env vars: SSL_CERT_DIR and SSL_CERT_FILE to override the default certificate
            // locations
            return rSetSocketError(tp->sock, "Unable to set default certificate locations");
        }
        store = SSL_CTX_get_cert_store(ctx);
        if (tp->revokeFile && !X509_STORE_load_locations(store, tp->revokeFile, 0)) {
            return rSetSocketError(tp->sock, "Cannot load certificate revoke list: %s", tp->revokeFile);
        }
        X509_STORE_set_ex_data(store, 0, (void*) tp);
        verifyMode = SSL_VERIFY_PEER;
        if (server) {
            verifyMode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
        }
        SSL_CTX_set_verify(ctx, verifyMode, verifyPeerCertificate);
    }
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY | SSL_MODE_RELEASE_BUFFERS | SSL_MODE_ENABLE_PARTIAL_WRITE);

    // Enable TLS session resumption for server connections
    if (server) {
        RAND_bytes(resume, sizeof(resume));
        SSL_CTX_set_session_id_context(ctx, resume, sizeof(resume));
        SSL_CTX_sess_set_cache_size(ctx, ME_R_SSL_CACHE);
    }

    if (ME_R_TLS_SET_OPTIONS) {
        SSL_CTX_set_options(ctx, ME_R_TLS_SET_OPTIONS);
    }
    if (ME_R_TLS_CLEAR_OPTIONS) {
        SSL_CTX_clear_options(ctx, ME_R_TLS_CLEAR_OPTIONS);
    }
    if (tp->alpn) {
        if (tp->server) {
            SSL_CTX_set_alpn_select_cb(ctx, selectAlpn, (void*) tp);
        } else {
            size_t alpnLen = slen(tp->alpn);
            // NOTE: This ALPN protocol string format only supports one protocol
            // ALPN length is limited to 255 bytes by protocol and buffer is 128 bytes
            if (alpnLen > 255) {
                return rSetSocketError(tp->sock, "ALPN protocol name exceeds 255 bytes");
            }
            if (alpnLen > 126) {
                return rSetSocketError(tp->sock, "ALPN protocol name too long: %zu bytes", alpnLen);
            }
            SFMT(abuf, "%c%s", (uchar) alpnLen, tp->alpn);
            SSL_CTX_set_alpn_protos(ctx, (cuchar*) abuf, (uint) slen(abuf));
        }
    }
    if (initEngine(tp) < 0) {
        // Continue without engine
    }
    return 0;
}

static int initEngine(Rtls *tp)
{
#if R_HAS_CRYPTO_ENGINE
    if (tp->engine) {
        ENGINE *engine;
        if (!(engine = ENGINE_by_id(tp->engine))) {
            return rSetSocketError(tp->sock, "Cannot find crypto device %s", tp->engine);
        }
        if (!ENGINE_set_default(engine, ENGINE_METHOD_ALL)) {
            ENGINE_free(engine);
            return rSetSocketError(tp->sock, "Cannot find crypto device %s", tp->engine);
        }
        rInfo("tls", "Loaded crypto device %s", tp->engine);
        ENGINE_free(engine);
    }
#endif
    return 0;
}

static int selectAlpn(SSL *ssl, cuchar **out, uchar *outlen, cuchar *in, uint inlen, void *arg)
{
    Rtls  *tp;
    cchar *alpn;

    tp = arg;
    alpn = tp->alpn;
    if (alpn == 0) {
        return SSL_TLSEXT_ERR_NOACK;
    }
    /*
        WARNING: this appalling API expects pbuf to be static / persistent and sets *out to refer to it.
        NOTE: ALPN protocol string only supports one protocol.
     */
    if (SSL_select_next_proto((uchar**) out, outlen, (cuchar*) alpn, (uint) slen(alpn), in,
                              inlen) != OPENSSL_NPN_NEGOTIATED) {
        return SSL_TLSEXT_ERR_NOACK;
    }
    return SSL_TLSEXT_ERR_OK;
}

PUBLIC Rtls *rAcceptTls(Rtls *tp, Rtls *listen)
{
    tp->verifyPeer = listen->verifyPeer;
    tp->verifyIssuer = listen->verifyIssuer;
    tp->ctx = listen->ctx;
    tp->server = 1;
    return tp;
}

PUBLIC int rUpgradeTls(Rtls *tp, Socket fd, cchar *peer, Ticks deadline)
{
    int rc;

    assert(tp);

    tp->fd = fd;

    if ((tp->handle = (SSL*) SSL_new(tp->ctx)) == 0) {
        return R_ERR_BAD_STATE;
    }
    SSL_set_app_data(tp->handle, (void*) tp);
    SSL_set_SSL_CTX(tp->handle, tp->ctx);

    // Apply cached session for client-side resumption
    if (tp->session) {
        SSL_set_session(tp->handle, tp->session);
    }

    /*
        Create a socket bio. We don't use the BIO except as storage for the fd
     */
    if ((tp->bio = BIO_new_socket((int) tp->fd, BIO_NOCLOSE)) == 0) {
        SSL_free(tp->handle);
        tp->handle = NULL;
        return R_ERR_BAD_STATE;
    }
    SSL_set_bio(tp->handle, tp->bio, tp->bio);

    if (tp->server) {
        SSL_set_accept_state(tp->handle);
        rc = 0;
    } else {
        if (peer) {
            tp->peer = sclone(peer);
            X509_VERIFY_PARAM *param = SSL_get0_param(tp->handle);
            X509_VERIFY_PARAM_set_hostflags(param, 0);
            X509_VERIFY_PARAM_set1_host(param, peer, 0);
            SSL_set_tlsext_host_name(tp->handle, peer);
        }

        ERR_clear_error();
        if ((rc = SSL_connect(tp->handle)) < 1) {
            int error = SSL_get_error(tp->handle, rc);
            if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE || error == SSL_ERROR_WANT_CONNECT) {
                rc = 0;
            } else {
                char ebuf[80];
                getTlsError(tp, ebuf, sizeof(ebuf));
                return rSetSocketError(tp->sock, "Connect failed: error %s", ebuf);
            }
        }
    }
    if (handshake(tp, deadline) < 0) {
        return R_ERR_CANT_INITIALIZE;
    }
    return rc;
}

static int handshake(Rtls *tp, Ticks deadline)
{
    int error, mask, rc;

    mask = R_IO;
    for (;;) {
        ERR_clear_error();
        if ((rc = SSL_do_handshake(tp->handle)) >= 0) {
            break;
        }
        error = SSL_get_error(tp->handle, rc);
        mask = 0;
        if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_ACCEPT) {
            mask |= R_READABLE;
        } else if (error == SSL_ERROR_WANT_WRITE || error == SSL_ERROR_WANT_CONNECT) {
            mask |= R_WRITABLE;
        } else {
#if ME_R_DEBUG_LOGGING
            if (rEmitLog("debug", "tls")) {
                char ebuf[80];
                getTlsError(tp, ebuf, sizeof(ebuf));
                rDebug("tls", "SSL_read %s", ebuf);
            }
#endif
            return R_ERR_CANT_CONNECT;
        }
        if (rWaitForIO(tp->sock->wait, mask, deadline) < 0) {
            return R_ERR_TIMEOUT;
        }
    }
    tp->protocol = sclone(SSL_get_version(tp->handle));
    tp->cipher = sclone(SSL_get_cipher(tp->handle));
    tp->connected = 1;

#if ME_R_DEBUG_LOGGING
    if (rEmitLog("debug", "tls")) {
        rDebug("tls", "Handshake with %s and %s", tp->protocol, tp->cipher);
    }
#endif
    return 1;
}

/*
    Return the number of bytes read. Return -1 on errors and EOF. Distinguish EOF via mprIsSocketEof.
    If non-blocking, may return zero if no data or still handshaking.
    Let rReadSync do a wait for I/O if required.
 */
PUBLIC ssize rReadTls(Rtls *tp, void *buf, size_t len)
{
    int rc, error, toRead;

    if (tp->handle == 0) {
        return R_ERR_BAD_STATE;
    }
    ERR_clear_error();
    toRead = (len > INT_MAX) ? INT_MAX : (int) len;
    rc = SSL_read(tp->handle, buf, toRead);
    if (rc <= 0) {
        error = SSL_get_error(tp->handle, rc);
        if (!(error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_CONNECT || error == SSL_ERROR_WANT_ACCEPT)) {
            if (error != SSL_ERROR_ZERO_RETURN) {
                char ebuf[80];
                getTlsError(tp, ebuf, sizeof(ebuf));
                rDebug("tls", "SSL_read %s", ebuf);
            }
            return R_ERR_CANT_READ;
        }
        rc = 0;
    }
    return rc;
}

/*
    Write data. Return the number of bytes written or -1 on errors.
 */
PUBLIC ssize rWriteTls(Rtls *tp, cvoid *buf, size_t len)
{
    size_t totalWritten;
    int    error, rc, toWrite;

    if (tp->bio == 0 || tp->handle == 0 || len <= 0) {
        return R_ERR_BAD_STATE;
    }
    totalWritten = 0;

    do {
        ERR_clear_error();
        toWrite = (len > INT_MAX) ? INT_MAX : (int) len;
        rc = SSL_write(tp->handle, buf, toWrite);
        if (rc <= 0) {
            error = SSL_get_error(tp->handle, rc);
            if (error != SSL_ERROR_WANT_WRITE) {
                return R_ERR_CANT_WRITE;
            }
            break;
        }
        totalWritten += (size_t) rc;
        buf = (void*) ((char*) buf + rc);
        len -= (size_t) rc;
    } while (len > 0);

    return (ssize) totalWritten;
}

/*
    Load a certificate into the context from the supplied buffer. Type indicates the desired format. The path is only
       used for errors.
 */
static int loadCert(Rtls *tp, SSL_CTX *ctx, cchar *buf, size_t len, int type, cchar *path)
{
    X509 *cert;
    BIO  *bio;
    bool loaded;

    assert(ctx);
    assert(buf);
    assert(type);
    assert(path && *path);

    cert = 0;
    loaded = 0;

    if ((bio = BIO_new_mem_buf((void*) buf, (int) len)) == 0) {
        rSetSocketError(tp->sock, "Unable to allocate memory for certificate %s", path);
    } else {
        if (type == FORMAT_PEM) {
            if ((cert = PEM_read_bio_X509(bio, NULL, 0, NULL)) == 0) {
                // Error reported by caller if loading all formats fail
            }
        } else if (type == FORMAT_DER) {
            if ((cert = d2i_X509_bio(bio, NULL)) == 0) {
                // Error reported by caller
            }
        }
        if (cert) {
            if (SSL_CTX_use_certificate(ctx, cert) != 1) {
                rSetSocketError(tp->sock, "Unable to use certificate %s", path);
            } else {
                loaded = 1;
            }
        }
    }
    if (bio) {
        BIO_free(bio);
    }
    if (cert) {
        X509_free(cert);
    }
    return loaded ? 0 : R_ERR_CANT_LOAD;
}

/*
    Load a certificate file in either PEM or DER format
 */
static int parseCert(Rtls *tp, cchar *certFile)
{
    SSL_CTX *ctx;
    char    *buf;
    size_t  len;
    int     rc;

    assert(tp);
    assert(certFile);
    ctx = tp->ctx;

    rc = 0;
    if (ctx == NULL || certFile == NULL) {
        return rc;
    }
    if ((buf = rReadFile(certFile, &len)) == 0) {
        rc = rSetSocketError(tp->sock, "Unable to read certificate %s", certFile);
    } else {
        if (loadCert(tp, ctx, buf, len, FORMAT_PEM, certFile) < 0 &&
            loadCert(tp, ctx, buf, len, FORMAT_DER, certFile) < 0) {
            rc = rSetSocketError(tp->sock, "Unable to load certificate %s", certFile);
        }
    }
    if (buf) {
        memset(buf, 0, len);
        rFree(buf);
    }
    return rc;
}

/*
    Load a key into the context from the supplied buffer. Type indicates the key format.  Path only used for
       diagnostics.
 */
static int loadKey(Rtls *tp, SSL_CTX *ctx, cchar *buf, size_t len, int type, cchar *path)
{
    EVP_PKEY *pkey;
    BIO      *bio;
    bool     loaded;
    cchar    *cp;

    assert(ctx);
    assert(buf);
    assert(type);
    assert(path && *path);

    pkey = 0;
    loaded = 0;

    /*
        Strip off EC parameters
     */
    if ((cp = sncontains(buf, "-----END EC PARAMETERS-----", len)) != NULL) {
        buf = &cp[28];
    }
    if ((bio = BIO_new_mem_buf((void*) buf, (int) len)) == 0) {
        rSetSocketError(tp->sock, "Unable to allocate memory for key %s", path);
        return R_ERR_MEMORY;
    }
    if (type == FORMAT_PEM) {
        // Headless: No support for passwords for encrypted private keys
        pkey = PEM_read_bio_PrivateKey(bio, NULL, 0, NULL);
    } else if (type == FORMAT_DER) {
        pkey = d2i_PrivateKey_bio(bio, NULL);
    }
    if (pkey) {
        if (SSL_CTX_use_PrivateKey(ctx, pkey) != 1) {
            rSetSocketError(tp->sock, "Unable to use key %s", path);
        } else {
            loaded = 1;
        }
        EVP_PKEY_free(pkey);
    }
    if (bio) {
        BIO_free(bio);
    }
    return loaded ? 0 : R_ERR_CANT_LOAD;
}

/*
    Load a key file in either PEM or DER format
 */
static int parseKey(Rtls *tp, SSL_CTX *ctx, cchar *keyFile)
{
    char   *buf;
    size_t len;
    int    rc;

    assert(ctx);
    assert(keyFile);

    buf = 0;
    rc = 0;

    if (ctx == NULL || keyFile == NULL) {
        ;
    } else if ((buf = rReadFile(keyFile, &len)) == 0) {
        rc = rSetSocketError(tp->sock, "Unable to read key %s", keyFile);

    } else if (loadKey(tp, ctx, buf, len, FORMAT_PEM, keyFile) < 0 &&
               loadKey(tp, ctx, buf, len, FORMAT_DER, keyFile) < 0) {
        rc = rSetSocketError(tp->sock, "Unable to load key %s", keyFile);
    }
    if (buf) {
        memset(buf, 0, len);
        rFree(buf);
    }
    return rc;
}

static int verifyPeerCertificate(int ok, X509_STORE_CTX *xctx)
{
    X509 *cert;
    SSL  *handle;
    Rtls *tp;
    char subject[1024], issuer[1024], peerName[1024];
    int  error;

    subject[0] = issuer[0] = '\0';
    handle = (SSL*) X509_STORE_CTX_get_ex_data(xctx, SSL_get_ex_data_X509_STORE_CTX_idx());
    if (!handle) {
        return 0;
    }
    tp = (Rtls*) SSL_get_app_data(handle);
    if (!tp) {
        return 0;
    }
    cert = X509_STORE_CTX_get_current_cert(xctx);
    if (!cert) {
        rSetSocketError(tp->sock, "No certificate provided");
        return 0;
    }
    error = X509_STORE_CTX_get_error(xctx);

    ok = 1;
    if (X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject) - 1) < 0) {
        rSetSocketError(tp->sock, "Cannot get subject name");
        ok = 0;
    }
    if (X509_NAME_oneline(X509_get_issuer_name(cert), issuer, sizeof(issuer) - 1) < 0) {
        rSetSocketError(tp->sock, "Cannot get issuer name");
        ok = 0;
    }
    if (X509_NAME_get_text_by_NID(X509_get_subject_name(cert), NID_commonName, peerName, sizeof(peerName) - 1) == 0) {
        rSetSocketError(tp->sock, "Cannot get peer name");
        ok = 0;
    }
    switch (error) {
    case X509_V_OK:
        break;
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
        // Normal self signed certificate
        if (tp->verifyIssuer == 1) {
            rSetSocketError(tp->sock, "Self-signed certificate");
            ok = 0;
        }
        break;

    case X509_V_ERR_CERT_UNTRUSTED:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
        if (tp->verifyIssuer == 1) {
            // Issuer cannot be verified
            rSetSocketError(tp->sock, "Certificate not trusted");
            ok = 0;
        }
        break;

    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
        if (tp->verifyIssuer == 1) {
            // Issuer cannot be verified
            rSetSocketError(tp->sock, "Certificate not trusted");
            ok = 0;
        }
        break;

    case X509_V_ERR_CERT_HAS_EXPIRED:
        rSetSocketError(tp->sock, "Certificate has expired");
        ok = 0;
        break;

#ifdef X509_V_ERR_HOSTNAME_MISMATCH
    case X509_V_ERR_HOSTNAME_MISMATCH:
        rSetSocketError(tp->sock, "Certificate hostname mismatch. Expecting %s got %s", tp->peer, peerName);
        ok = 0;
        break;
#endif
    case X509_V_ERR_CERT_CHAIN_TOO_LONG:
    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_CERT_REJECTED:
    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
    case X509_V_ERR_INVALID_CA:
    default:
        rSetSocketError(tp->sock, "Certificate verification error %d", error);
        ok = 0;
        break;
    }
    return ok;
}

static char *getTlsError(Rtls *tp, char *buf, size_t bufsize)
{
    ERR_error_string_n(ERR_get_error(), buf, bufsize - 1);
    buf[bufsize - 1] = '\0';
    return buf;
}

static int setCiphers(SSL_CTX *ctx, cchar *ciphers)
{
    char *cbuf;

    cbuf = sclone(ciphers);
    for (char *cp = cbuf; *cp; cp++) {
        if (*cp == ',') *cp = ':';
    }
    rInfo("tls", "Using SSL ciphers: %s", cbuf);
    //  Try TLS1.3
    if (SSL_CTX_set_ciphersuites(ctx, cbuf) != 1) {
        //  Try TLS1.2 and below
        if (SSL_CTX_set_cipher_list(ctx, cbuf) != 1) {
            rFree(cbuf);
            return R_ERR_CANT_INITIALIZE;
        }
    }
    rFree(cbuf);
    return 0;
}

PUBLIC void rSetTlsCerts(Rtls *tp, cchar *ca, cchar *key, cchar *cert, cchar *revoke)
{
    if (key) {
        rFree(tp->keyFile);
        tp->keyFile = sclone(key);
    }
    if (cert) {
        rFree(tp->certFile);
        tp->certFile = sclone(cert);
    }
    if (revoke) {
        rFree(tp->revokeFile);
        tp->revokeFile = sclone(revoke);
    }
    if (ca) {
        rFree(tp->caFile);
        tp->caFile = sclone(ca);
    }
}

PUBLIC void rSetTlsDefaultCerts(cchar *ca, cchar *key, cchar *cert, cchar *revoke)
{
    if (ca) {
        rFree(defaultCaFile);
        defaultCaFile = sclone(ca);
    }
    if (key) {
        rFree(defaultKeyFile);
        defaultKeyFile = sclone(key);
    }
    if (cert) {
        rFree(defaultCertFile);
        defaultCertFile = sclone(cert);
    }
    if (revoke) {
        rFree(defaultRevokeFile);
        defaultRevokeFile = sclone(revoke);
    }
}

PUBLIC void rSetTlsCiphers(Rtls *tp, cchar *ciphers)
{
    rFree(tp->ciphers);
    tp->ciphers = 0;
    if (ciphers && *ciphers) {
        tp->ciphers = sclone(ciphers);
    }
}

PUBLIC void rSetTlsDefaultCiphers(cchar *ciphers)
{
    rFree(defaultCiphers);
    defaultCiphers = 0;
    if (ciphers && *ciphers) {
        defaultCiphers = sclone(ciphers);
    }
}

PUBLIC void rSetTlsAlpn(Rtls *tp, cchar *alpn)
{
    rFree(tp->alpn);
    tp->alpn = sclone(alpn);
}

PUBLIC void rSetTlsDefaultAlpn(cchar *alpn)
{
    rFree(defaultAlpn);
    defaultAlpn = sclone(alpn);
}

PUBLIC void rSetTlsVerify(Rtls *tp, int verifyPeer, int verifyIssuer)
{
    tp->verifyPeer = verifyPeer;
    tp->verifyIssuer = verifyIssuer;
}

PUBLIC void rSetTlsDefaultVerify(int verifyPeer, int verifyIssuer)
{
    defaultVerifyPeer = verifyPeer;
    defaultVerifyIssuer = verifyIssuer;
}

PUBLIC bool rIsTlsConnected(Rtls *tp)
{
    return tp->connected;
}

PUBLIC void rSetTlsEngine(Rtls *tp, cchar *engine)
{
    rFree(tp->engine);
    tp->engine = sclone(engine);
}

PUBLIC void *rGetTlsSession(RSocket *sp)
{
    Rtls *tp;

    if (!sp || !sp->tls) {
        return NULL;
    }
    tp = sp->tls;
    if (tp->handle) {
        return SSL_get1_session(tp->handle);
    }
    return NULL;
}

PUBLIC void rSetTlsSession(RSocket *sp, void *session)
{
    Rtls *tp;

    if (!sp || !sp->tls) {
        return;
    }
    tp = sp->tls;
    tp->session = (SSL_SESSION*) session;
}

PUBLIC void rFreeTlsSession(void *session)
{
    if (session) {
        SSL_SESSION_free((SSL_SESSION*) session);
    }
}

#else
void opensslDummy(void)
{
}
#endif /* ME_COM_OPENSSL */
#endif /* R_USE_TLS */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/printf.c ************/

/*
    printf.c -- More secure, smaller, safer printf

    This routine uses minimal stack and is null tolerant.

    IMPORTANT: This printf implementation is NOT designed to be 100% compatible with standard printf.
    It provides a secure, embedded-friendly subset of printf functionality with the following differences:
    - The %n format specifier is not supported (security)
    - Floating point formatting may differ slightly from standard printf
    - Some advanced format specifiers may not be supported
    - Optimized for embedded systems with limited resources

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/



/*
    WARNING: The R_OWN_PRINTF=0 build configuration is not recommended for production use.
    It relies on the system's printf implementation, which may be vulnerable to format string attacks
    (e.g., using "%n"). The default build uses a secure, smaller, custom printf implementation.
    Disabling this should only be done with a full understanding of the security implications.
 */
#ifndef R_OWN_PRINTF
    #define R_OWN_PRINTF    1
#endif

#define R_PRINTF_ALLOC_SIZE 256

/*********************************** Defines **********************************/
#if R_OWN_PRINTF
/*
    Class definitions
 */
#define CLASS_NORMAL    0               /* [All other]      Normal characters */
#define CLASS_PERCENT   1               /* [%]              Begin format */
#define CLASS_MODIFIER  2               /* [-+ #,']         Modifiers */
#define CLASS_ZERO      3               /* [0]              Special modifier - zero pad */
#define CLASS_STAR      4               /* [*]              Width supplied by arg */
#define CLASS_DIGIT     5               /* [1-9]            Field widths */
#define CLASS_DOT       6               /* [.]              Introduce precision */
#define CLASS_BITS      7               /* [hlL]            Length bits */
#define CLASS_TYPE      8               /* [cdefginopsSuxX] Type specifiers */

#define STATE_NORMAL    0               /* Normal chars in format string */
#define STATE_PERCENT   1               /* "%" */
#define STATE_MODIFIER  2               /* -+ #,' */
#define STATE_WIDTH     3               /* Width spec */
#define STATE_DOT       4               /* "." */
#define STATE_PRECISION 5               /* Precision spec */
#define STATE_BITS      6               /* Size spec */
#define STATE_TYPE      7               /* Data type */
#define STATE_COUNT     8

PUBLIC char stateMap[] = {
    /*     STATES:  Normal Percent Modifier Width  Dot  Prec Bits Type */
    /* CLASS           0      1       2       3     4     5    6    7  */
    /* Normal   0 */ 0,     0,      0,      0,    0,    0,   0,   0,
    /* Percent  1 */ 1,     0,      1,      1,    1,    1,   1,   1,
    /* Modifier 2 */ 0,     2,      2,      0,    0,    0,   0,   0,
    /* Zero     3 */ 0,     2,      2,      3,    5,    5,   0,   0,
    /* Star     4 */ 0,     3,      3,      0,    5,    0,   0,   0,
    /* Digit    5 */ 0,     3,      3,      3,    5,    5,   0,   0,
    /* Dot      6 */ 0,     4,      4,      4,    0,    0,   0,   0,
    /* Bits     7 */ 0,     6,      6,      6,    6,    6,   6,   0,
    /* Types    8 */ 0,     7,      7,      7,    7,    7,   7,   0,
};

/*
    Format: %[modifier][width][precision][bits][type]

    The Class map will map from a specifier letter to a state.
 */
PUBLIC char classMap[] = {
    /*   0  ' '    !     "     #     $     %     &     ' */
    2,    0,    0,    2,    0,    1,    0,    2,
    /*  07   (     )     *     +     ,     -     .     / */
    0,    0,    4,    2,    2,    2,    6,    0,
    /*  10   0     1     2     3     4     5     6     7 */
    3,    5,    5,    5,    5,    5,    5,    5,
    /*  17   8     9     :     ;     <     =     >     ? */
    5,    5,    0,    0,    0,    0,    0,    0,
    /*  20   @     A     B     C     D     E     F     G */
    8,    0,    0,    0,    0,    8,    0,    8,
    /*  27   H     I     J     K     L     M     N     O */
    0,    0,    0,    0,    7,    0,    8,    0,
    /*  30   P     Q     R     S     T     U     V     W */
    0,    0,    0,    8,    0,    0,    0,    0,
    /*  37   X     Y     Z     [     \     ]     ^     _ */
    8,    0,    0,    0,    0,    0,    0,    0,
    /*  40   '     a     b     c     d     e     f     g */
    0,    0,    0,    8,    8,    8,    8,    8,
    /*  47   h     i     j     k     l     m     n     o */
    7,    8,    0,    0,    7,    0,    8,    8,
    /*  50   p     q     r     s     t     u     v     w */
    8,    0,    0,    8,    0,    8,    0,    8,
    /*  57   x     y     z  */
    8,    0,    7,
};

/*
    Flags
 */
#define SPRINTF_LEFT_ALIGN  0x1         /* Left align */
#define SPRINTF_LEAD_SIGN   0x2         /* Always sign the result */
#define SPRINTF_LEAD_SPACE  0x4         /* Leading space for +ve numbers */
#define SPRINTF_LEAD_ZERO   0x10        /* Zero pad */
#define SPRINTF_LEAD_PREFIX 0x20        /* 0 or 0x prefix */
#define SPRINTF_SHORT       0x40        /* 16-bit */
#define SPRINTF_LONG        0x80        /* 32-bit */
#define SPRINTF_INT64       0x100       /* 64-bit */
#define SPRINTF_COMMA       0x200       /* Thousand comma separators */
#define SPRINTF_UPPER_CASE  0x400       /* As the name says for numbers */
#define SPRINTF_SSIZE       0x800       /* Size of ssize */

typedef struct PContext {
    uchar *buf;
    uchar *endbuf;
    uchar *end;
    char format;
    int flags;
    int growBy;
    int len;
    int maxsize;
    int precision;
    int width;
    int upper;
    int floating;
    int overflow;
} PContext;

#define BPUT(ctx, c) \
        do { \
            (ctx)->len++; \
            /* Less one to allow room for the null */ \
            if ((ctx)->end >= ((ctx)->endbuf - sizeof(char))) { \
                if (growBuf(ctx) > 0) { \
                    *(ctx)->end++ = (uchar) (c); \
                } \
            } else { \
                *(ctx)->end++ = (uchar) (c); \
            } \
        } while (0)

#define BPUT_NULL(ctx) \
        do { \
            if ((ctx)->end >= (ctx)->endbuf) { \
                if (growBuf(ctx) > 0) { \
                    *(ctx)->end = '\0'; \
                } \
            } else { \
                *(ctx)->end = '\0'; \
            } \
        } while (0)

/********************************** Forwards **********************************/

static int  getNextState(char c, int state);
static int  growBuf(PContext *ctx);
static ssize innerSprintf(char **buf, size_t maxsize, cchar *spec, va_list args);
static void outFloat(PContext *ctx, char specChar, double value);
static void outFloatE(PContext *ctx, char specChar, double value);
static void outNum(PContext *ctx, size_t radix, int64 value);
static void outString(PContext *ctx, char *str, ssize len);

#endif /* R_OWN_PRINTF */
/************************************* Code ***********************************/

PUBLIC ssize rPrintf(cchar *fmt, ...)
{
    va_list ap;
    char    *buf;
    ssize   len;

    va_start(ap, fmt);
    len = rVsaprintf(&buf, 0, fmt, ap);
    va_end(ap);

    if (len > 0) {
        len = write(1, buf, (uint) len);
        rFree(buf);
    }
    return len;
}

PUBLIC ssize rFprintf(FILE *fp, cchar *fmt, ...)
{
    va_list ap;
    char    *buf;
    ssize   len;

    if (fp == 0) {
        return R_ERR_BAD_HANDLE;
    }
    va_start(ap, fmt);
    len = rVsaprintf(&buf, 0, fmt, ap);
    va_end(ap);

    if (len > 0) {
        len = write(fileno(fp), buf, (uint) len);
        rFree(buf);
    }
    return len;
}

PUBLIC ssize rSnprintf(char *buf, size_t maxsize, cchar *fmt, ...)
{
    va_list ap;
    ssize   len;

    va_start(ap, fmt);
    len = rVsnprintf(buf, maxsize, fmt, ap);
    va_end(ap);
    return len;
}

#if R_OWN_PRINTF
PUBLIC ssize rVsnprintf(char *buf, size_t maxsize, cchar *spec, va_list args)
{
    return innerSprintf(&buf, maxsize, spec, args);
}

PUBLIC ssize rVsaprintf(char **buf, size_t maxsize, cchar *spec, va_list args)
{
    *buf = 0;
    return innerSprintf(buf, maxsize, spec, args);
}

/*
    Format a string into a buffer. If *buf is null, allocate and caller must free.
    Returns the count of characters stored in buf or a negative error code for memory errors.
    If a buffer is supplied and is not large enough, the return value will be >= maxsize.
 */
static ssize innerSprintf(char **buf, size_t maxsize, cchar *spec, va_list args)
{
    PContext ctx;
    size_t   len;
    int64    iValue;
    uint64   uValue;
    bool     allocating;
    int      state;
    char     c;

    assert(buf);
    if (!buf) {
        return R_ERR_BAD_ARGS;
    }
    if (spec == 0) {
        spec = "";
    }
    allocating = (*buf == 0);
    if (!allocating) {
        //  User supplied buffer
        if (maxsize <= 0) {
            return R_ERR_BAD_ARGS;
        }
        ctx.buf = (uchar*) *buf;
        ctx.endbuf = &ctx.buf[maxsize];
        ctx.growBy = -1;
    } else {
        if (maxsize <= 0) {
            //  No limit
            maxsize = 0;
            len = R_PRINTF_ALLOC_SIZE;
        } else {
            len = min(R_PRINTF_ALLOC_SIZE, maxsize);
        }
        if ((ctx.buf = rAlloc(len)) == 0) {
            return R_ERR_MEMORY;
        }
        ctx.endbuf = &ctx.buf[len];
        ctx.growBy = R_PRINTF_ALLOC_SIZE;
    }
    ctx.maxsize = (int) maxsize;
    ctx.precision = 0;
    ctx.format = 0;
    ctx.floating = 0;
    ctx.width = 0;
    ctx.upper = 0;
    ctx.len = 0;
    ctx.flags = 0;
    ctx.end = ctx.buf;
    *ctx.buf = '\0';

    state = STATE_NORMAL;

    while ((c = *spec++) != '\0') {
        state = getNextState(c, state);

        switch (state) {
        case STATE_NORMAL:
            BPUT(&ctx, c);
            break;

        case STATE_PERCENT:
            ctx.precision = -1;
            ctx.width = 0;
            ctx.flags = 0;
            ctx.upper = 0;
            ctx.floating = 0;
            break;

        case STATE_MODIFIER:
            switch (c) {
            case '+':
                ctx.flags |= SPRINTF_LEAD_SIGN;
                break;
            case '-':
                ctx.flags |= SPRINTF_LEFT_ALIGN;
                break;
            case '#':
                ctx.flags |= SPRINTF_LEAD_PREFIX;
                break;
            case '0':
                ctx.flags |= SPRINTF_LEAD_ZERO;
                break;
            case ' ':
                ctx.flags |= SPRINTF_LEAD_SPACE;
                break;
            case ',':
            case '\'':
                ctx.flags |= SPRINTF_COMMA;
                break;
            }
            break;

        case STATE_WIDTH:
            if (c == '*') {
                ctx.width = va_arg(args, int);
                if (ctx.width < 0) {
                    ctx.width = -ctx.width;
                    ctx.flags |= SPRINTF_LEFT_ALIGN;
                }
            } else {
                while (isdigit((uchar) c)) {
                    if (ctx.width > (INT_MAX - (c - '0')) / 10) {
                        ctx.width = INT_MAX;
                    } else {
                        ctx.width = ctx.width * 10 + (c - '0');
                    }
                    c = *spec++;
                }
                spec--;
            }
            break;

        case STATE_DOT:
            ctx.precision = 0;
            break;

        case STATE_PRECISION:
            if (c == '*') {
                ctx.precision = va_arg(args, int);
            } else {
                while (isdigit((uchar) c)) {
                    if (ctx.precision > (INT_MAX - (c - '0')) / 10) {
                        ctx.precision = INT_MAX;
                    } else {
                        ctx.precision = ctx.precision * 10 + (c - '0');
                    }
                    c = *spec++;
                }
                spec--;
            }
            break;

        case STATE_BITS:
            switch (c) {
            case 'L':
                ctx.flags |= SPRINTF_INT64;
                break;

            case 'l':
                if (ctx.flags & SPRINTF_LONG) {
                    //  "ll"
                    ctx.flags &= ~SPRINTF_LONG;
                    ctx.flags |= SPRINTF_INT64;
                } else {
                    ctx.flags |= SPRINTF_LONG;
                }
                break;

            case 'h':
                ctx.flags |= SPRINTF_SHORT;
                break;

            case 'z':
                ctx.flags |= SPRINTF_SSIZE;
            }
            break;

        case STATE_TYPE:
            ctx.format = c;
            switch (c) {
            case 'G':
            case 'g':
            case 'f':
                ctx.floating = 1;
                outFloat(&ctx, c, (double) va_arg(args, double));
                break;

            case 'e':
            case 'E':
                ctx.floating = 1;
                outFloatE(&ctx, c, (double) va_arg(args, double));
                break;

            case 'c':
                BPUT(&ctx, (char) va_arg(args, int));
                break;

            case 's':
                // string
                outString(&ctx, va_arg(args, char*), -1);
                break;

            case 'i':
            case 'd':
                if (ctx.flags & SPRINTF_SHORT) {
                    iValue = (short) va_arg(args, int);
                } else if (ctx.flags & SPRINTF_LONG) {
                    iValue = (long) va_arg(args, long);
                } else if (ctx.flags & SPRINTF_SSIZE) {
                    iValue = (int64) va_arg(args, size_t);
                } else if (ctx.flags & SPRINTF_INT64) {
                    iValue = (int64) va_arg(args, int64);
                } else {
                    iValue = (int) va_arg(args, int);
                }
                outNum(&ctx, 10, iValue);
                break;

            case 'X':
                ctx.flags |= SPRINTF_UPPER_CASE;
#if ME_64
                ctx.flags &= ~(SPRINTF_SHORT | SPRINTF_LONG);
                ctx.flags |= SPRINTF_INT64;
#else
                ctx.flags &= ~(SPRINTF_INT64);
#endif
            /*  Fall through  */
            case 'o':
            case 'x':
            case 'u':
                if (ctx.flags & SPRINTF_SHORT) {
                    uValue = (ushort) va_arg(args, uint);
                } else if (ctx.flags & SPRINTF_LONG) {
                    uValue = (ulong) va_arg(args, ulong);
                } else if (ctx.flags & SPRINTF_SSIZE) {
                    uValue = (uint64) va_arg(args, size_t);
                } else if (ctx.flags & SPRINTF_INT64) {
                    uValue = (uint64) va_arg(args, uint64);
                } else {
                    uValue = va_arg(args, uint);
                }
                if (c == 'u') {
                    outNum(&ctx, 10, (int64) uValue);
                } else if (c == 'o') {
                    outNum(&ctx, 8, (int64) uValue);
                } else {
                    if (c == 'X') {
                        ctx.upper = 1;
                    }
                    outNum(&ctx, 16, (int64) uValue);
                }
                break;

#if DEPRECATE         // SECURITY
            case 'n': /* Count of chars seen thus far */
                if (ctx.flags & SPRINTF_SHORT) {
                    short *count = va_arg(args, short*);
                    *count = (int) (ctx.end - ctx.buf);
                } else if (ctx.flags & SPRINTF_LONG) {
                    long *count = va_arg(args, long*);
                    *count = (int) (ctx.end - ctx.buf);
                } else {
                    int *count = va_arg(args, int*);
                    *count = (int) (ctx.end - ctx.buf);
                }
                break;
#endif

            case 'p':       /* Pointer */
#if ME_64
                uValue = (uint64) va_arg(args, void*);
#else
                uValue = (uint) PTOI(va_arg(args, void*));
#endif
                ctx.flags |= SPRINTF_LEAD_PREFIX;
                outNum(&ctx, 16, (int64) uValue);
                break;

            default:
                BPUT(&ctx, c);
            }
        }
    }
    BPUT_NULL(&ctx);
    *buf = (char*) ctx.buf;

    if (allocating) {
        if (ctx.maxsize > 0 && ctx.len >= ctx.maxsize) {
            rFree(ctx.buf);
            *buf = NULL;
            return R_ERR_MEMORY;
        }
    }
    return ctx.len;
}

static int getNextState(char c, int state)
{
    int chrClass, index;

    index = c - ' ';
    if (index < 0 || index >= (int) sizeof(classMap)) {
        chrClass = CLASS_NORMAL;
    } else {
        chrClass = classMap[index];
    }
    assert((chrClass * STATE_COUNT + state) < (int) sizeof(stateMap));
    state = stateMap[chrClass * STATE_COUNT + state];
    return state;
}

static void outString(PContext *ctx, char *str, ssize flen)
{
    char   *cp;
    size_t i, len;

    if (str == NULL) {
        str = "null";
        len = 4;
    } else if (ctx->flags & SPRINTF_LEAD_PREFIX) {
        len = slen(str);
    } else if (ctx->precision >= 0) {
        for (cp = str, len = 0; len < (size_t) ctx->precision; len++) {
            if (*cp++ == '\0') {
                break;
            }
        }
    } else if (flen < 0) {
        len = slen(str);
    } else {
        len = (size_t) flen;
    }
    if (!(ctx->flags & SPRINTF_LEFT_ALIGN)) {
        for (i = len; i < (size_t) ctx->width; i++) {
            BPUT(ctx, (char) ' ');
        }
    }
    for (i = 0; i < len && *str; i++) {
        BPUT(ctx, *str++);
    }
    if (ctx->flags & SPRINTF_LEFT_ALIGN) {
        for (i = len; i < (size_t) ctx->width; i++) {
            BPUT(ctx, (char) ' ');
        }
    }
}

static void outNum(PContext *ctx, size_t radix, int64 value)
{
    static char numBuf[64];
    char        *cp, *endp;
    cchar       *prefix;
    int         letter, len, leadingZeros, i, fill, precision;
    uint64      uval;

    cp = endp = &numBuf[sizeof(numBuf) - 1];
    *endp = '\0';
    prefix = "";

    if (ctx->flags & SPRINTF_LEAD_PREFIX) {
        if (radix == 16) {
            prefix = "0x";
        } else if (radix == 8) {
            prefix = "0";
        }
    } else if (ctx->flags & SPRINTF_LEAD_SPACE && value >= 0) {
        prefix = " ";
    } else if (ctx->flags & SPRINTF_LEAD_SIGN && value >= 0) {
        prefix = "+";
    } else if (value < 0) {
        prefix = "-";
    }
    /*
        Convert to ascii
     */
    if (value < 0) {
        uval = (value == INT64_MIN) ? (uint64) INT64_MAX + 1 : (uint64) - value;
    } else {
        uval = (uint64) value;
    }
    if (radix == 16) {
        do {
            letter = (int) (uval % radix);
            if (letter > 9) {
                if (ctx->flags & SPRINTF_UPPER_CASE) {
                    letter = 'A' + letter - 10;
                } else {
                    letter = 'a' + letter - 10;
                }
            } else {
                letter += '0';
            }
            *--cp = letter;
            uval /= radix;
        } while (uval != 0);

    } else if (ctx->flags & SPRINTF_COMMA) {
        i = 1;
        do {
            int digit = (int) (uval % radix);
            *--cp = '0' + digit;
            uval /= radix;
            if ((i++ % 3) == 0 && uval != 0) {
                *--cp = ',';
            }
        } while (uval != 0);
    } else {
        do {
            int digit = (int) (uval % radix);
            *--cp = '0' + digit;
            uval /= radix;
        } while (uval != 0);
    }

    len = (int) (endp - cp);
    precision = max(ctx->precision, 0);
    leadingZeros = (!ctx->floating && precision > len) ? precision - len : 0;

    if (ctx->width) {
        if (ctx->floating) {
            //  Doing the integer portion of a float
            fill = ctx->width - precision - len - 1;
        } else {
            fill = ctx->width - max(precision, len);
        }
        if (prefix) {
            fill -= (int) slen(prefix);
        }
        if (fill < 0) {
            fill = 0;
        }
    } else {
        fill = 0;
    }

    if (!(ctx->flags & SPRINTF_LEFT_ALIGN)) {
        if (!(ctx->flags & SPRINTF_LEAD_ZERO)) {
            for (i = 0; i < fill; i++) {
                BPUT(ctx, ' ');
            }
        }
        if (prefix && *prefix) {
            while (*prefix) {
                BPUT(ctx, *prefix++);
            }
        }
        if (ctx->flags & SPRINTF_LEAD_ZERO) {
            for (i = 0; i < fill; i++) {
                BPUT(ctx, '0');
            }
        }

    } else if (prefix != 0) {
        while (*prefix) {
            BPUT(ctx, *prefix++);
        }
    }
    for (i = 0; i < leadingZeros; i++) {
        BPUT(ctx, '0');
    }
    while (*cp) {
        BPUT(ctx, *cp);
        cp++;
    }
    if (ctx->flags & SPRINTF_LEFT_ALIGN) {
        for (i = 0; i < fill; i++) {
            BPUT(ctx, ' ');
        }
    }
}

static void outFloat(PContext *ctx, char specchar, double value)
{
    double fpart, round, v;
    uchar  *last;
    int    digit, precision;
    int64  ipart;

    if (ctx->format == 'g' || ctx->format == 'G') {
        v = fabs(value);
        if (v < 0.0001 || v > 1000000) {
            outFloatE(ctx, specchar, value);
            return;
        }
    }
    precision = ctx->precision < 0 ? 6 : ctx->precision;

    /*
        Round the value before splitting into integer and fractional parts.
        This handles cases like 0.99 rounded to 1.0.
     */
    round = 0.5;
    for (int i = 0; i < precision; i++) {
        round /= 10.0;
    }
    value += value < 0 ? -round : round;

    ipart = (int64) value;
    outNum(ctx, 10, ipart);

    if (precision > 0) {
        BPUT(ctx, '.');
    }
    fpart = value - (double) ipart;
    if (fpart < 0) {
        fpart = -fpart;
    }
    for (int i = 0; i < precision; i++) {
        fpart *= 10;
        digit = (int) fpart;
        BPUT(ctx, digit + '0');
        fpart -= digit;
    }
    if (ctx->format == 'g') {
        //  Remove trailing zeros and decimal point if precision is 0
        if (ctx->precision < 0) {
            for (last = &ctx->end[-1]; ctx->end > ctx->buf; last--, ctx->end--, ctx->len--) {
                if (*last == '.') {
                    *last = '\0';
                    ctx->end = last;
                    break;
                } else if (*last == '0') {
                    *last = '\0';
                } else {
                    break;
                }
            }
        }
    }
}

static double normalizeScientific(double x, int *exponent)
{
    if (x == 0.0) {
        *exponent = 0;
        return 0.0;
    }
    int    exp = (int) floor(log10(fabs(x)));
    double mantissa = x / pow(10.0, exp);

    // Adjust if mantissa is not in the range [1.0, 10.0)
    if (fabs(mantissa) < 1.0) {
        mantissa *= 10.0;
        exp -= 1;
    }
    *exponent = exp;
    return mantissa;
}

static void outFloatE(PContext *ctx, char specchar, double value)
{
    double fpart, mantissa, round;
    uchar  *last;
    int64  ipart;
    int    digit, exponent, fexp, precision;

    precision = ctx->precision < 0 ? 6 : ctx->precision;
    mantissa = normalizeScientific(value, &exponent);

    /*
        Round the mantissa.
     */
    round = 0.5;
    for (int i = 0; i < precision; i++) {
        round /= 10.0;
    }
    mantissa += (mantissa < 0) ? -round : round;

    if (fabs(mantissa) >= 10.0) {
        mantissa /= 10.0;
        exponent++;
    }

    ipart = (int64) mantissa;
    outNum(ctx, 10, ipart);

    if (precision > 0) {
        BPUT(ctx, '.');
    }
    fpart = mantissa - (double) ipart;
    if (fpart < 0) {
        fpart = -fpart;
    }
    for (int i = 0; i < precision; i++) {
        fpart *= 10;
        //  Round out IEEE imprecision
        digit = (int) (fpart + 1.0e-15);
        BPUT(ctx, digit + '0');
        fpart -= digit;
    }
    if (ctx->format == 'g') {
        //  Remove trailing zeros and decimal point if precision is 0
        if (ctx->precision < 0) {
            for (last = &ctx->end[-1]; ctx->end > ctx->buf; last--, ctx->end--) {
                if (*last == '0' || *last == '.') {
                    *last = '\0';
                } else {
                    break;
                }
            }
        }
    }
    fexp = abs(exponent);
    BPUT(ctx, (ctx->format == 'E' || ctx->format == 'G') ? 'E' : 'e');
    BPUT(ctx, exponent < 0 ? '-' : '+');
    if (fexp >= 100) {
        BPUT(ctx, '0' + fexp / 100);
        BPUT(ctx, '0' + (fexp / 10) % 10);
        BPUT(ctx, '0' + fexp % 10);
    } else {
        BPUT(ctx, '0' + fexp / 10);
        BPUT(ctx, '0' + fexp % 10);
    }
}

/*
    grow the buffer to fit new data. return 1 if the buffer can grow.
    grow using the growby size specified when creating the buffer.
 */
static int growBuf(PContext *ctx)
{
    uchar  *newbuf;
    size_t buflen, newSize;

    buflen = (size_t) (ctx->endbuf - ctx->buf);
    if (ctx->maxsize > 0 && buflen >= (size_t) ctx->maxsize) {
        return R_ERR_BAD_ARGS;
    }
    if (ctx->growBy <= 0) {
        //  User supplied buffer
        return 0;
    }
    if (ctx->growBy > 0 && buflen > MAXSSIZE - (size_t) ctx->growBy) {
        // Integer overflow
        return R_ERR_MEMORY;
    }
    newSize = buflen + (size_t) ctx->growBy;
    if ((newbuf = rAlloc(newSize)) == 0) {
        return R_ERR_MEMORY;
    }
    if (ctx->buf) {
        memcpy(newbuf, ctx->buf, buflen);
        rFree(ctx->buf);
    }
    ctx->end = newbuf + (ctx->end - ctx->buf);
    ctx->buf = newbuf;
    ctx->endbuf = &ctx->buf[newSize];

    /*
        Increase growBy to reduce overhead
     */
    if (ctx->growBy <= (int) (INT_MAX / 2)) {
        ctx->growBy *= 2;
    }
    return 1;
}

#else /* R_OWN_PRINTF */

/*
    Incase you want to map onto the real printf
 */
PUBLIC ssize rVsnprintf(char *buf, size_t maxsize, cchar *spec, va_list args)
{
    return vsnprintf(buf, maxsize, spec, args);
}

PUBLIC ssize rVsaprintf(char **buf, size_t maxsize, cchar *spec, va_list args)
{
    return vasprintf(buf, spec, args);
}
#endif /* R_OWN_PRINTF */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/run.c ************/

/**
    run.c - Shared helper functions for command execution

    This file contains platform-independent argument parsing functions.
    The actual rRun() implementations are in platform-specific files:
    - src/unix.c - Unix/Linux implementation (fork/exec)
    - src/win.c - Windows implementation (CreateProcess)
    - src/vxworks.c - VxWorks implementation (stub)

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



#if R_USE_RUN

/********************************** Forwards **********************************/

static size_t parseArgs(char *args, char **argv, size_t maxArgc);

/************************************ Code ************************************/
/*
    Parse the args and return the count of args.
 */
static size_t parseArgs(char *args, char **argv, size_t maxArgc)
{
    char   *dest, *src, *start;
    size_t argc;
    int    quote;

    /*
        Example     "showColors" red 'light blue' "yellow white" 'Cannot \"render\"'
        Becomes:    ["showColors", "red", "light blue", "yellow white", "Cannot \"render\""]
     */
    for (argc = 0, src = args; src && *src != '\0' && argc < maxArgc; argc++) {
        while (isspace((uchar) * src)) {
            src++;
        }
        if (*src == '\0') {
            break;
        }
        start = dest = src;
        if (*src == '"' || *src == '\'') {
            quote = *src;
            src++;
            dest++;
        } else {
            quote = 0;
        }
        if (argv) {
            argv[argc] = src;
        }
        while (*src) {
            if (*src == '\\' && src[1] && (src[1] == '\\' || src[1] == '"' || src[1] == '\'')) {
                src++;
            } else {
                if (quote) {
                    if (*src == quote && !(src > start && src[-1] == '\\')) {
                        break;
                    }
                } else if (*src == ' ') {
                    break;
                }
            }
            if (argv) {
                *dest++ = *src;
            }
            src++;
        }
        if (*src != '\0') {
            src++;
        }
        if (argv) {
            *dest++ = '\0';
        }
    }
    return argc;
}

/*
    Make an argv array. All args are in a single memory block of which argv
    points to the start. Program name at [0], first arg starts at argv[1].
 */
PUBLIC ssize rMakeArgs(cchar *command, char ***argvp, bool argsOnly)
{
    char   **argv, *vector, *args;
    size_t argc, len, size;

    assert(command);
    if (!command) {
        return R_ERR_BAD_ARGS;
    }
    // Count the args
    len = slen(command) + 1;
    argc = parseArgs((char*) command, NULL, R_RUN_ARGS_MAX);
    if (argsOnly) {
        argc++;
    }
    // Allocate one vector for argv and the actual args themselves
    size = ((argc + 1) * sizeof(char*)) + len;
    if ((vector = (char*) rAlloc(size)) == 0) {
        return R_ERR_MEMORY;
    }
    args = &vector[(argc + 1) * sizeof(char*)];
    scopy(args, size - (size_t) (args - vector), command);
    argv = (char**) vector;

    if (argsOnly) {
        parseArgs(args, &argv[1], argc);
        argv[0] = "";
    } else {
        parseArgs(args, argv, argc);
    }
    if (argc == 0) {
        rFree(vector);
        return R_ERR_BAD_ARGS;
    }
    argv[argc] = 0;
    *argvp = (char**) argv;
    return (ssize) argc;
}

#endif /* R_USE_RUN */
/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/socket.c ************/

/*
    socket.c - Wrapper for TCP/IP Sockets

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



#if R_USE_SOCKET
/*********************************** Locals ***********************************/

#define ME_SOCKET_TIMEOUT    (30 * 1000)
#define ME_HANDSHAKE_TIMEOUT (30 * 1000)
#ifndef ME_SOCKET_MAX
    #define ME_SOCKET_MAX    1000
#endif

static int           activeSockets = 0;
static int           socketLimit = ME_SOCKET_MAX;
static RSocketCustom socketCustom;

/********************************** Forwards **********************************/

static void acceptSocket(RSocket *listen, int mask);
static void socketHandlerFiber(RSocket *sp);
static int getOsError(RSocket *sp);
#if ME_DEBUG
static void traceSocket(Socket fd, cchar *label);
#endif

/************************************ Code ************************************/

PUBLIC RSocket *rAllocSocket(void)
{
    RSocket *sp;

    if ((sp = rAllocType(RSocket)) == 0) {
        return 0;
    }
    sp->fd = INVALID_SOCKET;
    sp->linger = -1;
    return sp;
}

PUBLIC void rFreeSocket(RSocket *sp)
{
    if (!sp) {
        return;
    }
    if (sp->flags & R_SOCKET_SERVER) {
        activeSockets--;
    }
    if (sp->fd != INVALID_SOCKET) {
        rCloseSocket(sp);
    }
#if ME_COM_SSL
    if (sp->tls) {
        rFreeTls(sp->tls);
    }
#endif
    if (sp->wait) {
        rFreeWait(sp->wait);
    }
    rFree(sp->error);
    rFree(sp);
}

PUBLIC void rCloseSocket(RSocket *sp)
{
    char buf[64];

    if (!sp || (sp->flags & R_SOCKET_CLOSED)) {
        return;
    }
#if ME_COM_SSL
    if (sp->tls && !(sp->flags & R_SOCKET_EOF)) {
        rCloseTls(sp->tls);
    }
#endif
    if (sp->fd != INVALID_SOCKET) {
        if (sp->linger != 0 && !(sp->flags & R_SOCKET_EOF)) {
            rSetSocketBlocking(sp, 0);
            while (recv(sp->fd, buf, sizeof(buf), MSG_NOSIGNAL) > 0);
            if (shutdown(sp->fd, SHUT_RDWR) == 0) {
                while (recv(sp->fd, buf, sizeof(buf), MSG_NOSIGNAL) > 0);
            }
        } else {
            /*
                Always call shutdown for server sockets to handle macOS "poisoned socket" case
                Under high load, macOS can hand accept() a socket with internal RST state but readable data.
                close() alone won't send FIN/RST for these poisoned sockets, leaving clients hung.
                shutdown(SHUT_RDWR) forces the kernel to send RST even for poisoned TCBs.
             */
            shutdown(sp->fd, SHUT_RDWR);
        }
        closesocket(sp->fd);
        sp->fd = INVALID_SOCKET;
    }
    sp->flags |= R_SOCKET_CLOSED | R_SOCKET_EOF;

    if (sp->wait) {
        // The resumed party may free the socket -- be careful!
        rResumeWaitFiber(sp->wait, R_READABLE | R_WRITABLE | R_TIMEOUT);
    }
}

PUBLIC void rDisconnectSocket(RSocket *sp)
{
    if (!sp) {
        return;
    }
    if (sp->fd != INVALID_SOCKET) {
        shutdown(sp->fd, SHUT_RDWR);
    }
}

PUBLIC void rResetSocket(RSocket *sp)
{
    if (sp->fd != INVALID_SOCKET) {
        rCloseSocket(sp);
        sp->flags = 0;
    }
}

/*
    This routine is non-blocking and may return 0 (success) while the connection attempt is pending.
    Subsequent reads or writes will discover the connection error.
 */
PUBLIC int rConnectSocket(RSocket *sp, cchar *host, int port, Ticks deadline)
{
    struct addrinfo         hints, *res, *r;
    struct sockaddr_storage peerAddr;
    Socklen                 errorLen, peerLen;
    char                    pbuf[16];
    int                     error, rc;

    if (!host) {
        return rSetSocketError(sp, "Host address required for connection");
    }
    if (deadline <= 0) {
        deadline = rGetTicks() + ME_HANDSHAKE_TIMEOUT;
    }
    if (sp->fd != INVALID_SOCKET) {
        rCloseSocket(sp);
    }
    sp->flags = sp->flags & (R_SOCKET_FAST_CONNECT | R_SOCKET_FAST_CLOSE);

 #if ME_COM_SSL
    if (sp->tls && rConfigTls(sp->tls, 0) < 0) {
        if (!sp->error) {
            return rSetSocketError(sp, "Cannot configure TLS");
        }
        return R_ERR_CANT_CONNECT;
    }
 #endif
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_protocol = IPPROTO_TCP;

    sitosbuf(pbuf, sizeof(pbuf), port, 10);
    if (getaddrinfo(host, pbuf, &hints, &res) != 0) {
        rSetSocketError(sp, "Cannot find address of %s:%d", host, port);
        return R_ERR_BAD_ARGS;
    }
    /*
        Two-pass connection: try IPv4 addresses first, then IPv6.
        We use IPv4 addresses first to optimize for servers that only support IPv4.
     */
    int connected = 0;
    for (int pass = 0; pass < 2 && !connected; pass++) {
        int targetFamily = (pass == 0) ? AF_INET : AF_INET6;

        for (r = res; r; r = r->ai_next) {
            if (r->ai_family != targetFamily) {
                continue;
            }
            if (sp->fd != INVALID_SOCKET) {
                closesocket(sp->fd);
                sp->fd = INVALID_SOCKET;
            }
            if (sp->wait) {
                rFreeWait(sp->wait);
                sp->wait = NULL;
            }
            if ((sp->fd = socket(r->ai_family, r->ai_socktype, r->ai_protocol)) == SOCKET_ERROR) {
                rSetSocketError(sp, "Cannot open socket for %s:%d", host, port);
                continue;
            }
            rSetSocketBlocking(sp, 0);
            sp->wait = rAllocWait((int) sp->fd);

            do {
                rc = connect(sp->fd, r->ai_addr, (socklen_t) r->ai_addrlen);
            } while (rc < 0 && rGetOsError() == EINTR);

            if (rc == 0 || (rc < 0 && (rGetOsError() == EINPROGRESS || rGetOsError() == EAGAIN))) {
#if ME_UNIX_LIKE
                fcntl(sp->fd, F_SETFD, FD_CLOEXEC);
#endif
                sp->activity = rGetTime();

                if (rWaitForIO(sp->wait, R_WRITABLE, deadline) == 0) {
                    continue;
                }
                /*
                    First check SO_ERROR for connection failures. If SO_ERROR is non-zero, connection failed.
                    Then use getpeername to verify connection is truly established. This catches a macOS bug
                    where SO_ERROR returns 0 but the connection isn't actually established.
                 */
                error = 0;
                errorLen = sizeof(error);
                if (getsockopt(sp->fd, SOL_SOCKET, SO_ERROR, (char*) &error, &errorLen) < 0 || error != 0) {
                    continue;
                }
                peerLen = sizeof(peerAddr);
                if (getpeername(sp->fd, (struct sockaddr*) &peerAddr, &peerLen) == 0) {
                    connected = 1;
                    break;
                }
#if MACOSX
                /*
                    MACOSX bug: SO_ERROR returns 0 but the connection isn't actually established yet.
                    This is triggered by another socket writing a large amount of data to the local server that
                    is not read and the connect is closed.
                 */
                for (int i = 0; i < 10; i++) {
                    if (getpeername(sp->fd, (struct sockaddr*) &peerAddr, &peerLen) == 0) {
                        connected = 1;
                        break;
                    }
                    rSleep(10);
                }
                if (connected) {
                    break;
                }
#endif
                continue;
            }
        }
    }
    freeaddrinfo(res);
    if (!connected) {
        if (sp->fd != INVALID_SOCKET) {
            closesocket(sp->fd);
            sp->fd = INVALID_SOCKET;
        }
        if (sp->wait) {
            rFreeWait(sp->wait);
            sp->wait = NULL;
        }
        rSetSocketError(sp, "Cannot connect socket to %s:%d", host, port);
        return R_ERR_CANT_CONNECT;
    }
 #if ME_COM_SSL
    if (sp->tls && rUpgradeTls(sp->tls, sp->fd, host, deadline) < 0) {
        return rSetSocketError(sp, "Cannot upgrade socket to TLS");
    }
 #endif
    if (sp->linger >= 0) {
        struct linger linger;
        linger.l_onoff = 1;
        linger.l_linger = sp->linger;
        setsockopt(sp->fd, SOL_SOCKET, SO_LINGER, (char*) &linger, sizeof(linger));
    }
#if ME_DEBUG
    if (rEmitLog("socket", "debug")) {
        traceSocket(sp->fd, "Client bound to");
    }
#endif
    return 0;
}

PUBLIC int rListenSocket(RSocket *lp, cchar *host, int port, RSocketProc handler, void *arg)
{
    struct addrinfo         hints, *res, *r;
    struct sockaddr_storage addr;
    char                    pbuf[16];
    Socklen                 addrLen;
    int                     family;

    if (!lp || !handler) {
        return R_ERR_BAD_ARGS;
    }
 #if ME_COM_SSL
    if (lp->tls && rConfigTls(lp->tls, 1) < 0) {
        return R_ERR_CANT_INITIALIZE;
    }
 #endif
    //  Resolve the host address to determine the address family and bind address
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // Use wildcard address if host is NULL

    /*
        When dual-stack is available (IPV6_V6ONLY defined), prefer IPv6 to accept both IPv4 and
        IPv6 connections via a single socket. IPV6_V6ONLY is set to 0 below to enable dual-stack.
        When host is a specific IPv4 address like "127.0.0.1", use IPv4 only.
        When dual-stack is not available, use AF_UNSPEC to let the system choose.
        NOTE: macosx dual-stack does not work reliably with localhost, so we use IPv4 only.
     */
#if defined(IPV6_V6ONLY)
    if (smatch(host, "127.0.0.1") || smatch(host, "localhost")) {
        hints.ai_family = AF_INET;   // Explicit IPv4 loopback
    } else {
        hints.ai_family = AF_INET6;  // Dual-stack for all other hosts
    }
#else
    if (smatch(host, "127.0.0.1")) {
        hints.ai_family = AF_INET;   // Explicit IPv4 loopback
    } else {
        hints.ai_family = AF_UNSPEC; // Use resolved address family
    }
#endif

    sitosbuf(pbuf, sizeof(pbuf), port, 10);
    if (getaddrinfo(host, pbuf, &hints, &res) != 0) {
        return rSetSocketError(lp, "Cannot resolve address %s:%d", host ? host : "*", port);
    }
    //  Try each resolved address until one succeeds
    for (r = res; r; r = r->ai_next) {
        family = r->ai_family;

        if ((lp->fd = socket(family, SOCK_STREAM, 0)) == SOCKET_ERROR) {
            continue;
        }
 #if ME_UNIX_LIKE || VXWORKS
        int enable = 1;
        if (setsockopt(lp->fd, SOL_SOCKET, SO_REUSEADDR, (char*) &enable, sizeof(enable)) != 0) {
            rSetSocketError(lp, "Cannot set reuseaddr, errno %d", rGetOsError());
            closesocket(lp->fd);
            lp->fd = INVALID_SOCKET;
            continue;
        }
 #endif
 #if defined(IPV6_V6ONLY)
        //  For IPv6 sockets, disable IPv6-only mode to allow IPv4 connections on dual-stack systems
        if (family == AF_INET6) {
            int no = 0;
            setsockopt(lp->fd, IPPROTO_IPV6, IPV6_V6ONLY, (void*) &no, sizeof(no));
        }
 #endif
        //  Copy the resolved address to our storage
        memcpy(&addr, r->ai_addr, r->ai_addrlen);
        addrLen = (Socklen) r->ai_addrlen;

        if (bind(lp->fd, (struct sockaddr*) &addr, addrLen) < 0) {
            rSetSocketError(lp, "Cannot bind address %s:%d, errno %d", host ? host : "*", port, rGetOsError());
            closesocket(lp->fd);
            lp->fd = INVALID_SOCKET;
            continue;
        }
#if ME_DEBUG
        if (rEmitLog("socket", "debug")) {
            traceSocket(lp->fd, "Server bound to");
        }
#endif
        break;
    }
    freeaddrinfo(res);

    if (lp->fd == INVALID_SOCKET) {
        return R_ERR_CANT_OPEN;
    }
    lp->flags |= R_SOCKET_LISTENER;
    if (listen(lp->fd, SOMAXCONN) < 0) {
        rSetSocketError(lp, "Listen error %d", rGetOsError());
        closesocket(lp->fd);
        return R_ERR_CANT_OPEN;
    }
 #if ME_UNIX_LIKE
    fcntl(lp->fd, F_SETFD, FD_CLOEXEC);
 #endif
    rSetSocketBlocking(lp, 0);

    assert(!lp->wait);
    lp->wait = rAllocWait((int) lp->fd);
    lp->activity = rGetTime();
    lp->handler = handler;
    lp->arg = arg;

    //  Run acceptSocket on main fiber when readable (fast path for accept)
    rSetWaitHandler(lp->wait, (RWaitProc) acceptSocket, lp, R_READABLE, 0, R_WAIT_MAIN_FIBER);
    return 0;
}

/*
    This routine is called by the accept wait handler when a new connection is accepted.
    Runs on main fiber - only does accept() and basic field assignment.
    All socket setup syscalls moved to socketHandlerFiber.

    With edge-triggered I/O (EV_CLEAR), we must loop to drain the entire listen backlog.
    Otherwise connections can accumulate and cause ECONNRESET errors under load.
 */
static void acceptSocket(RSocket *listen, int mask)
{
    RSocket                 *sp;
    struct sockaddr_storage addr;
    Socklen                 addrLen;
    Socket                  fd;

    //  Edge-triggered: drain all pending connections
    do {
        addrLen = sizeof(addr);
        fd = accept(listen->fd, (struct sockaddr*) &addr, &addrLen);
        if (fd == SOCKET_ERROR) {
            if (rGetOsError() != EAGAIN) {
                rLog("error", "socket", "Accept failed, errno %d", rGetOsError());
            }
            break;
        }
        //  Check limit AFTER accept (must drain backlog for edge-triggered)
        if (activeSockets >= socketLimit) {
            rLog("error", "socket", "Too many active sockets (%d/%d), rejecting connection", activeSockets,
                 socketLimit);
            closesocket(fd);
            continue;  // Keep draining backlog
        }
        if ((sp = rAllocSocket()) == 0) {
            // Memory errors handled globally
            rError("socket", "Cannot allocate socket");
            closesocket(fd);
            break;
        }
        activeSockets++;

        //  Minimal setup on main fiber - just assign fields (no syscalls)
        sp->fd = fd;
        sp->handler = listen->handler;
        sp->arg = listen;  // Temporarily store listen socket for fiber to access TLS config
        sp->flags |= R_SOCKET_SERVER;

        //  Run via a fiber from the pool for socket setup and handler
        if (rSpawnFiber("socket", (RFiberProc) socketHandlerFiber, sp) < 0) {
            rFreeSocket(sp);
        }
    } while (1);
    //  No re-arm needed - edge-triggered event will fire on next connection after backlog is drained
}

/*
    Socket handler fiber - runs in spawned fiber.
    Performs all socket setup syscalls and TLS work off the main fiber.
 */
static void socketHandlerFiber(RSocket *sp)
{
    RSocket *listen;

    listen = (RSocket*) sp->arg;   // Temporarily stored listen socket

    //  Socket setup (syscalls done in fiber to keep main fiber fast)
    sp->activity = rGetTime();
    sp->wait = rAllocWait((int) sp->fd);

    rSetSocketBlocking(sp, 0);
    rSetSocketNoDelay(sp, 1);

 #if ME_UNIX_LIKE
    fcntl(sp->fd, F_SETFD, FD_CLOEXEC);
 #endif
 #if MACOSX
    //  Disable SIGPIPE on this socket. MSG_NOSIGNAL is not supported on macosx.
    int one = 1;
    setsockopt(sp->fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
 #endif

 #if ME_COM_SSL
    if (listen->tls) {
        sp->tls = rAllocTls(sp);
        rAcceptTls(sp->tls, listen->tls);
        if (rUpgradeTls(sp->tls, sp->fd, 0, rGetTicks() + ME_HANDSHAKE_TIMEOUT) < 0) {
            rSetSocketError(sp, "Cannot upgrade socket to TLS");
            rFreeSocket(sp);
            return;
        }
    }
 #endif
    sp->arg = listen->arg;
    (sp->handler)(sp->arg, sp);
}

/*
    Read from a socket. If in blocking mode, this will block until data is available.
    Peferable to use rReadSocket which can wait without blocking via fiber coroutines until a deadline is reached.
 */
PUBLIC ssize rReadSocketSync(RSocket *sp, char *buf, size_t bufsize)
{
    ssize bytes;
    int   error;

    assert(buf);
    assert(bufsize > 0);

    if (!sp || !buf || bufsize <= 0) {
        return R_ERR_BAD_ARGS;
    }
    if (sp->flags & R_SOCKET_EOF) {
        return R_ERR_CANT_READ;
    }
#if ME_COM_SSL
    if (sp->tls) {
        if ((bytes = rReadTls(sp->tls, buf, bufsize)) < 0) {
            sp->flags |= R_SOCKET_EOF;
        }
        return bytes;
    }
#endif
    while (1) {
        bytes = recv(sp->fd, buf, (uint) bufsize, MSG_NOSIGNAL);
        if (bytes < 0) {
            error = getOsError(sp);
            if (error == EINTR) {
                continue;
            } else if (error == EAGAIN || error == EWOULDBLOCK) {
                bytes = 0;                        /* No data available */
            } else if (error == ECONNRESET) {
                sp->flags |= R_SOCKET_EOF;        /* Disorderly disconnect */
                bytes = R_ERR_CANT_READ;
            } else {
                sp->flags |= R_SOCKET_EOF;        /* Some other error */
                bytes = -error;
            }
        } else if (bytes == 0) {                  /* EOF */
            sp->flags |= R_SOCKET_EOF;
            bytes = R_ERR_CANT_READ;
        }
        break;
    }
    sp->activity = rGetTime();
    return bytes;
}

PUBLIC ssize rReadSocket(RSocket *sp, char *buf, size_t bufsize, Ticks deadline)
{
    ssize nbytes;

    if (!sp || !buf || bufsize <= 0 || bufsize > MAXSSIZE / 2) {
        return R_ERR_BAD_ARGS;
    }
    if (deadline <= 0) {
        deadline = rGetTicks() + ME_HANDSHAKE_TIMEOUT;
    }
    while (1) {
        nbytes = rReadSocketSync(sp, buf, bufsize);
        if (nbytes != 0) {
            return nbytes;
        }
        if (rWaitForIO(sp->wait, R_READABLE, deadline) == 0) {
            return R_ERR_TIMEOUT;
        }
    }
}

PUBLIC ssize rWriteSocket(RSocket *sp, cvoid *buf, size_t bufsize, Ticks deadline)
{
    ssize  written;
    size_t toWrite;

    if (deadline <= 0) {
        deadline = rGetTicks() + ME_HANDSHAKE_TIMEOUT;
    }
    for (toWrite = bufsize; toWrite > 0; ) {
        written = rWriteSocketSync(sp, buf, toWrite);
        if (written < 0) {
            return written;
        }
        buf = (char*) buf + written;
        toWrite -= (size_t) written;
        if (toWrite > 0) {
            // rWriteSocketSync has already blocked until data can be written
            if (rWaitForIO(sp->wait, R_WRITABLE, deadline) == 0) {
                return R_ERR_TIMEOUT;
            }
        }
    }
    if (sp->flags & R_SOCKET_EOF) {
        return R_ERR_CANT_WRITE;
    }
    return (ssize) bufsize;
}

/*
    Write to a socket. If in blocking mode, this will block until data can be written
    Peferable to use rWriteSocket which can wait without blocking via fiber coroutines until a deadline is reached.
 */
PUBLIC ssize rWriteSocketSync(RSocket *sp, cvoid *buf, size_t bufsize)
{
    size_t len;
    ssize  written, bytes;
    int    error;

    if (!sp || !buf || bufsize < 0) {
        return R_ERR_BAD_ARGS;
    }
    if (sp->flags & R_SOCKET_EOF) {
        bytes = R_ERR_CANT_WRITE;
    } else {
#if ME_COM_SSL
        if (sp->tls) {
            if ((bytes = rWriteTls(sp->tls, buf, bufsize)) < 0) {
                sp->flags |= R_SOCKET_EOF;
            }
            return bytes;
        }
#endif
        for (len = bufsize, bytes = 0; len > 0; ) {
            written = send(sp->fd, &((char*) buf)[bytes], (uint) len, MSG_NOSIGNAL);
            if (written < 0) {
                error = getOsError(sp);
                if (error == EINTR) {
                    continue;
                } else if (error == EAGAIN || error == EWOULDBLOCK) {
                    return bytes;
                } else {
                    return -error;
                }
            }
            len -= (size_t) written;
            bytes += (ssize) written;
        }
    }
    sp->activity = rGetTime();
    return bytes;
}

/*
    Set a socket into blocking I/O mode. from a socket.
    Sockets are opened in non-blocking mode by default.
 */
PUBLIC void rSetSocketBlocking(RSocket *sp, bool on)
{
#if ME_WIN_LIKE
    {
        ulong flag = on ? 0 : 1;
        ioctlsocket(sp->fd, FIONBIO, (ulong*) &flag);
    }
#elif VXWORKS
    {
        int flag = on ? 0 : 1;
        ioctl(sp->fd, FIONBIO, (int) &flag);
    }
#else
    if (on) {
        fcntl(sp->fd, F_SETFL, fcntl(sp->fd, F_GETFL) & ~O_NONBLOCK);
    } else {
        fcntl(sp->fd, F_SETFL, fcntl(sp->fd, F_GETFL) | O_NONBLOCK);
    }
#endif
}

static int getOsError(RSocket *sp)
{
#if ME_WIN_LIKE
    int rc;
    switch (rc = WSAGetLastError()) {
    case WSAEINTR:
        return EINTR;
    case WSAENETDOWN:
        return ENETDOWN;
    case WSAEWOULDBLOCK:
        return EWOULDBLOCK;
    case WSAEPROCLIM:
        return EAGAIN;
    case WSAECONNRESET:
    case WSAECONNABORTED:
        return ECONNRESET;
    case WSAECONNREFUSED:
        return ECONNREFUSED;
    case WSAEADDRINUSE:
        return EADDRINUSE;
    default:
        return EINVAL;
    }
#else
    return errno;
#endif
}

PUBLIC bool rIsSocketClosed(RSocket *sp)
{
    return sp ? sp->flags & R_SOCKET_CLOSED : 1;
}

PUBLIC bool rIsSocketEof(RSocket *sp)
{
    return sp ? sp->flags & R_SOCKET_EOF : 1;
}

PUBLIC Socket rGetSocketHandle(RSocket *sp)
{
    return sp ? sp->fd : -1;
}

PUBLIC bool rIsSocketSecure(RSocket *sp)
{
    return sp ? sp->tls != 0 : 0;
}

#if ME_COM_SSL
PUBLIC void rSetTls(RSocket *sp)
{
    if (sp && !sp->tls) {
        sp->tls = rAllocTls(sp);
    }
}

PUBLIC void rSetSocketCerts(RSocket *sp, cchar *ca, cchar *key, cchar *cert, cchar *revoke)
{
    if (!sp->tls) {
        rSetTls(sp);
    }
    rSetTlsCerts(sp->tls, ca, key, cert, revoke);
    sp->hasCert = (key && cert) ? 1 : 0;
}

PUBLIC void rSetSocketDefaultCerts(cchar *ca, cchar *key, cchar *cert, cchar *revoke)
{
    rSetTlsDefaultCerts(ca, key, cert, revoke);
}

PUBLIC void rSetSocketCiphers(RSocket *sp, cchar *ciphers)
{
    rSetTlsCiphers(sp->tls, ciphers);
}

PUBLIC void rSetSocketDefaultCiphers(cchar *ciphers)
{
    rSetTlsDefaultCiphers(ciphers);
}

PUBLIC void rSetSocketLinger(RSocket *sp, int linger)
{
    if (sp) {
        sp->linger = linger;
    }
}

PUBLIC void rSetSocketNoDelay(RSocket *sp, int enable)
{
    int value = enable ? 1 : 0;

    if (sp && sp->fd != INVALID_SOCKET) {
        setsockopt(sp->fd, IPPROTO_TCP, TCP_NODELAY, (char*) &value, sizeof(value));
    }
}

PUBLIC void rSetSocketVerify(RSocket *sp, int verifyPeer, int verifyIssuer)
{
    if (!sp->tls) {
        rSetTls(sp);
    }
    rSetTlsVerify(sp->tls, verifyPeer, verifyIssuer);
}

PUBLIC void rSetSocketDefaultVerify(int verifyPeer, int verifyIssuer)
{
    rSetTlsDefaultVerify(verifyPeer, verifyIssuer);
}

PUBLIC bool rIsSocketConnected(RSocket *sp)
{
    if (sp->flags & R_SOCKET_CLOSED) {
        return 0;
    }
    if (sp->tls) {
        return rIsTlsConnected(sp->tls);
    }
    return 1;
}
#endif

PUBLIC void rSetSocketWaitMask(RSocket *sp, int64 mask, Ticks deadline)
{
    if (sp) {
        rSetWaitMask(sp->wait, mask, deadline);
    }
}

PUBLIC cchar *rGetSocketError(RSocket *sp)
{
    if (sp->error) {
        return sp->error;
    }
    return "";
}

PUBLIC int rSetSocketError(RSocket *sp, cchar *fmt, ...)
{
    va_list ap;

    if (!sp->error) {
        va_start(ap, fmt);
        sp->error = sfmtv(fmt, ap);
        va_end(ap);
        // Debug build only
        rDebug("socket", "%s", sp->error);
    }
    return R_ERR_CANT_COMPLETE;
}

PUBLIC RWait *rGetSocketWait(RSocket *sp)
{
    return sp ? sp->wait : NULL;
}

PUBLIC RSocketCustom rGetSocketCustom(void)
{
    return socketCustom;
}

PUBLIC void rSetSocketCustom(RSocketCustom custom)
{
    socketCustom = custom;
}

PUBLIC int rGetSocketLimit(void)
{
    return socketLimit;
}

PUBLIC void rSetSocketLimit(int limit)
{
    socketLimit = limit;
}

PUBLIC bool rCheckInternet(void)
{
    struct addrinfo hints, *res = NULL;
    int             rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rc = getaddrinfo("www.google.com", "http", &hints, &res);
    if (res) {
        freeaddrinfo(res);
    }
    return rc == 0;
}

/*
    Return a numerical IP address and port for the local bound address
 */
PUBLIC int rGetSocketAddr(RSocket *sp, char *ipbuf, size_t ipbufLen, int *port)
{
    struct sockaddr_storage addrStorage;
    struct sockaddr         *addr;
    Socklen                 addrLen;

#if (ME_UNIX_LIKE || ME_WIN_LIKE)
    char service[NI_MAXSERV];
#endif

    // Add input validation
    if (!sp || !ipbuf || ipbufLen <= 0) {
        return R_ERR_BAD_ARGS;
    }

    *port = 0;
    *ipbuf = '\0';

    addr = (struct sockaddr*) &addrStorage;
    addrLen = sizeof(addrStorage);
    if (getsockname(sp->fd, addr, &addrLen) < 0) {
        return R_ERR_CANT_COMPLETE;
    }

#if (ME_UNIX_LIKE || ME_WIN_LIKE)
#if ME_WIN_LIKE || defined(IN6_IS_ADDR_V4MAPPED)
    if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6*) addr;
        if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
            struct sockaddr_in addr4;
            memset(&addr4, 0, sizeof(addr4));
            addr4.sin_family = AF_INET;
            addr4.sin_port = addr6->sin6_port;
            memcpy(&addr4.sin_addr.s_addr, addr6->sin6_addr.s6_addr + 12, sizeof(addr4.sin_addr.s_addr));
            memcpy(addr, &addr4, sizeof(addr4));
            addrLen = sizeof(addr4);
        }
    }
#endif
    if (getnameinfo(addr, addrLen, ipbuf, (socklen_t) ipbufLen, service, sizeof(service),
                    NI_NUMERICHOST | NI_NUMERICSERV | NI_NOFQDN)) {
        return R_ERR_BAD_VALUE;
    }
    *port = atoi(service);

#else /* !UNIX && !WIN */
    struct sockaddr_in *sa;

#if HAVE_NTOA_R
    sa = (struct sockaddr_in*) addr;
    inet_ntoa_r(sa->sin_addr, ipbuf, ipbufLen);
#else
    uchar *cp;
    sa = (struct sockaddr_in*) addr;
    cp = (uchar*) &sa->sin_addr;
    sfmtbuf(ipbuf, ipbufLen, "%d.%d.%d.%d", cp[0], cp[1], cp[2], cp[3]);
#endif
    *port = ntohs(sa->sin_port);
#endif
    return 0;
}

#if ME_DEBUG
/*
    Debug routine to display the socket's bound address, port and family
 */
static void traceSocket(Socket fd, cchar *label)
{
    struct sockaddr_storage boundAddr;
    Socklen                 boundLen;
    char                    ip[INET6_ADDRSTRLEN];
    int                     boundPort;

    boundLen = sizeof(boundAddr);
    boundPort = 0;
    scopy(ip, sizeof(ip), "unknown");

    if (getsockname(fd, (struct sockaddr*) &boundAddr, &boundLen) == 0) {
        if (boundAddr.ss_family == AF_INET) {
            struct sockaddr_in *s = (struct sockaddr_in*) &boundAddr;
            inet_ntop(AF_INET, &s->sin_addr, ip, sizeof(ip));
            boundPort = ntohs(s->sin_port);
        } else if (boundAddr.ss_family == AF_INET6) {
            struct sockaddr_in6 *s = (struct sockaddr_in6*) &boundAddr;
            inet_ntop(AF_INET6, &s->sin6_addr, ip, sizeof(ip));
            boundPort = ntohs(s->sin6_port);
        } else {
            rDebug("socket", "%s unknown address family %d", label, boundAddr.ss_family);
            return;
        }
        rDebug("socket", "%s %s:%d %s", label, ip, boundPort, boundAddr.ss_family == AF_INET ? "IPv4" : "IPv6");
    }
}
#endif

#if ME_HAS_SENDFILE
/*
    Send a file over a socket using zero-copy sendfile.
    Returns the number of bytes sent, or -1 on error with errno set.
 */
PUBLIC ssize rSendFile(RSocket *sock, int fd, Offset offset, size_t len)
{
    RWait *wp;

    wp = sock->wait;
    if (!wp) {
        sock->wait = wp = rAllocWait((int) sock->fd);
    }

#if LINUX
    off_t off;
    ssize total, remaining, written;

    total = 0;
    remaining = (ssize) len;
    off = offset;

    while (remaining > 0) {
        written = sendfile(sock->fd, fd, &off, (size_t) remaining);
        if (written < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                if (rWaitForIO(wp, R_WRITABLE, 0) == 0) {
                    break;
                }
                continue;
            }
            return (total > 0) ? total : -1;
        }
        total += written;
        remaining -= written;
    }
    return total;

#elif MACOSX || FREEBSD
    off_t written, total, remaining;
    int   rc;

    total = 0;
    remaining = (off_t) len;

    while (remaining > 0) {
        written = remaining;
        rc = sendfile(fd, sock->fd, offset + total, &written, NULL, 0);
        if (rc < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                if (written > 0) {
                    total += written;
                    remaining -= written;
                } else {
                    if (rWaitForIO(wp, R_WRITABLE, 0) == 0) {
                        break;
                    }
                }
                continue;
            }
            return (total > 0) ? (ssize) total : -1;
        }
        total += written;
        remaining -= written;
        if (written == 0) {
            break;
        }
    }
    return (ssize) total;

#else
    errno = ENOSYS;
    return -1;
#endif
}
#endif /* ME_HAS_SENDFILE */

#endif /* R_USE_SOCKET */
/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/string.c ************/

/**
    string.c - String routines r for embedded programming

    This module provides safe replacements for the standard string library.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



#if R_USE_STRING
/*********************************** Locals ***********************************/

#define HASH_PRIME          0x01000193

#define R_STRING_ALLOC_SIZE 256

/************************************ Code ************************************/
/*
    Convert an integer to a string buffer with the specified radix
 */
PUBLIC char *sitosbuf(char *buf, size_t size, int64 value, int radix)
{
    uint64 uval;
    char   *cp, *end;
    char   digits[] = "0123456789ABCDEF";
    int    negative;

    if (radix <= 0) {
        radix = 10;
    }
    if (size < 2) {
        return 0;
    }
    //  Fill the buffer from the end forward
    end = cp = &buf[size];
    *--cp = '\0';

    if (value < 0) {
        uval = (value == INT64_MIN) ? (uint64_t) INT64_MAX + 1 : (uint64_t) (-value);
        negative = 1;
        size--;
    } else {
        uval = (uint64_t) value;
        negative = 0;
    }
    do {
        if (cp == buf) return 0; // Out of space
        *--cp = digits[uval % (uint64) radix];
        uval /= (uint64) radix;
    } while (uval > 0);

    if (negative) {
        if (cp == buf) {
            return 0;
        }
        *--cp = '-';
    }
    if (buf < cp) {
        // Move the null too
        memmove(buf, cp, (size_t) (end - cp));
    }
    return buf;
}

/*
    Format a number as a string. Support radix 10 and 16.
 */
PUBLIC char *sitosx(int64 value, int radix)
{
    char num[32];

    if (sitosbuf(num, sizeof(num), value, radix) == 0) {
        return sclone("");
    }
    return sclone(num);
}

PUBLIC char *sitos(int64 value)
{
    return sitosx(value, 10);
}

PUBLIC char *scamel(cchar *str)
{
    char   *ptr;
    size_t size, len;

    if (str == 0) {
        str = "";
    }
    len = slen(str);
    size = len + 1;
    if ((ptr = rAlloc(size)) != 0) {
        memcpy(ptr, str, len);
        ptr[len] = '\0';
        ptr[0] = (char) tolower((uchar) ptr[0]);
    }
    return ptr;
}

PUBLIC int scaselesscmp(cchar *s1, cchar *s2)
{
    if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    return sncaselesscmp(s1, s2, max(slen(s1), slen(s2)));
}

PUBLIC bool scaselessmatch(cchar *s1, cchar *s2)
{
    return scaselesscmp(s1, s2) == 0;
}

PUBLIC char *schr(cchar *s, int c)
{
    if (s == 0) {
        return 0;
    }
    return strchr(s, c);
}

PUBLIC char *sncontains(cchar *str, cchar *pattern, size_t limit)
{
    cchar  *cp, *s1, *s2;
    size_t lim;

    if (str == 0) {
        return 0;
    }
    if (pattern == 0 || *pattern == '\0') {
        return 0;
    }
    if (limit == 0 || limit >= MAXINT) {
        limit = MAXINT;
    }
    for (cp = str; limit > 0 && *cp; cp++, limit--) {
        s1 = cp;
        s2 = pattern;
        for (lim = limit; *s1 && *s2 && (*s1 == *s2) && lim > 0; lim--) {
            s1++;
            s2++;
        }
        if (*s2 == '\0') {
            return (char*) cp;
        }
    }
    return 0;
}

PUBLIC char *scontains(cchar *str, cchar *pattern)
{
    return sncontains(str, pattern, 0);
}

PUBLIC char *sncaselesscontains(cchar *str, cchar *pattern, size_t limit)
{
    cchar  *cp, *s1, *s2;
    size_t lim;

    if (str == 0) {
        return 0;
    }
    if (pattern == 0 || *pattern == '\0') {
        return 0;
    }
    if (limit == 0 || limit >= MAXINT) {
        limit = MAXINT;
    }
    for (cp = str; limit > 0 && *cp; cp++, limit--) {
        s1 = cp;
        s2 = pattern;
        for (lim = limit; *s1 && *s2 && (tolower(*s1) == tolower(*s2)) && lim > 0; lim--) {
            s1++;
            s2++;
        }
        if (*s2 == '\0') {
            return (char*) cp;
        }
    }
    return 0;
}

/*
    Copy a string into a buffer. Always ensure it is null terminated.
 */
PUBLIC ssize scopy(char *dest, size_t destMax, cchar *src)
{
    size_t len;

    if (!dest || destMax <= 0 || (destMax > MAXINT - 8)) {
        return R_ERR_BAD_ARGS;
    }
    len = slen(src);
    if (destMax <= len) {
        // Must ensure room for null
        return R_ERR_WONT_FIT;
    }
    if (len > 0) {
        memcpy(dest, src, len);
    }
    dest[len] = '\0';
    return (ssize) len;
}

PUBLIC char *sclone(cchar *str)
{
    char   *ptr;
    size_t size, len;

    if (str == 0) {
        str = "";
    }
    len = slen(str);
    size = len + 1;
    if ((ptr = rAlloc(size)) != 0) {
        memcpy(ptr, str, len);
        ptr[len] = '\0';
    }
    return ptr;
}

/*
    Preserve NULLs
 */
PUBLIC char *scloneNull(cchar *str)
{
    if (str == NULL) return NULL;
    return sclone(str);
}

/*
    Only clone if the string is defined and not empty
 */
PUBLIC char *scloneDefined(cchar *str)
{
    if (str == NULL || *str == '\0') return NULL;
    return sclone(str);
}

PUBLIC int scmp(cchar *s1, cchar *s2)
{
    if (s1 == s2) {
        return 0;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    return sncmp(s1, s2, max(slen(s1), slen(s2)));
}

PUBLIC cchar *sends(cchar *str, cchar *suffix)
{
    size_t strLen, suffixLen, offset;

    if (str == 0 || suffix == 0) {
        return 0;
    }
    strLen = slen(str);
    suffixLen = slen(suffix);
    if (strLen < suffixLen) {
        return 0;
    }
    offset = strLen - suffixLen;
    if (strcmp(&str[offset], suffix) == 0) {
        return &str[offset];
    }
    return 0;
}

PUBLIC char *sfmt(cchar *format, ...)
{
    va_list ap;
    char    *buf;

    if (format == 0) {
        format = "%s";
    }
    va_start(ap, format);
    rVsaprintf(&buf, 0, format, ap);
    va_end(ap);
    return buf;
}

PUBLIC char *sfmtv(cchar *format, va_list arg)
{
    char *buf;

    rVsaprintf(&buf, 0, format, arg);
    return buf;
}

PUBLIC char *sfmtbuf(char *buf, size_t bufsize, cchar *fmt, ...)
{
    va_list ap;
    ssize   rc;

    assert(buf);
    assert(fmt);
    assert(bufsize > 0);

    if (!buf || !fmt || bufsize <= 0) {
        return 0;
    }
    va_start(ap, fmt);
    rc = rVsnprintf(buf, bufsize, fmt, ap);
    va_end(ap);
    if (rc < 0 || (size_t) rc >= bufsize) {
        //  Error or truncated
        return NULL;
    }
    return buf;
}

PUBLIC char *sfmtbufv(char *buf, size_t bufsize, cchar *fmt, va_list arg)
{
    ssize rc;

    assert(buf);
    assert(fmt);
    assert(bufsize > 0);

    if (!buf || !fmt || bufsize <= 0) {
        return 0;
    }
    rc = rVsnprintf(buf, bufsize, fmt, arg);
    if (rc < 0 || (size_t) rc >= bufsize) {
        //  Error or truncated
        return NULL;
    }
    return buf;
}

/*
    Simple case sensitive hash function.
 */
PUBLIC uint shash(cchar *cname, size_t len)
{
    uint hash;

    assert(cname);

    if (cname == 0 || len > MAXINT) {
        return 0;
    }
    hash = (uint) len;
    while (len-- > 0) {
        hash = hash ^ (uint) (uchar) (*cname++);
        hash = hash * HASH_PRIME;
    }
    return hash;
}

/*
    Simple case insensitive hash function.
 */
PUBLIC uint shashlower(cchar *cname, size_t len)
{
    uint hash;

    assert(cname);

    if (cname == 0 || len > MAXINT) {
        return 0;
    }
    hash = (uint) len;
    while (len-- > 0) {
        hash ^= (uint) tolower((uchar) * cname++);
        hash *= HASH_PRIME;
    }
    return hash;
}

PUBLIC char *sjoin(cchar *str, ...)
{
    va_list ap;
    char    *result;

    va_start(ap, str);
    result = sjoinv(str, ap);
    va_end(ap);
    return result;
}

PUBLIC char *sjoinfmt(cchar *str, cchar *fmt, ...)
{
    va_list ap;
    char    *buf, *result;

    va_start(ap, fmt);
    rVsaprintf(&buf, 0, fmt, ap);
    va_end(ap);
    result = sjoin(str, buf, NULL);
    rFree(buf);
    return result;
}

PUBLIC char *sjoinv(cchar *buf, va_list args)
{
    va_list ap;
    char    *dest, *str, *dp;
    size_t  bytes, len, required;

    va_copy(ap, args);
    required = 1;
    if (buf) {
        if (required + slen(buf) > MAXINT) {
            return 0;
        }
        required += slen(buf);
    }
    str = va_arg(ap, char*);
    while (str) {
        bytes = slen(str);
        if (required > MAXINT - bytes) {
            va_end(ap);
            return 0;
        }
        required += bytes;
        str = va_arg(ap, char*);
    }
    if ((dest = rAlloc(required)) == 0) {
        va_end(ap);
        return 0;
    }
    dp = dest;
    if (buf) {
        len = slen(buf);
        memcpy(dp, buf, len);
        dp += len;
    }
    va_copy(ap, args);
    str = va_arg(ap, char*);
    while (str) {
        len = slen(str);
        memcpy(dp, str, len);
        dp += len;
        str = va_arg(ap, char*);
    }
    *dp = '\0';
    va_end(ap);
    return dest;
}

PUBLIC ssize sjoinbuf(char *buf, size_t bufsize, cchar *a, cchar *b)
{
    size_t len, len2, remaining, total;

    len = (size_t) sncopy(buf, bufsize, a, slen(a));
    remaining = (len < bufsize) ? bufsize - len : 0;
    len2 = (size_t) sncopy(&buf[len], remaining, b, slen(b));

    // Check for overflow when adding
    total = len + len2;
    if (total > MAXSSIZE) {
        return MAXSSIZE;
    }
    return (ssize) total;
}

PUBLIC char *sjoinArgs(int argc, cchar **argv, cchar *sep)
{
    RBuf *buf;
    int  i;

    if (sep == 0) {
        sep = "";
    }
    buf = rAllocBuf(R_STRING_ALLOC_SIZE);
    for (i = 0; i < argc; i++) {
        rPutToBuf(buf, "%s%s", argv[i], sep);
    }
    if (argc > 0) {
        rAdjustBufEnd(buf, -1);
    }
    return rBufToStringAndFree(buf);
}

PUBLIC size_t slen(cchar *s)
{
    return s ? strlen(s) : 0;
}

PUBLIC char *slower(char *str)
{
    char *cp;

    if (str) {
        for (cp = str; *cp; cp++) {
            if (isupper((uchar) * cp)) {
                *cp = (char) tolower((uchar) * cp);
            }
        }
    }
    return str;
}

PUBLIC bool smatch(cchar *s1, cchar *s2)
{
    return scmp(s1, s2) == 0;
}

/*
    Secure constant time comparison
 */
PUBLIC bool smatchsec(cchar *s1, cchar *s2)
{
    size_t i, len1, len2, maxLen;
    uchar  c, lengthDiff;

    len1 = slen(s1);
    len2 = slen(s2);

    /* Record if lengths differ, but don't return early */
    lengthDiff = (uchar) (len1 != len2);

    /* Always compare the maximum length to ensure constant time */
    maxLen = (len1 > len2) ? len1 : len2;

    /* Perform comparison over the full maximum length */
    for (i = 0, c = 0; i < maxLen; i++) {
        uchar c1 = (i < len1) ? (uchar) s1[i] : 0;
        uchar c2 = (i < len2) ? (uchar) s2[i] : 0;
        c |= c1 ^ c2;
    }

    /* Include length difference in the final result */
    c |= lengthDiff;

    return !c;
}

PUBLIC int sncaselesscmp(cchar *s1, cchar *s2, size_t n)
{
    int rc;

    if (n > MAXINT) {
        return 0;
    }
    if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    for (rc = 0; n > 0 && *s1 && rc == 0; s1++, s2++, n--) {
        rc = tolower((uchar) * s1) - tolower((uchar) * s2);
    }
    if (rc) {
        return (rc > 0) ? 1 : -1;
    } else if (n == 0) {
        return 0;
    } else if (*s1 == '\0' && *s2 == '\0') {
        return 0;
    } else if (*s1 == '\0') {
        return -1;
    } else if (*s2 == '\0') {
        return 1;
    }
    return 0;
}

/*
    Clone a sub-string of a specified length. The null is added after the length.
    The given len can be longer than the source string.
 */
PUBLIC char *snclone(cchar *str, size_t len)
{
    char   *ptr;
    size_t size, l;

    if (str == 0) {
        str = "";
    }
    l = slen(str);
    len = min(l, len);
    size = len + 1;
    if ((ptr = rAlloc(size)) != 0) {
        memcpy(ptr, str, len);
        ptr[len] = '\0';
    }
    return ptr;
}

/*
    Case sensitive string comparison. Limited by length
 */
PUBLIC int sncmp(cchar *s1, cchar *s2, size_t n)
{
    int rc;

    if (n > MAXINT) {
        return 0;
    }
    if (s1 == 0 && s2 == 0) {
        return 0;
    } else if (s1 == 0) {
        return -1;
    } else if (s2 == 0) {
        return 1;
    }
    for (rc = 0; n > 0 && *s1 && rc == 0; s1++, s2++, n--) {
        rc = *s1 - *s2;
    }
    if (rc) {
        return (rc > 0) ? 1 : -1;
    } else if (n == 0) {
        return 0;
    } else if (*s1 == '\0' && *s2 == '\0') {
        return 0;
    } else if (*s1 == '\0') {
        return -1;
    } else if (*s2 == '\0') {
        return 1;
    }
    return 0;
}

/*
    This routine copies at most "count" characters from a string. It ensures the result is always null terminated and
    the buffer does not overflow. Returns R_ERR_WONT_FIT if the buffer is too small.
 */
PUBLIC ssize sncopy(char *dest, size_t destMax, cchar *src, size_t count)
{
    size_t len;

    if (!dest || !src || dest == src || count > MAXINT || destMax == 0 || destMax > MAXINT) {
        return R_ERR_BAD_ARGS;
    }
    len = min(slen(src), count);
    if (destMax <= len || destMax < 1) {
        return R_ERR_WONT_FIT;
    }
    if (len > 0) {
        memcpy(dest, src, len);
        dest[len] = '\0';
    } else {
        *dest = '\0';
        len = 0;
    }
    return (ssize) len;
}

PUBLIC ssize sncat(char *dest, size_t destMax, cchar *src)
{
    size_t count, len;

    if (!dest || !src || dest == src || destMax == 0 || destMax > MAXINT) {
        return R_ERR_BAD_ARGS;
    }
    len = slen(dest);
    if (len >= destMax) {
        return R_ERR_WONT_FIT;
    }
    count = slen(src);
    if (count >= destMax - len) {
        return R_ERR_WONT_FIT;
    }
    memcpy(&dest[len], src, count);
    dest[len + count] = '\0';
    return (ssize) (len + count);
}

PUBLIC bool snumber(cchar *s)
{
    if (!s) {
        return 0;
    }
    if (*s == '-' || *s == '+') {
        s++;
    }
    return *s && strspn(s, "1234567890") == strlen(s);
}

PUBLIC bool sspace(cchar *s)
{
    if (!s) {
        return 1;
    }
    while (isspace((uchar) * s)) {
        s++;
    }
    if (*s == '\0') {
        return 1;
    }
    return 0;
}

/*
    Hex
 */
PUBLIC bool shnumber(cchar *s)
{
    if (!s) {
        return 0;
    }
    return *s && strspn(s, "1234567890abcdefABCDEFxX") == strlen(s);
}

/*
    Floating point
    Float:      [+|-][DIGITS].[DIGITS][(e|E)[+|-]DIGITS]
 */
PUBLIC bool sfnumber(cchar *s)
{
    cchar *cp;
    int   dots, valid;

    if (!s || !*s) {
        return 0;
    }
    valid = s && *s && strspn(s, "1234567890.+-eE") == strlen(s) && strspn(s, "+-1234567890") > 0;
    if (valid) {
        /*
            Some extra checks
         */
        for (cp = s, dots = 0; *cp; cp++) {
            if (*cp == '.') {
                if (dots++ > 0) {
                    valid = 0;
                    break;
                }
            }
        }
    }
    return valid;
}

PUBLIC char *stitle(cchar *str)
{
    char   *ptr;
    size_t size, len;

    if (str == 0) {
        str = "";
    }
    len = slen(str);
    size = len + 1;
    if ((ptr = rAlloc(size)) != 0) {
        memcpy(ptr, str, len);
        ptr[len] = '\0';
        ptr[0] = (char) toupper((uchar) ptr[0]);
    }
    return ptr;
}

PUBLIC char *spbrk(cchar *str, cchar *set)
{
    cchar *sp;

    if (str == 0 || set == 0) {
        return 0;
    }
    for (; *str; str++) {
        for (sp = set; *sp; sp++) {
            if (*str == *sp) {
                return (char*) str;
            }
        }
    }
    return 0;
}

PUBLIC char *srchr(cchar *s, int c)
{
    if (s == 0) {
        return 0;
    }
    return strrchr(s, c);
}

/*
    Supports: "NN Suffix" or NNSuffix. eg. 64k or "64 k"
 */
PUBLIC int64 svalue(cchar *str)
{
    char  value[80];
    char  *tok;
    int64 factor, number;

    if (slen(str) >= sizeof(value)) {
        return 0;
    }
    scopy(value, sizeof(value), str);
    tok = strim(slower(value), " \t", R_TRIM_BOTH);
    if (sstarts(tok, "unlimited") || sstarts(tok, "infinite")) {
        number = MAXINT64;
    } else if (sstarts(tok, "never") || sstarts(tok, "forever")) {
        //  Year 2200
        number = 7260757200000L;
    } else {
        number = (int64) stoi(tok);
        if (sends(tok, "min") || sends(tok, "mins") || sends(tok, "minute") || sends(tok, "minutes")) {
            factor = 60;
        } else if (sends(tok, "hr") || sends(tok, "hrs") || sends(tok, "hour") || sends(tok, "hours")) {
            factor = 60 * 60;
        } else if (sends(tok, "day") || sends(tok, "days")) {
            factor = 60 * 60 * 24;
        } else if (sends(tok, "week") || sends(tok, "weeks")) {
            factor = 60 * 60 * 24 * 7;
        } else if (sends(tok, "month") || sends(tok, "months")) {
            factor = 60 * 60 * 24 * 30;
        } else if (sends(tok, "year") || sends(tok, "years")) {
            factor = 60 * 60 * 24 * 365;
        } else if (sends(tok, "kb") || sends(tok, "k")) {
            factor = 1024;
        } else if (sends(tok, "mb") || sends(tok, "m")) {
            factor = 1024 * 1024;
        } else if (sends(tok, "gb") || sends(tok, "g")) {
            factor = (uint64) 1024 * 1024 * 1024;
        } else {
            // bytes, bytes, sec, secs, second, seconds
            factor = 1;
        }
        number = ((int64) number > INT64_MAX / factor) ? INT64_MAX : number * factor;
    }
    return number;
}

PUBLIC int svaluei(cchar *str)
{
    int64 number;

    number = svalue(str);
    if (number > INT_MAX) {
        return INT_MAX;
    }
    if (number < INT_MIN) {
        return INT_MIN;
    }
    return (int) number;
}

PUBLIC char *srejoin(char *buf, ...)
{
    va_list args;
    char    *result;

    va_start(args, buf);
    result = srejoinv(buf, args);
    va_end(args);
    return result;
}

PUBLIC char *srejoinv(char *buf, va_list args)
{
    va_list ap;
    char    *dest, *str, *dp;
    size_t  bytes, len, required;

    va_copy(ap, args);
    len = slen(buf);
    required = len + 1;
    str = va_arg(ap, char*);

    while (str) {
        bytes = slen(str);
        if (required > MAXINT - bytes) {
            rError("runtime", "srejoinv integer overflow");
            va_end(ap);
            rFree(buf);
            return 0;
        }
        required += bytes;
        str = va_arg(ap, char*);
    }
    if ((dest = rAlloc(required)) == 0) {
        va_end(ap);
        rFree(buf);
        return 0;
    }
    memcpy(dest, buf, len);
    dp = &dest[len];
    va_copy(ap, args);
    str = va_arg(ap, char*);
    while (str) {
        bytes = slen(str);
        memcpy(dp, str, bytes);
        dp += bytes;
        str = va_arg(ap, char*);
    }
    *dp = '\0';
    va_end(ap);
    rFree(buf);
    return dest;
}

PUBLIC char *sreplace(cchar *str, cchar *pattern, cchar *replacement)
{
    RBuf   *buf;
    cchar  *s;
    size_t plen;

    if (!pattern || pattern[0] == '\0' || !str || str[0] == '\0') {
        return sclone(str);
    }
    buf = rAllocBuf(R_STRING_ALLOC_SIZE);
    plen = slen(pattern);
    for (s = str; *s; s++) {
        if (sncmp(s, pattern, plen) == 0) {
            if (replacement) {
                rPutStringToBuf(buf, replacement);
            }
            s += plen - 1;
        } else {
            rPutCharToBuf(buf, *s);
        }
    }
    return rBufToStringAndFree(buf);
}

/*
    Split a string at a substring and return the parts.
    This differs from stok in that it never returns null. Also, stok eats leading delimiters, whereas
    ssplit will return an empty string if there are leading delimiters.
    Note: Modifies the original string and returns the string for chaining.
 */
PUBLIC char *ssplit(char *str, cchar *delim, char **last)
{
    char *end;

    if (last) {
        *last = "";
    }
    if (str == 0) {
        return "";
    }
    if (delim == 0 || *delim == '\0') {
        return str;
    }
    if ((end = strpbrk(str, delim)) != 0) {
        *end++ = '\0';
        end += strspn(end, delim);
    } else {
        end = "";
    }
    if (last) {
        *last = end;
    }
    return str;
}

PUBLIC size_t sspn(cchar *str, cchar *set)
{
    if (str == 0 || set == 0 || *str == 0 || *set == 0) {
        return 0;
    }
    return strspn(str, set);
}

PUBLIC bool sstarts(cchar *str, cchar *prefix)
{
    if (str == 0 || prefix == 0) {
        return 0;
    }
    if (strncmp(str, prefix, slen(prefix)) == 0) {
        return 1;
    }
    return 0;
}

PUBLIC double stod(cchar *str)
{
    if (str) {
        return strtod(str, NULL);
    }
    return NAN;
}

PUBLIC int64 stoi(cchar *str)
{
    return stoix(str, NULL, 10);
}

PUBLIC int64 stoix(cchar *str, char **end, int radix)
{
    int64 result;

    if (!str || !*str) {
        return 0;
    }
    result = strtoll(str, end, radix);
    return result;
}

PUBLIC double stof(cchar *str)
{
    if (str == 0 || *str == 0) {
        return 0.0;
    }
    return atof(str);
}

/*
    Note "str" is modifed as per strok()
    WARNING: this does not allocate
 */
PUBLIC char *stok(char *str, cchar *delim, char **last)
{
    char   *start, *end;
    size_t i;

    assert(delim);

    if (!delim) {
        return 0;
    }
    if (str) {
        start = str;
    } else if (last) {
        start = *last;
    } else {
        return 0;
    }
    if (start == 0) {
        if (last) {
            *last = 0;
        }
        return 0;
    }
    i = strspn(start, delim);
    start += i;
    if (*start == '\0') {
        if (last) {
            *last = 0;
        }
        return 0;
    }
    end = strpbrk(start, delim);
    if (end) {
        *end++ = '\0';
        i = strspn(end, delim);
        end += i;
    }
    if (last) {
        *last = end;
    }
    return start;
}

/*
    Tokenize a string at a pattern and return the token. The next token is returned in *nextp.
    The delimiter is a string not a set of characters.
    If the pattern is not found, last is set to null.
    Note: Modifies the original string and returns the string for chaining.
 */
PUBLIC char *sptok(char *str, cchar *pattern, char **nextp)
{
    char *cp, *end;

    if (nextp) {
        *nextp = "";
    }
    if (str == 0) {
        return 0;
    }
    if (pattern == 0 || *pattern == '\0') {
        return str;
    }
    if ((cp = strstr(str, pattern)) != 0) {
        *cp = '\0';
        end = &cp[slen(pattern)];
    } else {
        end = 0;
    }
    if (nextp) {
        *nextp = end;
    }
    return str;
}

PUBLIC char *ssub(cchar *str, size_t offset, size_t len)
{
    char   *result;
    size_t size;

    assert(str);
    assert(offset >= 0);
    assert(0 <= len && len < MAXINT);

    if (str == 0 || offset > MAXINT || len > MAXINT) {
        return 0;
    }
    size = len + 1;
    if ((result = rAlloc(size)) == 0) {
        return 0;
    }
    sncopy(result, size, &str[offset], len);
    return result;
}

PUBLIC char *strim(char *str, cchar *set, int where)
{
    char   *s;
    size_t len, i;

    if (str == 0 || set == 0) {
        return 0;
    }
    if (where == 0) {
        where = R_TRIM_START | R_TRIM_END;
    }
    if (where & R_TRIM_START) {
        i = strspn(str, set);
    } else {
        i = 0;
    }
    s = &str[i];
    if (where & R_TRIM_END) {
        len = slen(s);
        while (len > 0 && strspn(&s[len - 1], set) > 0) {
            s[len - 1] = '\0';
            len--;
        }
    }
    return s;
}

PUBLIC char *supper(char *str)
{
    char *cp;

    if (str) {
        for (cp = str; *cp; cp++) {
            if (islower((uchar) * cp)) {
                *cp = (char) toupper((uchar) * cp);
            }
        }
    }
    return str;
}

/*
    Expand ${token} references in a string.
 */
PUBLIC char *stemplate(cchar *str, void *keys)
{
    RBuf   *buf;
    cchar  *value;
    char   *src, *result, *cp, *tok, *start;
    size_t bytes;

    if (str) {
        if (schr(str, '$') == 0) {
            return sclone(str);
        }
        buf = rAllocBuf(R_STRING_ALLOC_SIZE);
        for (src = (char*) str; *src; ) {
            if (*src == '$') {
                start = src;
                if (*++src == '{') {
                    for (cp = ++src; *cp && *cp != '}'; cp++);
                    tok = snclone(src, (size_t) (cp - src));
                } else {
                    for (cp = src; *cp && (isalnum((uchar) * cp) || *cp == '_'); cp++);
                    tok = snclone(src, (size_t) (cp - src));
                }
                value = rLookupName(keys, tok);
                if (value != 0) {
                    rPutStringToBuf(buf, value);
                    if (start[1] == '{') {
                        src = cp + 1;
                    } else {
                        src = cp;
                    }
                } else {
                    // Token not found, so copy original text
                    if (start[1] == '{') {
                        bytes = (size_t) ((cp + 1) - start);
                    } else {
                        bytes = (size_t) (cp - start);
                    }
                    rPutSubToBuf(buf, start, bytes);
                    src = (char*) start + bytes;
                }
                rFree(tok);
            } else {
                rPutCharToBuf(buf, *src++);
            }
        }
        result = rBufToStringAndFree(buf);
    } else {
        result = sclone("");
    }
    return result;
}

PUBLIC void szero(char *str)
{
    char *cp;

    for (cp = str; cp && *cp; ) {
        *cp++ = '\0';
    }
}

#if KEEP
/*
    String to list. This parses the string of space separated arguments. Single and double quotes are suppored.
    This returns a stable list.
 */
PUBLIC RList *stolist(cchar *src)
{
    RList *list;
    cchar *start;
    int   quote;

    list = rAllocList(0, 0);
    if (!list) {
        return NULL;
    }
    while (src && *src != '\0') {
        while (isspace((uchar) * src)) {
            src++;
        }
        if (*src == '\0') {
            break;
        }
        for (quote = 0, start = src; *src; src++) {
            if (*src == '\\') {
                src++;
            } else if (*src == '"' || *src == '\'') {
                if (*src == quote) {
                    quote = 0;
                    src++;
                    break;
                } else if (quote == 0) {
                    quote = *src;
                }
            } else if (isspace((uchar) * src) && !quote) {
                break;
            }
        }
        if (rAddItem(list, snclone(start, src - start)) < 0) {
            rFreeList(list);
            return NULL;
        }
    }
    return list;
}
#endif

#endif /* R_USE_STRING */
/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/thread.c ************/

/**
    thread.c - Cross platform thread and locking support.

    The portable runtime is not THREAD SAFE in general. Use only rStartEvent and rStartFiber
    in foreign threads.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes **********************************/



#if R_USE_THREAD
/******************************** Locals *************************************/

#ifndef ME_R_SPIN_COUNT
    #define ME_R_SPIN_COUNT 1500 /* Windows lock spin count */
#endif

typedef struct ThreadContext {
    RFiber *fiber;
    RThreadProc fn;
    void *arg;
} ThreadContext;

static RLock   globalLock;
static RThread mainThread;

/********************************** Forwards *********************************/

static void threadMain(ThreadContext *context);

/************************************ Code ***********************************/

PUBLIC int rInitThread(void)
{
    rInitLock(&globalLock);
    mainThread = rGetCurrentThread();
    return 0;
}

PUBLIC void rTermThread(void)
{
    rTermLock(&globalLock);
}

PUBLIC int rCreateThread(cchar *name, void *proc, void *data)
{
    size_t stackSize;

    stackSize = ME_STACK_SIZE;

    #if ME_WIN_LIKE
    {
        HANDLE h;
        uint   threadId;

        h = (HANDLE) _beginthreadex(NULL, 0, proc, (void*) data, 0, &threadId);
        if (h == NULL) {
            return R_ERR_CANT_INITIALIZE;
        }
    }
    #elif VXWORKS
    {
        int handle, pri;

        taskPriorityGet(taskIdSelf(), &pri);
        handle = taskSpawn(name, pri, VX_FP_TASK, stackSize, (FUNCPTR) proc, (int) data, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        if (handle < 0) {
            return R_ERR_CANT_INITIALIZE;
        }
    }
    #else /* UNIX | PTHREADS | ESP32 */
    {
        pthread_attr_t attr;
        pthread_t      h;

        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_attr_setstacksize(&attr, stackSize);

        if (pthread_create(&h, &attr, proc, (void*) data) != 0) {
            pthread_attr_destroy(&attr);
            return R_ERR_CANT_CREATE;
        }
        pthread_attr_destroy(&attr);
    }
    #endif /* UNIX */
    return 0;
}

PUBLIC RThread rGetMainThread(void)
{
    return mainThread;
}

PUBLIC RThread rGetCurrentThread(void)
{
#if ESP32
    return (RThread) xTaskGetCurrentTaskHandle();
#elif PTHREADS
    return (RThread) pthread_self();
#elif ME_WIN_LIKE
    return (RThread) GetCurrentThreadId();
#elif VXWORKS
    return (RThread) taskIdSelf();
#endif
}


/*
    Spawn a thread and yield to it and then return with the result of the called function.
    WARNING:
 */
PUBLIC void *rSpawnThread(RThreadProc fn, void *arg)
{
    ThreadContext *context;

    context = rAllocType(ThreadContext);
    context->fiber = rGetFiber();
    if (!context->fiber) {
        rFree(context);
        return 0;
    }
    context->fn = fn;
    context->arg = arg;

    if (rCreateThread("runtime", threadMain, context) < 0) {
        rFree(context);
        return 0;
    }
    return rYieldFiber(0);
}

static void threadMain(ThreadContext *context)
{
    void *result;

    //  Invoke the thread entry function
    result = context->fn(context->arg);
    //  Wakeup the original fiber. The yield will return this result.
    rAllocEvent(context->fiber, NULL, result, 0, 0);
    rFree(context);
}

PUBLIC RLock *rAllocLock(void)
{
    RLock *lock;

    if ((lock = rAllocType(RLock)) == 0) {
        return 0;
    }
    return rInitLock(lock);
}

PUBLIC void rTermLock(RLock *lock)
{
    assert(lock);
#if PTHREADS
    pthread_mutex_destroy(&lock->cs);
#elif ME_WIN_LIKE
    DeleteCriticalSection(&lock->cs);
#elif VXWORKS
    semDelete(lock->cs);
#endif
}

PUBLIC void rFreeLock(RLock *lock)
{
    rTermLock(lock);
    rFree(lock);
}

PUBLIC RLock *rInitLock(RLock *lock)
{
#if PTHREADS
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
#if ME_UNIX_LIKE
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
#else
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
#endif
    pthread_mutex_init(&lock->cs, &attr);
    pthread_mutexattr_destroy(&attr);

#elif ME_WIN_LIKE && !ME_DEBUG && CRITICAL_SECTION_NO_DEBUG_INFO && ME_64 && _WIN32_WINNT >= 0x0600
    InitializeCriticalSectionEx(&lock->cs, ME_R_SPIN_COUNT, CRITICAL_SECTION_NO_DEBUG_INFO);

#elif ME_WIN_LIKE
    InitializeCriticalSectionAndSpinCount(&lock->cs, ME_R_SPIN_COUNT);

#elif VXWORKS
    lock->cs = semMCreate(SEM_Q_PRIORITY | SEM_DELETE_SAFE);
#endif
    lock->initialized = 1;
    return lock;
}

/*
    Try to attain a lock. Do not block! Returns true if the lock was attained.
 */
PUBLIC bool rTryLock(RLock *lock)
{
    int rc;

    if (lock == 0) return 0;

#if PTHREADS
    rc = pthread_mutex_trylock(&lock->cs) != 0;
#elif ME_WIN_LIKE
    rc = TryEnterCriticalSection(&lock->cs) == 0;
#elif VXWORKS
    rc = semTake(lock->cs, NO_WAIT) != OK;
#endif
#if ME_DEBUG && KEEP
    if (!rc) {
        //  Only set owner if lock was successfully acquired
        lock->owner = rGetCurrentThread();
    }
#endif
    return (rc) ? 0 : 1;
}

/*
    Big global lock. Avoid using this.
 */
PUBLIC void rGlobalLock(void)
{
    rLock(&globalLock);
}

PUBLIC void rGlobalUnlock(void)
{
    rUnlock(&globalLock);
}

#if ME_USE_LOCK_MACROS
/*
    Still define these even if using macros to make linking with *.def export files easier
 */
#undef rLock
#undef rUnlock
#endif

/*
    Lock a mutex
 */
PUBLIC void rLock(RLock *lock)
{
    if (lock == 0) return;
#if PTHREADS
    pthread_mutex_lock(&lock->cs);
#elif ME_WIN_LIKE
    EnterCriticalSection(&lock->cs);
#elif VXWORKS
    semTake(lock->cs, WAIT_FOREVER);
#endif
#if ME_DEBUG && KEEP
    // Store last locker only
    lock->owner = rGetCurrentThread();
#endif
}

PUBLIC void rUnlock(RLock *lock)
{
    if (lock == 0) return;
#if PTHREADS
    pthread_mutex_unlock(&lock->cs);
#elif ME_WIN_LIKE
    LeaveCriticalSection(&lock->cs);
#elif VXWORKS
    semGive(lock->cs);
#endif
}

/*
    Use functions for debug mode. Production release uses macros.
 */

PUBLIC void rMemoryBarrier(void)
{
    #if defined(VX_MEM_BARRIER_RW)
    VX_MEM_BARRIER_RW();

    #elif ME_WIN_LIKE
    MemoryBarrier();

    #elif ME_COMPILER_HAS_ATOMIC
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    #elif ME_COMPILER_HAS_SYNC
    __sync_synchronize();

    #elif __GNUC__ && (ME_CPU_ARCH == ME_CPU_X86 || ME_CPU_ARCH == ME_CPU_X64) && !VXWORKS
    asm volatile ("mfence" : : : "memory");

    #elif __GNUC__ && (ME_CPU_ARCH == ME_CPU_PPC) && !VXWORKS
    asm volatile ("sync" : : : "memory");

    #elif __GNUC__ && (ME_CPU_ARCH == ME_CPU_ARM) && !VXWORKS
    asm volatile ("" ::: "memory");

    #elif XTENSA
    __asm__ __volatile__ ("memw" ::: "memory");

    #else
    rGlobalLock();
    rGlobalUnlock();
    #endif
}

#endif /* R_USE_THREAD */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/time.c ************/

/**
    time.c - Date and Time handling

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



#if R_USE_TIME
/********************************** Defines ***********************************/

#define ME_MAX_DATE      128
#ifndef R_HIGH_RES_TIMER
#define R_HIGH_RES_TIMER 1
#endif

/********************************** Forwards **********************************/

static int localTime(struct tm *timep, Time time);
struct tm *universalTime(struct tm *timep, Time time);

/************************************ Code ************************************/

PUBLIC char *rGetDate(cchar *format)
{
    struct tm   tm;
    static char buf[ME_MAX_DATE];

    localTime(&tm, rGetTime());
    if (format == 0 || *format == '\0') {
        format = R_DEFAULT_DATE;
    }
    strftime(buf, sizeof(buf), format, &tm);
    return sclone(buf);
}

PUBLIC char *rFormatLocalTime(cchar *format, Time time)
{
    struct tm   tm;
    static char buf[ME_MAX_DATE];

    if (format == 0) {
        format = R_DEFAULT_DATE;
    }
    localTime(&tm, time);
    strftime(buf, sizeof(buf), format, &tm);
    return sclone(buf);
}

PUBLIC char *rFormatUniversalTime(cchar *format, Time time)
{
    struct tm   tm;
    static char buf[ME_MAX_DATE];

    if (format == 0) {
        format = R_DEFAULT_DATE;
    }
    universalTime(&tm, time);
    strftime(buf, sizeof(buf), format, &tm);
    return sclone(buf);
}

PUBLIC char *rGetIsoDate(Time time)
{
    struct tm   tm;
    static char buf[ME_MAX_DATE];
    size_t      len;

    universalTime(&tm, time);
    strftime(buf, sizeof(buf), "%FT%T", &tm);
    len = slen(buf);
    if (len + 7 <= sizeof(buf)) {
        sfmtbuf(&buf[len], 7, ".%03dZ", (int) (time % 1000));
    }
    return sclone(buf);
}

PUBLIC char *rGetHttpDate(Time when)
{
    struct tm tm;
    time_t    time;
    char      buf[64];

    time = when / TPS;
    if (universalTime(&tm, time) == NULL) {
        return NULL;
    }
    //  Format as RFC 7231 IMF-fixdate: "Mon, 10 Nov 2025 21:28:28 GMT"
    if (strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm) == 0) {
        return NULL;
    }
    return sclone(buf);
}

/*
    Returns time in milliseconds since the epoch: 0:0:0 UTC Jan 1 1970.
 */
PUBLIC Time rGetTime(void)
{
#if VXWORKS
    struct timespec tv;
    clock_gettime(CLOCK_REALTIME, &tv);
    return (Time) (((Time) tv.tv_sec) * 1000) + (tv.tv_nsec / (1000 * 1000));
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (Time) (((Time) tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
#endif
}

#if ESP32
PUBLIC time_t timegm(struct tm *tm)
{
    //  Currently only GMT supported
    return mktime(tm) - 0;
}
#endif

/*
    Parse ISO-8601 timestamps like:
      "2023-12-25T10:30:45+00:00"
      "2023-12-25T10:30:45Z"
      "2022-01-01T00:00:00.000Z"

    Also accepts numeric offsets: +/-HH:MM or +/-HHMM

    Returns:
      - On success: ticks since Unix epoch in units of TPS.
      - On failure: -1.
 */
PUBLIC Time rParseIsoDate(cchar *str)
{
    struct tm tm;
    cchar     *p;
    int64     seconds, nsec;
    int       n, year, month, day, hour, min, sec;
    int       digits, frac, offsetSign, offsetHours, offsetMins;

    n = 0;
    nsec = 0;

    if (!str || !*str) {
        return -1;
    }
    // Parse "YYYY-MM-DDTHH:MM:SS"
    if (sscanf(str, "%4d-%2d-%2dT%2d:%2d:%2d%n", &year, &month, &day, &hour, &min, &sec, &n) != 6) {
        return -1;
    }
    p = str + n;

    // Validate date and time component ranges
    if (year < 1900 || year > 9999) {
        return -1;
    }
    if (month < 1 || month > 12) {
        return -1;
    }
    if (day < 1 || day > 31) {
        return -1;
    }
    if (hour < 0 || hour > 23) {
        return -1;
    }
    if (min < 0 || min > 59) {
        return -1;
    }
    if (sec < 0 || sec > 60) {  // 60 allows for leap seconds
        return -1;
    }

    // Optional fractional seconds: ".ssssss..."
    if (*p == '.') {
        p++;
        frac = 0;
        digits = 0;

        // Read up to 9 digits of fractional seconds
        while (isdigit((unsigned char) *p) && digits < 9) {
            frac = (frac * 10) + (*p - '0');
            digits++;
            p++;
        }
        // Scale to nanoseconds if fewer than 9 digits
        while (digits < 9) {
            frac *= 10;
            digits++;
        }
        // Ignore any extra digits beyond 9
        while (isdigit((unsigned char) *p)) {
            p++;
        }
        nsec = frac;    /* 0..999,999,999 */
    }

    // Timezone: 'Z' or [+/-]HH[:MM] or [+/-]HHMM
    offsetSign = 0;
    offsetHours = 0;
    offsetMins = 0;

    if (*p == 'Z') {
        // UTC offset 0
        offsetSign = 0;
        p++;
    } else if (*p == '+' || *p == '-') {
        offsetSign = (*p == '-') ? -1 : +1;
        p++;
        // Parse HH
        if (!isdigit((unsigned char) p[0]) || !isdigit((unsigned char) p[1])) {
            return -1;
        }
        offsetHours = (p[0] - '0') * 10 + (p[1] - '0');
        p += 2;
        // Optional ':'
        if (*p == ':') {
            p++;
        }
        // Optional MM
        if (isdigit((unsigned char) p[0]) && isdigit((unsigned char) p[1])) {
            offsetMins = (p[0] - '0') * 10 + (p[1] - '0');
            p += 2;
        }
        if (offsetHours > 23 || offsetMins > 59) {
            return -1;
        }
    } else {
        // No timezone → reject (we require Z or explicit offset)
        return -1;
    }
    // No trailing junk allowed
    if (*p != '\0') {
        return -1;
    }

    // Build broken-down time
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    tm.tm_isdst = -1;

    seconds = rMakeUniversalTime(&tm);
    if (seconds == (time_t) -1) {
        return -1;
    }
    /*
        Offset semantics:
        Input represents local time with offset relative to UTC:
          local_time = UTC + offset
        We parsed local_time into 'seconds'. To get UTC:
          UTC = local_time - offset
     */
    if (offsetSign != 0) {
        long offsetSecs = offsetHours * 3600L + offsetMins * 60L;
        if (offsetSign > 0) {
            // +HH:MM
            seconds -= offsetSecs;
        } else {
            // -HH:MM
            seconds += offsetSecs;
        }
    }
    // Convert to ticks: seconds * TPS + fractional part in ticks
    Time base = (Time) seconds * (Time) TPS;
    Time msec = (Time) nsec * (Time) TPS / 1000000000LL;
    return base + msec;
}

/*
    Parse an HTTP date string
 */
PUBLIC Time rParseHttpDate(cchar *value)
{
#if !defined(ESP32)
    struct tm tm;

    if (strptime(value, "%a, %d %b %Y %H:%M:%S", &tm) != NULL) {
#if ME_WIN_LIKE
        return _mkgmtime(&tm);
#else
        return timegm(&tm);
#endif
    }
#endif
    return 0;
}

/*
    High resolution timer
 */
#if R_HIGH_RES_TIMER
#if (LINUX || MACOSX) && ME_CPU_ARCH == ME_CPU_X86
PUBLIC uint64 rGetHiResTicks(void)
{
    uint64 now;

    __asm__ __volatile__ ("rdtsc" : "=A" (now));
    return now;
}
#elif (LINUX || MACOSX) && ME_CPU_ARCH == ME_CPU_X64
PUBLIC uint64 rGetHiResTicks(void)
{
    uint32 low, high;

    __asm__ __volatile__ ("rdtsc" : "=a" (low), "=d" (high));
    return ((uint64) high << 32) | low;
}
#elif MACOSX && (ME_CPU_ARCH == ME_CPU_ARM64)
PUBLIC uint64 rGetHiResTicks(void)
{
    return mach_absolute_time();
}
#elif WINDOWS
PUBLIC uint64 rGetHiResTicks(void)
{
    LARGE_INTEGER now;

    QueryPerformanceCounter(&now);
    return (((uint64) now.HighPart) << 32) + now.LowPart;
}
#else
PUBLIC uint64 rGetHiResTicks(void)
{
    return (uint64) rGetTicks();
}
#endif
#else
PUBLIC uint64 rGetHiResTicks(void)
{
    return (uint64) rGetTicks();
}
#endif

/*
    Ugh! Apparently monotonic clocks are broken on VxWorks prior to 6.7
 */
#ifdef CLOCK_MONOTONIC_RAW
    #if ME_UNIX_LIKE
        #define HAS_MONOTONIC_RAW     1
    #elif VXWORKS
        #if (_WRS_VXWORKS_MAJOR > 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR >= 7))
            #define HAS_MONOTONIC_RAW 1
        #endif
    #endif
#endif
#ifdef CLOCK_MONOTONIC
    #if ME_UNIX_LIKE || ESP32
        #define HAS_MONOTONIC     1
    #elif VXWORKS
        #if (_WRS_VXWORKS_MAJOR > 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR >= 7))
            #define HAS_MONOTONIC 1
        #endif
    #endif
#endif


/*
    Return time in milliseconds that never goes backwards. This is used for timers and not for time of day.
    The actual value returned is system dependant and does not represent time since Jan 1 1970.
    It may drift from real-time over time.
 */
PUBLIC Ticks rGetTicks(void)
{
#if ME_WIN_LIKE && ME_64 && _WIN32_WINNT >= 0x0600
    /* Windows Vista and later. Test for 64-bit so that building on deprecated Windows XP will work */
    return GetTickCount64();
#elif MACOSX
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    return mach_absolute_time() * info.numer / info.denom / (1000 * 1000);
#elif HAS_MONOTONIC_RAW
    /*
        Monotonic raw is the local oscillator. This may over time diverge from real time ticks.
        We prefer this to monotonic because the ticks will always increase by the same regular amount without
           adjustments.
     */
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC_RAW, &tv);
    return (Ticks) (((Ticks) tv.tv_sec) * 1000) + (tv.tv_nsec / (1000 * 1000));
#elif HAS_MONOTONIC
    /*
        Monotonic is the local oscillator with NTP gradual adjustments as NTP learns the differences between the local
        oscillator and NTP measured real clock time. i.e. it will adjust ticks over time to not lose ticks.
     */
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return (Ticks) (((Ticks) tv.tv_sec) * 1000) + (tv.tv_nsec / (1000 * 1000));
#else
    /*
        Last chance. Need to resort to rGetTime which is subject to user and seasonal adjustments.
        This code will prevent it going backwards, but may suffer large jumps forward.
     */
    static Time lastTicks = 0;
    static Time adjustTicks = 0;
    Time        result, diff;

    if (lastTicks == 0) {
        /* This will happen at init time when single threaded */
#if ME_WIN_LIKE
        lastTicks = GetTickCount();
#else
        lastTicks = rGetTime();
#endif
    }
#if ME_WIN_LIKE
    /*
        GetTickCount will wrap in 49.7 days
     */
    result = GetTickCount() + adjustTicks;
#else
    result = rGetTime() + adjustTicks;
#endif
    if ((diff = (result - lastTicks)) < 0) {
        /*
            Handle time reversals. Don't handle jumps forward. Sorry.
            Note: this is not time day, so it should not matter.
         */
        adjustTicks -= diff;
        result -= diff;
    }
    lastTicks = result;
    return result;
#endif
}

/*
    Return the number of milliseconds until the given timeout has expired.
 */
PUBLIC Ticks rGetRemainingTicks(Ticks mark, Ticks timeout)
{
    Ticks now, diff;

    now = rGetTicks();
    diff = (now - mark);
    if (diff < 0) {
        /*
            Detect time going backwards. Should never happen now.
         */
        assert(diff >= 0);
        diff = 0;
    }
    return timeout - diff;
}

PUBLIC Ticks rGetElapsedTicks(Ticks mark)
{
    return rGetTicks() - mark;
}

PUBLIC Time rGetElapsedTime(Time mark)
{
    return rGetTime() - mark;
}

PUBLIC Time rMakeTime(struct tm *tp)
{
    return mktime(tp);
}

PUBLIC Time rMakeUniversalTime(struct tm *tp)
{
#if ME_WIN_LIKE
    return _mkgmtime(tp);
#else
    return timegm(tp);
#endif
}

/*************************************** O/S Layer ***********************************/

static int localTime(struct tm *timep, Time time)
{
#if ME_UNIX_LIKE
    time_t when = (time_t) (time / TPS);
    if (localtime_r(&when, timep) == 0) {
        return R_ERR;
    }
#else
    struct tm *tp;
    time_t    when = (time_t) (time / TPS);
    if ((tp = localtime(&when)) == 0) {
        return R_ERR;
    }
    *timep = *tp;
#endif
    return 0;
}

struct tm *universalTime(struct tm *timep, Time time)
{
#if ME_UNIX_LIKE
    time_t when = (time_t) (time / TPS);
    return gmtime_r(&when, timep);
#else
    struct tm *tp;
    time_t    when;
    when = (time_t) (time / TPS);
    if ((tp = gmtime(&when)) == 0) {
        return 0;
    }
    *timep = *tp;
    return timep;
#endif
}

/*
    Compatibility for windows and VxWorks
 */
#if ME_WIN_LIKE || (VXWORKS && (_WRS_VXWORKS_MAJOR < 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR < 9)))

PUBLIC int gettimeofday(struct timeval *tv, struct timezone *tz)
{
#if ME_WIN_LIKE
    FILETIME   fileTime;
    Time       now;
    static int tzOnce;

    if (NULL != tv) {
        // Convert from 100-nanosec units to microseconds
        GetSystemTimeAsFileTime(&fileTime);
        now = ((((Time) fileTime.dwHighDateTime) << BITS(uint)) + ((Time) fileTime.dwLowDateTime));
        now /= 10;

        now -= TIME_GENESIS;
        tv->tv_sec = (long) (now / 1000000);
        tv->tv_usec = (long) (now % 1000000);
    }
    if (NULL != tz) {
        TIME_ZONE_INFORMATION zone;
        int                   rc, bias;
        rc = GetTimeZoneInformation(&zone);
        bias = (int) zone.Bias;
        if (rc == TIME_ZONE_ID_DAYLIGHT) {
            tz->tz_dsttime = 1;
        } else {
            tz->tz_dsttime = 0;
        }
        tz->tz_minuteswest = bias;
    }
    return 0;

#elif VXWORKS
    struct tm       tm;
    struct timespec now;
    time_t          t;
    char            *tze, *p;
    int             rc;

    if ((rc = clock_gettime(CLOCK_REALTIME, &now)) == 0) {
        tv->tv_sec = now.tv_sec;
        tv->tv_usec = (now.tv_nsec + 500) / TPS;
        if ((tze = getenv("TIMEZONE")) != 0) {
            if ((p = strchr(tze, ':')) != 0) {
                if ((p = strchr(p + 1, ':')) != 0) {
                    tz->tz_minuteswest = stoi(++p);
                }
            }
            t = tickGet();
            tz->tz_dsttime = (localtime_r(&t, &tm) == 0) ? tm.tm_isdst : 0;
        }
    }
    return rc;
#endif /* VXWORKS */
}
#endif /* ME_WIN_LIKE || VXWORKS */

#endif /* R_USE_TIME */
/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/unix.c ************/

/**
    unix.c - Posix specific adaptions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



#if ME_UNIX_LIKE
/*********************************** Code *************************************/
/*
    Signal handler for SIGUSR1 and SIGUSR2
 */
static void termHandler(int signo)
{
    /*
        This is safe to call from a signal handler.
        rSetState is async thread safe.
     */
    rSetState(signo == SIGUSR1 ? R_RESTART : R_STOPPED);
}

#if R_USE_EVENT
static void setLogFilter(void)
{
    rSetLogFilter("all", "all", 1);
}
#endif

static void logHandler(int signo)
{
#if R_USE_EVENT
    rStartEvent((RFiberProc) setLogFilter, 0, 0);
#endif
}

static void contHandler(int signo)
{
}

PUBLIC int rInitOs(void)
{
    struct sigaction sa;

    /*
        Cleanup the environment. IFS is often a security hole
     */
    setenv("IFS", "\t ", 1);

    // Deliberately restrictive umask. Mask out group and other permissions.
    umask(022);

    // Setup signal handlers using sigaction for portability
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);

    sa.sa_handler = contHandler;
    sigaction(SIGCONT, &sa, NULL);

    sa.sa_handler = termHandler;
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = logHandler;
    sigaction(SIGUSR2, &sa, NULL);

    // Initialize syslog
    openlog("r", LOG_PID | LOG_CONS, LOG_USER);

    return 0;
}

PUBLIC void rTermOs(void)
{
    closelog();
}

/*
    Write a message in the O/S native log (syslog in the case of linux)
 */
PUBLIC void rWriteToOsLog(cchar *message)
{
    syslog(LOG_INFO, "%s", message);
}

#if R_USE_RUN
PUBLIC int rRun(cchar *command, char **output)
{
    RBuf  *buf;
    pid_t pid;
    ssize nbytes;
    char  **argv;
    int   fds[2] = { -1, -1 };
    int   exitStatus, status;

    if (!command || *command == '\0') {
        return R_ERR_BAD_ARGS;
    }
    if (output) {
        *output = NULL;
    }
    if (rMakeArgs(command, &argv, 0) <= 0) {
        rError("run", "Failed to parse command: %s", command);
        return R_ERR_BAD_ARGS;
    }
    if (pipe(fds) < 0) {
        rError("run", "Failed to create pipe");
        rFree(argv);
        return R_ERR_CANT_OPEN;
    }
    if ((pid = fork()) < 0) {
        rError("run", "Failed to fork");
        close(fds[0]);
        close(fds[1]);
        rFree(argv);
        return R_ERR_CANT_CREATE;
    }
    if (pid == 0) {
        /* Child: redirect stdout & stderr to pipe */
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]);
        close(fds[1]);

        /* Use execvp so PATH is searched for the command */
        execvp(argv[0], argv);
        _exit(127);
    }
    /* Parent */
    close(fds[1]);

    buf = rAllocBuf(ME_BUFSIZE);
    while ((nbytes = read(fds[0], rGetBufEnd(buf), rGetBufSpace(buf))) > 0) {
        if (rGetBufLength(buf) + (size_t) nbytes > R_RUN_MAX_OUTPUT) {
            break;
        }
        if (output) {
            rAdjustBufEnd(buf, nbytes);
            if (rGetBufSpace(buf) < ME_BUFSIZE) {
                if (rGrowBuf(buf, ME_BUFSIZE) < 0) {
                    break;
                }
            }
        }
    }
    close(fds[0]);
    rAddNullToBuf(buf);

    //  Wait for child completion
    status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        rError("run", "Failed to wait for child");
        rFree(argv);
        rFreeBuf(buf);
        return R_ERR_CANT_COMPLETE;
    }
    rFree(argv);

    if (WIFEXITED(status)) {
        exitStatus = WEXITSTATUS(status);
        if (exitStatus != 0) {
            rError("run", "Command failed with status: %d", exitStatus);
            rFreeBuf(buf);
            return exitStatus;
        }
        //  continue

    } else if (WIFSIGNALED(status)) {
        rError("run", "Command terminated by signal: %d", WTERMSIG(status));
        rFreeBuf(buf);
        return R_ERR_BAD_STATE;

    } else {
        rError("run", "Command terminated abnormally, status: %d", status);
        rFreeBuf(buf);
        return R_ERR_BAD_STATE;
    }
    if (output) {
        *output = rBufToStringAndFree(buf);
    } else {
        rFreeBuf(buf);
    }
    return 0;
}
#endif /* R_USE_RUN */

#endif /* ME_UNIX_LIKE */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/vxworks.c ************/

/**
    vxworks.c - Vxworks specific adaptions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



#if VXWORKS
/*********************************** Code *************************************/

PUBLIC int rInitOs(void)
{
    return 0;
}

PUBLIC void rTermOs(void)
{
}

#if _WRS_VXWORKS_MAJOR < 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR < 9)
PUBLIC int access(const char *path, int mode)
{
    struct stat sbuf;

    return stat((char*) path, &sbuf);
}
#endif

PUBLIC int rUnloadNativeModule(RModule *mp)
{
    if (unldByModuleId((MODULE_ID) mp->handle, 0) != OK) {
        return R_ERR_CANT_COMPLETE;
    }
    return 0;
}

PUBLIC void rWriteToOsLog(cchar *message, int level)
{
    // VxWorks does not have a system log facility
}

PUBLIC pid_t rGetPid(void)
{
    return (pid_t) taskIdSelf();
}

#if _WRS_VXWORKS_MAJOR < 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR < 9)
PUBLIC int fsync(int fd)
{
    return 0;
}
#endif


PUBLIC int usleep(uint usec)
{
    struct timespec timeout;
    int             rc;

    if (usec > MAXINT) {
        usec = MAXINT;
    }
    timeout.tv_sec = usec / (1000 * 1000);
    timeout.tv_nsec = usec % (1000 * 1000) * 1000;
    do {
        rc = nanosleep(&timeout, &timeout);
    } while (rc < 0 && errno == EINTR);
    return 0;
}

#if R_USE_RUN
/*
    VxWorks rRun implementation
    NOTE: This is a simplified implementation that runs commands in the same task context.
    For full process isolation, this would require taskSpawn with named pipes (pipeDevCreate),
    which adds significant complexity. This implementation is suitable for simple command execution.
 */
PUBLIC int rRun(cchar *command, char **output)
{
    /*
        VxWorks doesn't have a simple fork/exec model like Unix or CreateProcess like Windows.
        A full implementation would require:
        1. Loading the command as a module or finding it in the symbol table
        2. Creating named pipes with pipeDevCreate()
        3. Spawning a task with taskSpawn()
        4. Redirecting I/O with ioTaskStdSet()
        5. Coordinating with semaphores

        For now, return an error indicating this platform is not yet fully supported.
     */
    rError("run", "rRun is not yet implemented for VxWorks");
    if (output) {
        *output = NULL;
    }
    return R_ERR_BAD_STATE;
}
#endif /* R_USE_RUN */

/*
    Create a routine to pull in the GCC support routines for double and int64 manipulations for some platforms. Do this
    incase modules reference these routines. Without this, the modules have to reference them. Which leads to multiple
    defines if two modules include them. (Code to pull in moddi3, udivdi3, umoddi3)
 */
double  __R_floating_point_resolution(double a, double b, int64 c, int64 d, uint64 e, uint64 f)
{
    a = a / b;
    a = a * b;
    c = c / d;
    c = c % d;
    e = e / f;
    e = e % f;
    c = (int64) a;
    d = (uint64) a;
    a = (double) c;
    a = (double) e;
    return (a == b) ? a : b;
}

#else
void vxworksDummy(void)
{
}
#endif /* VXWORKS */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/wait.c ************/

/*
    wait.c - Event wait layer

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



#if R_USE_WAIT
/*********************************** Locals ***********************************/
/**
    Maximum number of wait events
 */
#ifndef ME_MAX_EVENTS
    #define ME_MAX_EVENTS 128
#endif

//  Use a socket pair for wakeup on non-Unix platforms (Windows, VxWorks, ESP32, FreeRTOS)
#if !ME_UNIX_LIKE
    #define R_USE_WAKEUP_SOCKET 1
#else
    #define R_USE_WAKEUP_SOCKET 0
#endif

#if R_USE_WAKEUP_SOCKET
static Socket wakeupSock[2] = { INVALID_SOCKET, INVALID_SOCKET };
static int    createWakeupSocket(void);
#endif

#if ME_EVENT_NOTIFIER == R_EVENT_SELECT
static fd_set readMask, writeMask, readEvents, writeEvents;
static int    highestFd = -1;
#elif ME_EVENT_NOTIFIER == R_EVENT_WSAPOLL
static WSAPOLLFD *pollFds = 0;
static int       pollMax = 0;
static int       pollCount = 0;
#endif

static int   waitfd = -1;
static RHash *waitMap;
static Ticks nextDeadline;
static bool  waiting = 0;

/*********************************** Forwards *********************************/

static void invokeExpired(void);
static void invokeHandler(size_t fd, int event);
static Ticks getTimeout(Ticks deadline);

/************************************* Code ***********************************/

PUBLIC int rInitWait(void)
{
    waitMap = rAllocHash(0, R_DYNAMIC_NAME);
    if (!waitMap) {
        return R_ERR_MEMORY;
    }
#if ME_EVENT_NOTIFIER == R_EVENT_EPOLL
    if ((waitfd = epoll_create(ME_MAX_EVENTS)) < 0) {
        rError("runtime", "Call to epoll failed");
        return R_ERR_CANT_INITIALIZE;
    }
#elif ME_EVENT_NOTIFIER == R_EVENT_KQUEUE
    if ((waitfd = kqueue()) < 0) {
        rError("runtime", "Call to kqueue failed");
        return R_ERR_CANT_INITIALIZE;
    }
#elif ME_EVENT_NOTIFIER == R_EVENT_SELECT
    memset(&readMask, 0, sizeof(readMask));
    memset(&writeMask, 0, sizeof(writeMask));
    memset(&readEvents, 0, sizeof(readEvents));
    memset(&writeEvents, 0, sizeof(writeEvents));
    if (createWakeupSocket() < 0) {
        rError("runtime", "Cannot create wakeup socket");
        return R_ERR_CANT_INITIALIZE;
    }
    FD_SET(wakeupSock[0], &readMask);
    highestFd = (int) wakeupSock[0];
#elif ME_EVENT_NOTIFIER == R_EVENT_WSAPOLL
    pollMax = ME_MAX_EVENTS;
    if ((pollFds = rAlloc(sizeof(WSAPOLLFD) * pollMax)) == 0) {
        return R_ERR_MEMORY;
    }
    if (createWakeupSocket() < 0) {
        rError("runtime", "Cannot create wakeup socket");
        rFree(pollFds);
        return R_ERR_CANT_INITIALIZE;
    }
    //  Reserve pollFds[0] for the wakeup socket permanently
    pollFds[0].fd = wakeupSock[0];
    pollFds[0].events = POLLIN;
    pollFds[0].revents = 0;
    pollCount = 1;
#endif
    nextDeadline = MAXINT;
    return 0;
}

PUBLIC void rTermWait(void)
{
    rFreeHash(waitMap);

#if ME_EVENT_NOTIFIER == R_EVENT_EPOLL || ME_EVENT_NOTIFIER == R_EVENT_KQUEUE
    if (waitfd >= 0) {
        close(waitfd);
        waitfd = -1;
    }
#elif ME_EVENT_NOTIFIER == R_EVENT_WSAPOLL
    rFree(pollFds);
    pollFds = 0;
#endif

#if R_USE_WAKEUP_SOCKET
    if (wakeupSock[0] != INVALID_SOCKET) {
        closesocket(wakeupSock[0]);
        wakeupSock[0] = INVALID_SOCKET;
    }
    if (wakeupSock[1] != INVALID_SOCKET) {
        closesocket(wakeupSock[1]);
        wakeupSock[1] = INVALID_SOCKET;
    }
#endif
}

PUBLIC RWait *rAllocWait(int fd)
{
    RWait *wp;

    if ((wp = rAllocType(RWait)) == 0) {
        return 0;
    }
    wp->fd = fd;
    if (!rAddName(waitMap, sitos(fd), wp, 0)) {
        rFree(wp);
        return 0;
    }
    return wp;
}

/*
    Free a wait object. Assumed that the underlying socket is already closed.
 */
PUBLIC void rFreeWait(RWait *wp)
{
    char fdbuf[32];

    if (wp) {
        if (wp->fd != INVALID_SOCKET) {
#if ME_EVENT_NOTIFIER == R_EVENT_SELECT || ME_EVENT_NOTIFIER == R_EVENT_WSAPOLL
            //  Must clear masks and recalculate highestFd (SELECT) or remove from pollFds (WSAPOLL)
            rSetWaitMask(wp, 0, 0);
#endif
            rRemoveName(waitMap, sitosbuf(fdbuf, sizeof(fdbuf), (int64) wp->fd, 10));
        }
        rResumeWaitFiber(wp, R_READABLE | R_WRITABLE | R_MODIFIED | R_TIMEOUT);
        rFree(wp);
    }
}

/*
    Resume any waiting fiber when a wait is freed
 */
PUBLIC void rResumeWaitFiber(RWait *wp, int mask)
{
    if (wp->fiber) {
        rResumeFiber(wp->fiber, (void*) (ssize) mask);
    }
}

/*
    Set a wait handler. If flags includes R_WAIT_MAIN_FIBER, the handler runs on the main fiber.
    Otherwise, a new fiber coroutine is created for triggered events.
 */
PUBLIC void rSetWaitHandler(RWait *wp, RWaitProc handler, cvoid *arg, int64 mask, Ticks deadline, int flags)
{
    wp->deadline = deadline;
    wp->handler = handler;
    wp->arg = arg;
    wp->flags = flags;
    rSetWaitMask(wp, mask, 0);
}

PUBLIC void rSetWaitMask(RWait *wp, int64 mask, Ticks deadline)
{
    Socket fd;
    int    priorMask;

    if (wp == 0) {
        return;
    }
    wp->deadline = deadline;
    if (wp->mask == (int) mask) {
        return;
    }
    fd = wp->fd;
    priorMask = wp->mask;
    wp->mask = (int) mask;

#if ME_EVENT_NOTIFIER == R_EVENT_EPOLL
    struct epoll_event ev;

    if (fd < 0 || fd >= FD_SETSIZE) {
        rError("wait", "Cannot add fd %d to epoll, exceeds FD_SETSIZE %d", (int) fd, FD_SETSIZE);
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.data.fd = fd;
    if (mask & R_READABLE) {
        ev.events |= EPOLLIN | EPOLLHUP;
    }
    if (mask & R_WRITABLE) {
        ev.events |= EPOLLOUT | EPOLLHUP;
    }
    if (mask & R_MODIFIED) {
        ev.events |= EPOLLIN | EPOLLHUP;
    }
    if (ev.events) {
        //  Try MOD first, fall back to ADD if not yet registered
        if (epoll_ctl(waitfd, EPOLL_CTL_MOD, fd, &ev) < 0 && errno == ENOENT) {
            (void) epoll_ctl(waitfd, EPOLL_CTL_ADD, fd, &ev);
        }
    } else {
        (void) epoll_ctl(waitfd, EPOLL_CTL_DEL, fd, &ev);
    }

#elif ME_EVENT_NOTIFIER == R_EVENT_KQUEUE
    struct kevent ev[4], *kp;
    int           flags;

    if (fd < 0 || fd >= FD_SETSIZE) {
        rError("wait", "Cannot add fd %d to kqueue, exceeds FD_SETSIZE %d", (int) fd, FD_SETSIZE);
        return;
    }
    flags = mask >> 32;
    memset(&ev, 0, sizeof(ev));
    kp = &ev[0];

    //  Only delete filters that were previously set but are no longer needed
    if ((priorMask & R_READABLE) && !(mask & R_READABLE)) {
        EV_SET(kp++, fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
    }
    if ((priorMask & R_WRITABLE) && !(mask & R_WRITABLE)) {
        EV_SET(kp++, fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
    }
    // EV_CLEAR makes events edge-triggered
    if (mask & R_READABLE) {
        EV_SET(kp++, fd, EVFILT_READ, EV_ADD | EV_CLEAR, flags, 0, 0);
    }
    if (mask & R_WRITABLE) {
        EV_SET(kp++, fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, flags, 0, 0);
    }
    if (mask & R_MODIFIED) {
        EV_SET(kp++, fd, EVFILT_VNODE, EV_ADD | EV_CLEAR, flags, 0, 0);
    }
    if (kp > &ev[0]) {
        int rc = kevent(waitfd, &ev[0], (int) (kp - ev), NULL, 0, NULL);
        //  ENOENT is expected when deleting filters for already-closed sockets
        if (rc != 0 && errno != ENOENT) {
            rLog("error", "wait", "kevent: rc %d, errno %d\n", rc, errno);
        }
    }
#elif ME_EVENT_NOTIFIER == R_EVENT_SELECT
    int i;

    if (fd < 0 || fd >= FD_SETSIZE) {
        rError("wait", "Cannot add fd %d to select, exceeds FD_SETSIZE %d", (int) fd, FD_SETSIZE);
        return;
    }
    FD_CLR(fd, &readMask);
    FD_CLR(fd, &writeMask);

    if (mask & R_READABLE) {
        FD_SET(fd, &readMask);
    }
    if (mask & R_WRITABLE) {
        FD_SET(fd, &writeMask);
    }
    if (mask & R_MODIFIED) {
        FD_SET(fd, &readMask);
        mask |= R_READABLE;
    }
    if (mask == 0 && fd >= highestFd) {
        //  Search backward from old highestFd to find the new highest
        highestFd = (int) wakeupSock[0];
        for (i = fd; i > wakeupSock[0]; i--) {
            if (FD_ISSET(i, &readMask) || FD_ISSET(i, &writeMask)) {
                highestFd = i;
                break;
            }
        }
    } else {
        highestFd = max(fd, highestFd);
    }
#elif ME_EVENT_NOTIFIER == R_EVENT_WSAPOLL
    if (fd < 0) {
        return;
    }
    //  Remove existing entry for this fd
    for (int i = 0; i < pollCount; i++) {
        if (pollFds[i].fd == fd) {
            pollFds[i] = pollFds[--pollCount];
            break;
        }
    }
    //  Add new entry if mask is non-zero
    if (mask) {
        //  Grow the poll table if full
        if (pollCount >= pollMax) {
            int       newMax = pollMax * 2;
            WSAPOLLFD *newFds = rAlloc(sizeof(WSAPOLLFD) * newMax);
            if (newFds) {
                memcpy(newFds, pollFds, sizeof(WSAPOLLFD) * pollCount);
                rFree(pollFds);
                pollFds = newFds;
                pollMax = newMax;
            } else {
                rError("wait", "Cannot grow poll table for fd %d", fd);
                return;
            }
        }
        pollFds[pollCount].fd = fd;
        pollFds[pollCount].events = 0;
        if (mask & R_READABLE) {
            pollFds[pollCount].events |= POLLIN;
        }
        if (mask & R_WRITABLE) {
            pollFds[pollCount].events |= POLLOUT;
        }
        if (mask & R_MODIFIED) {
            pollFds[pollCount].events |= POLLIN;
        }
        pollFds[pollCount].revents = 0;
        pollCount++;
    }
#endif
    (void) priorMask;
}

/*
    Async thread safe. Can be called from foreign threads to wake the event loop.
 */
PUBLIC void rWakeup(void)
{
#if !R_USE_WAKEUP_SOCKET
    if (waiting) {
        kill(getpid(), SIGCONT);
    }
#else
    char byte = 'W';
    if (waiting && wakeupSock[1] != INVALID_SOCKET) {
        send(wakeupSock[1], &byte, 1, MSG_NOSIGNAL);
    }
#endif
}

PUBLIC int rWait(Ticks deadline)
{
    Ticks timeout;

    if (rState >= R_STOPPING) return 0;

    waiting = 1;
    rMemoryBarrier();

    timeout = getTimeout(deadline);

#if ME_EVENT_NOTIFIER == R_EVENT_EPOLL
    struct epoll_event events[ME_MAX_EVENTS];
    int                event, fd, i, numEvents;

    if ((numEvents = epoll_wait(waitfd, events, sizeof(events) / sizeof(struct epoll_event), timeout)) < 0) {
        if (errno != EINTR) {
            rTrace("event", "Epoll returned %d, errno %d", numEvents, errno);
        }
        invokeExpired();
        waiting = 0;
        return 0;
    }
    if (numEvents == 0) {
        invokeExpired();
    } else {
        for (i = 0; i < numEvents; i++) {
            event = 0;
            fd = events[i].data.fd;
            if (events[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP)) {
                event |= R_READABLE;
            }
            if (events[i].events & (EPOLLOUT | EPOLLHUP)) {
                event |= R_WRITABLE;
            }
            if (event) {
                invokeHandler((size_t) fd, event);
            }
        }
    }

#elif ME_EVENT_NOTIFIER == R_EVENT_KQUEUE
    struct timespec ts;
    struct kevent   events[ME_MAX_EVENTS], *kev;
    int             event, fd, i, numEvents;

    ts.tv_sec = ((int) (timeout / 1000));
    ts.tv_nsec = ((int) ((timeout % 1000) * 1000 * 1000));

    if ((numEvents = kevent(waitfd, NULL, 0, events, ME_MAX_EVENTS, &ts)) < 0) {
        if (errno != EINTR && errno != EAGAIN) {
            rDebug("event", "kevent returned %d, errno %d", numEvents, errno);
        }
        invokeExpired();
        waiting = 0;
        return 0;
    }
    if (numEvents == 0) {
        invokeExpired();
    } else {
        for (i = 0; i < numEvents; i++) {
            kev = &events[i];
            fd = (int) kev->ident;
            event = 0;
            if (kev->filter == EVFILT_READ || kev->filter == EVFILT_VNODE ||
                kev->flags & (EV_ERROR | EV_EOF)) {
                event |= R_READABLE;
            }
            if (kev->filter == EVFILT_WRITE || kev->flags & (EV_ERROR | EV_EOF)) {
                event |= R_WRITABLE;
            }
            if (event) {
                invokeHandler((size_t) fd, event);
            }
        }
    }

#elif ME_EVENT_NOTIFIER == R_EVENT_SELECT
    struct timeval tv;
    char           buf[64];
    int            event, fd, numEvents;

    tv.tv_sec = (long) (timeout / 1000);
    tv.tv_usec = (long) ((timeout % 1000) * 1000);

    readEvents = readMask;
    writeEvents = writeMask;
    if (highestFd < 0) {
        usleep((int) timeout * TPS);
        invokeExpired();
        waiting = 0;
        return 0;
    }
#if FREERTOS
    if (timeout > 0) {
        taskYIELD();
    }
#endif
    if (select(highestFd + 1, &readEvents, &writeEvents, NULL, &tv) < 0) {
        if (errno == EBADF) {
            int newHighest = -1;
            //  Find and remove bad file descriptors
            for (fd = 0; fd <= highestFd; fd++) {
                if (FD_ISSET(fd, &readMask) || FD_ISSET(fd, &writeMask)) {
                    if (fcntl(fd, F_GETFD) < 0 && errno == EBADF) {
                        FD_CLR(fd, &readMask);
                        FD_CLR(fd, &writeMask);
                        invokeHandler((size_t) fd, R_READABLE | R_WRITABLE);
                    } else {
                        newHighest = fd;
                    }
                }
            }
            highestFd = newHighest;
        } else if (errno != ETIMEDOUT && errno != EINTR) {
            rTrace("event", "Select error %d", errno);
        }
        invokeExpired();
        waiting = 0;
        return 0;
    }
    numEvents = 0;
    for (fd = 0; fd < highestFd + 1; fd++) {
        event = 0;
        if (FD_ISSET(fd, &readEvents)) {
            if (fd == wakeupSock[0]) {
                while (recv(wakeupSock[0], buf, sizeof(buf), 0) > 0) {
                }
                continue;
            }
            event |= R_READABLE;
        }
        if (FD_ISSET(fd, &writeEvents)) {
            event |= R_WRITABLE;
        }
        if (event) {
            invokeHandler((size_t) fd, event);
            numEvents++;
        }
    }
    if (numEvents == 0) {
        invokeExpired();
    }
#elif ME_EVENT_NOTIFIER == R_EVENT_WSAPOLL
    SOCKET fd;
    int    event, i, numEvents;
    char   buf[64];

    timeout = min(timeout, 1000);
    if ((numEvents = WSAPoll(pollFds, pollCount, (int) timeout)) < 0) {
        rTrace("event", "WSAPoll error %d", WSAGetLastError());
        invokeExpired();
        waiting = 0;
        return 0;
    }
    if (numEvents == 0) {
        invokeExpired();
    } else {
        for (i = 0; i < pollCount; i++) {
            if (pollFds[i].revents == 0) continue;

            fd = pollFds[i].fd;

            //  Check if this is the wakeup socket at slot 0
            if (i == 0 && fd == wakeupSock[0]) {
                //  Drain the wakeup socket
                while (recv(wakeupSock[0], buf, sizeof(buf), MSG_NOSIGNAL) > 0) {
                }
                pollFds[i].revents = 0;
                continue;
            }

            event = 0;
            if (pollFds[i].revents & (POLLIN | POLLERR | POLLHUP)) {
                event |= R_READABLE;
            }
            if (pollFds[i].revents & (POLLOUT | POLLHUP)) {
                event |= R_WRITABLE;
            }
            if (event) {
                invokeHandler((size_t) fd, event);
            }
            pollFds[i].revents = 0;
        }
    }
#endif
    waiting = 0;
    return numEvents;
}

/*
    Invoke events that have expired deadlines
 */
static void invokeExpired(void)
{
    RWait  *wp;
    RName  *np;
    Ticks  now;
    Socket expired[ME_MAX_EVENTS];
    int    count;

    now = rGetTicks();
    count = 0;

    /*
        First pass: collect expired fds without modifying the hash.
        If more than ME_MAX_EVENTS expire simultaneously, extras are processed on the next rWait() call.
     */
    for (ITERATE_NAMES(waitMap, np)) {
        wp = (RWait*) np->value;
        if (wp->deadline && wp->deadline <= now) {
            if (count < ME_MAX_EVENTS) {
                expired[count++] = wp->fd;
            }
        }
    }
    /*
        Second pass: invoke handlers after iteration completes
        This prevents hash modification during iteration which would corrupt the iterator
     */
    for (int i = 0; i < count; i++) {
        invokeHandler((size_t) expired[i], R_TIMEOUT);
    }
}

/*
    This will invoke the handler or resume a waiting fiber
 */
static void invokeHandler(size_t fd, int mask)
{
    RWait  *wp;
    RFiber *fiber;
    char   fdbuf[32];

    if ((wp = rLookupName(waitMap, sitosbuf(fdbuf, sizeof(fdbuf), (int64) fd, 10))) == 0) {
        return;
    }
    if ((wp->mask | R_TIMEOUT) & mask) {
        wp->eventMask = mask;
        if (!wp->fiber && !wp->handler) {
            //  No fiber and no handler - cleanup wait and return
            rFreeWait(wp);
            return;
        }
        if (wp->flags & R_WAIT_MAIN_FIBER) {
            //  Run handler directly on main fiber - fast path for accept
            ((RWaitProc) wp->handler)(wp->arg, mask & ~R_TIMEOUT);
        } else if (wp->fiber) {
            //  Existing path: resume waiting fiber
            rResumeFiber(wp->fiber, (void*) (ssize) (mask & ~R_TIMEOUT));
        } else {
            //  Existing path: allocate new fiber for handler
            if ((fiber = rAllocFiber("wait", (RFiberProc) wp->handler, wp->arg)) == 0) {
                //  Wait for a fiber to be available to run the handler
                rStartEvent((REventProc) wp->handler, (void*) wp->arg, 1);
                return;
            }
            rResumeFiber(fiber, (void*) (ssize) (mask & ~R_TIMEOUT));
        }
    }
}

/*
    Wait for I/O -- only called by fiber code
    This will block for the required I/O up to the given time deadline.
 */
PUBLIC int rWaitForIO(RWait *wp, int mask, Ticks deadline)
{
    Ticks priorDeadline;
    int   priorMask;
    void  *value;

    if (deadline && deadline < rGetTicks()) {
        return 0;
    }
    priorDeadline = wp->deadline;
    priorMask = wp->mask;
    wp->fiber = rGetFiber();
    rSetWaitMask(wp, mask, deadline);

    value = rYieldFiber(0);

    wp->fiber = 0;
    rSetWaitMask(wp, priorMask, priorDeadline);
    return (int) (ssize) value;
}

PUBLIC int rGetWaitFd(void)
{
    return waitfd;
}

/*
    Get the timeout for waiting based on the earliest deadline
 */
static Ticks getTimeout(Ticks deadline)
{
    Ticks nextEvent, now, timeout;
    RWait *wp;
    RName *np;

    now = rGetTicks();

    for (ITERATE_NAMES(waitMap, np)) {
        wp = (RWait*) np->value;
        if (wp->deadline) {
            deadline = min(deadline, wp->deadline);
        }
    }
    if (nextDeadline < now) {
        nextDeadline = now;
    }
    if (deadline) {
        if (nextDeadline > now) {
            nextDeadline = min(nextDeadline, deadline);
        } else {
            nextDeadline = deadline;
        }
    }
    timeout = nextDeadline - now;
    if (timeout < 0) {
        timeout = 0;
    } else if (timeout > MAXINT) {
        //  Reduce to MAXINT to permit callers to be able to do ticks arithmetic
        timeout = MAXINT;
    }
    nextEvent = rGetNextDueEvent();
    timeout = min(timeout, nextEvent - now);
    timeout = max(timeout, 0);
    return timeout;
}

#if R_USE_WAKEUP_SOCKET
/*
    Create a TCP loopback socket pair for wakeup notifications.
    This allows rWakeup() to interrupt the event wait by writing to wakeupSock[1].
    Works on Windows, VxWorks, ESP32, FreeRTOS - any platform with BSD sockets.
 */
static int createWakeupSocket(void)
{
    Socket             listener, client, server;
    struct sockaddr_in addr;
    socklen_t          addrLen;

    if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        return R_ERR_CANT_INITIALIZE;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(listener, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        closesocket(listener);
        return R_ERR_CANT_INITIALIZE;
    }
    if (listen(listener, 1) < 0) {
        closesocket(listener);
        return R_ERR_CANT_INITIALIZE;
    }
    addrLen = sizeof(addr);
    if (getsockname(listener, (struct sockaddr*) &addr, &addrLen) < 0) {
        closesocket(listener);
        return R_ERR_CANT_INITIALIZE;
    }
    if ((client = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        closesocket(listener);
        return R_ERR_CANT_INITIALIZE;
    }
    if (connect(client, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        closesocket(listener);
        closesocket(client);
        return R_ERR_CANT_INITIALIZE;
    }
    addrLen = sizeof(addr);
    if ((server = accept(listener, (struct sockaddr*) &addr, &addrLen)) == INVALID_SOCKET) {
        closesocket(listener);
        closesocket(client);
        return R_ERR_CANT_INITIALIZE;
    }
    closesocket(listener);

    //  Set both sockets to non-blocking
#if ME_WIN_LIKE
    {
        ulong mode = 1;
        ioctlsocket(server, FIONBIO, &mode);
        ioctlsocket(client, FIONBIO, &mode);
    }
#elif VXWORKS
    {
        int flag = 1;
        ioctl(server, FIONBIO, (int) &flag);
        ioctl(client, FIONBIO, (int) &flag);
    }
#else
    fcntl(server, F_SETFL, fcntl(server, F_GETFL) | O_NONBLOCK);
    fcntl(client, F_SETFL, fcntl(client, F_GETFL) | O_NONBLOCK);
#endif

    wakeupSock[0] = server;
    wakeupSock[1] = client;
    return 0;
}
#endif

#endif /* R_USE_WAIT */
/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/win.c ************/

/**
    win.c - Windows specific adaptions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



#if CYGWIN
 #include "w32api/windows.h"
#endif

/********************************** Locals ************************************/

#if ME_WIN_LIKE
static HINSTANCE appInstance;
static HWND      appWindow;
static int       socketMessage;

/********************************* Forwards ***********************************/

static LRESULT CALLBACK winProc(HWND hwnd, UINT msg, UINT wp, LPARAM lp);

/*********************************** Code *************************************/
/*
    Initialize the O/S platform layer
 */

PUBLIC int rInitOs(void)
{
    WSADATA  wsaData;
    WNDCLASS wc;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return -1;
    }
    _fmode = _O_BINARY;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = appInstance;
    wc.hIcon = NULL;
    wc.lpfnWndProc = (WNDPROC) winProc;
    wc.lpszMenuName = wc.lpszClassName = ME_NAME;
    if (!RegisterClass(&wc)) {
        return -1;
    }
    if ((appWindow = CreateWindow(ME_NAME, ME_TITLE, WS_MINIMIZE | WS_POPUPWINDOW, CW_USEDEFAULT,
                                  0, 0, 0, NULL, NULL, appInstance, NULL)) == NULL) {
        return -1;
    }
    ShowWindow(appWindow, SW_SHOWNORMAL);
    UpdateWindow(appWindow);
    return 0;
}

PUBLIC void rTermOs(void)
{
    UnregisterClass(ME_NAME, appInstance);
    appInstance = 0;
    WSACleanup();
}

PUBLIC HINSTANCE rGetInst(void)
{
    return appInstance;
}

PUBLIC HWND rGetHwnd(void)
{
    return appWindow;
}

PUBLIC void rSetInst(HINSTANCE inst)
{
    appInstance = inst;
}

PUBLIC void rSetHwnd(HWND h)
{
    appWindow = h;
}

PUBLIC void rSetSocketMessage(int msg)
{
    socketMessage = msg;
}

PUBLIC void rWriteToOsLog(cchar *message, int level)
{
    HKEY       hkey;
    void       *event;
    long       errorType;
    ulong      exists;
    char       buf[ME_BUFSIZE], logName[ME_BUFSIZE], *cp, *value;
    wchar      *lines[9];
    int        type;
    static int once = 0;

    scopy(buf, sizeof(buf), message);
    cp = &buf[slen(buf) - 1];
    while (*cp == '\n' && cp > buf) {
        *cp-- = '\0';
    }
    type = EVENTLOG_ERROR_TYPE;
    lines[0] = buf;
    lines[1] = 0;
    lines[2] = lines[3] = lines[4] = lines[5] = 0;
    lines[6] = lines[7] = lines[8] = 0;

    if (once == 0) {
        // Initialize the registry
        once = 1;
        sfmtbuf(logName, sizeof(logName), "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s",
                rGetAppName());
        hkey = 0;

        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, logName, 0, NULL, 0, KEY_ALL_ACCESS, NULL,
                           &hkey, &exists) == ERROR_SUCCESS) {
            value = "%SystemRoot%\\System32\\netmsg.dll";
            if (RegSetValueEx(hkey, UT("EventMessageFile"), 0, REG_EXPAND_SZ, (uchar*) value,
                              (int) slen(value) + 1) != ERROR_SUCCESS) {
                RegCloseKey(hkey);
                return;
            }
            errorType = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
            if (RegSetValueEx(hkey, UT("TypesSupported"), 0, REG_DWORD, (uchar*) &errorType,
                              sizeof(DWORD)) != ERROR_SUCCESS) {
                RegCloseKey(hkey);
                return;
            }
            RegCloseKey(hkey);
        }
    }

    event = RegisterEventSource(0, rGetAppName());
    if (event) {
        /*
            3299 is the event number for the generic message in netmsg.dll.
            "%1 %2 %3 %4 %5 %6 %7 %8 %9" -- thanks Apache for the tip
         */
        ReportEvent(event, EVENTLOG_ERROR_TYPE, 0, 3299, NULL, sizeof(lines) / sizeof(wchar*), 0, lines, 0);
        DeregisterEventSource(event);
    }
}

/*
    Determine the registry hive by the first portion of the path. Return
    a pointer to the rest of key path after the hive portion.
 */
static cchar *getHive(cchar *keyPath, HKEY *hive)
{
    char  key[ME_MAX_PATH], *cp;
    ssize len;

    assert(keyPath && *keyPath);

    *hive = 0;

    scopy(key, sizeof(key), keyPath);
    key[sizeof(key) - 1] = '\0';

    if ((cp = schr(key, '\\')) != 0) {
        *cp++ = '\0';
    }
    if (cp == 0 || *cp == '\0') {
        return 0;
    }
    if (!scaselesscmp(key, "HKEY_LOCAL_MACHINE") || !scaselesscmp(key, "HKLM")) {
        *hive = HKEY_LOCAL_MACHINE;
    } else if (!scaselesscmp(key, "HKEY_CURRENT_USER") || !scaselesscmp(key, "HKCU")) {
        *hive = HKEY_CURRENT_USER;
    } else if (!scaselesscmp(key, "HKEY_USERS")) {
        *hive = HKEY_USERS;
    } else if (!scaselesscmp(key, "HKEY_CLASSES_ROOT")) {
        *hive = HKEY_CLASSES_ROOT;
    } else {
        return 0;
    }
    if (*hive == 0) {
        return 0;
    }
    len = slen(key) + 1;
    return keyPath + len;
}

PUBLIC RList *rListRegistry(cchar *key)
{
    HKEY  top, h;
    wchar name[ME_MAX_PATH];
    RList *list;
    int   index, size;

    assert(key && *key);

    /*
        Get the registry hive
     */
    if ((key = getHive(key, &top)) == 0) {
        return 0;
    }
    if (RegOpenKeyEx(top, key, 0, KEY_READ, &h) != ERROR_SUCCESS) {
        return 0;
    }
    list = rAllocList(0, 0);
    if (!list) {
        RegCloseKey(h);
        return 0;
    }
    index = 0;
    while (1) {
        size = sizeof(name) / sizeof(wchar);
        if (RegEnumValue(h, index, name, &size, 0, NULL, NULL, NULL) != ERROR_SUCCESS) {
            break;
        }
        if (rAddItem(list, sclone(name)) < 0) {
            rFreeList(list);
            RegCloseKey(h);
            return 0;
        }
        index++;
    }
    RegCloseKey(h);
    return list;
}

PUBLIC char *rReadRegistry(cchar *key, cchar *name)
{
    HKEY  top, h;
    char  *value;
    ulong type, size;

    assert(key && *key);

    /*
        Get the registry hive
     */
    if ((key = getHive(key, &top)) == 0) {
        return 0;
    }
    if (RegOpenKeyEx(top, key, 0, KEY_READ, &h) != ERROR_SUCCESS) {
        return 0;
    }

    /*
        Get the type
     */
    if (RegQueryValueEx(h, name, 0, &type, 0, &size) != ERROR_SUCCESS) {
        RegCloseKey(h);
        return 0;
    }
    if (type != REG_SZ && type != REG_EXPAND_SZ) {
        RegCloseKey(h);
        return 0;
    }
    if ((value = rAlloc(size + 1)) == 0) {
        RegCloseKey(h);
        return 0;
    }
    if (RegQueryValueEx(h, name, 0, &type, (uchar*) value, &size) != ERROR_SUCCESS) {
        RegCloseKey(h);
        return 0;
    }
    RegCloseKey(h);
    value[size] = '\0';
    return value;
}

PUBLIC int rWriteRegistry(cchar *key, cchar *name, cchar *value)
{
    HKEY  top, h, subHandle;
    ulong disposition;

    assert(key && *key);
    assert(value && *value);

    /*
        Get the registry hive
     */
    if ((key = getHive(key, &top)) == 0) {
        return R_ERR_CANT_ACCESS;
    }
    if (name && *name) {
        /*
            Write a registry string value
         */
        if (RegOpenKeyEx(top, key, 0, KEY_ALL_ACCESS, &h) != ERROR_SUCCESS) {
            return R_ERR_CANT_ACCESS;
        }
        if (RegSetValueEx(h, name, 0, REG_SZ, (uchar*) value, (int) slen(value) + 1) != ERROR_SUCCESS) {
            RegCloseKey(h);
            return R_ERR_CANT_READ;
        }

    } else {
        /*
            Create a new sub key
         */
        if (RegOpenKeyEx(top, key, 0, KEY_CREATE_SUB_KEY, &h) != ERROR_SUCCESS) {
            return R_ERR_CANT_ACCESS;
        }
        if (RegCreateKeyEx(h, value, 0, NULL, REG_OPTION_NON_VOLATILE,
                           KEY_ALL_ACCESS, NULL, &subHandle, &disposition) != ERROR_SUCCESS) {
            return R_ERR_CANT_ACCESS;
        }
        RegCloseKey(subHandle);
    }
    RegCloseKey(h);
    return 0;
}

/*
    Parse the command line args and return the count of args.
    If argv is NULL, the args are parsed read-only. If argv is set, then the args will be extracted,
    back-quotes removed and argv will be set to point to all the args.
    NOTE: this routine does not allocate.
 */
PUBLIC int rParseArgs(char *args, char **argv, int maxArgc)
{
    char *dest, *src, *star;
    int  quote, argc;

    /*
        Example     "showColors" red 'light blue' "yellow white" 'Cannot \"render\"'
        Becomes:    ["showColors", "red", "light blue", "yellow white", "Cannot \"render\""]
     */
    for (argc = 0, src = args; src && *src != '\0' && argc < maxArgc; argc++) {
        while (isspace((uchar) * src)) {
            src++;
        }
        if (*src == '\0') {
            break;
        }
        star = dest = src;
        if (*src == '"' || *src == '\'') {
            quote = *src;
            src++;
            dest++;
        } else {
            quote = 0;
        }
        if (argv) {
            argv[argc] = src;
        }
        while (*src) {
            if (*src == '\\' && src[1] && (src[1] == '\\' || src[1] == '"' || src[1] == '\'')) {
                src++;
            } else {
                if (quote) {
                    if (*src == quote && !(src > star && src[-1] == '\\')) {
                        break;
                    }
                } else if (*src == ' ') {
                    break;
                }
            }
            if (argv) {
                *dest++ = *src;
            }
            src++;
        }
        if (*src != '\0') {
            src++;
        }
        if (argv) {
            *dest++ = '\0';
        }
    }
    return argc;
}

/*
    Main menu window message handler.
 */
static LRESULT CALLBACK winProc(HWND hwnd, UINT msg, UINT wp, LPARAM lp)
{
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        rStop();
        return 0;

    case WM_SYSCOMMAND:
        break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/*
    Check for Windows Messages
 */
WPARAM checkWindowsMsgLoop(void)
{
    MSG msg;

    if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
        if (!GetMessage(&msg, NULL, 0, 0) || msg.message == WM_QUIT) {
            return msg.wParam;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

/*
    Windows message handler
 */
static LRESULT CALLBACK websAboutProc(HWND hwndDlg, uint msg, uint wp, long lp)
{
    LRESULT lResult;

    lResult = DefWindowProc(hwndDlg, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:
        break;
    case WM_DESTROY:
        break;
    case WM_COMMAND:
        break;
    }
    return lResult;
}

PUBLIC int usleep(uint usec)
{
    Sleep(usec / 1000);
    return 0;
}


/*
    Custom strptime to parse a HTTP if-modified string
 */
static const char *weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

PUBLIC char *strptime(const char *buf, const char *format, struct tm *tm)
{
    char weekday[4], month[4], gmt[4];
    int  day, year, hour, minute, second;
    int  wday, mon, i;

    (void) format; // Unused parameter

    memset(tm, 0, sizeof(struct tm));

    if (sscanf(buf, "%3s, %2d %3s %4d %2d:%2d:%2d %3s",
               weekday, &day, month, &year, &hour, &minute, &second, gmt) != 8) {
        return NULL;
    }

    // Validate and set weekday
    wday = -1;
    for (i = 0; i < 7; i++) {
        if (scmp(weekday, weekdays[i]) == 0) {
            wday = i;
            break;
        }
    }
    if (wday == -1) return NULL;
    tm->tm_wday = wday;

    // Validate and set month
    mon = -1;
    for (i = 0; i < 12; i++) {
        if (scmp(month, months[i]) == 0) {
            mon = i;
            break;
        }
    }
    if (mon == -1) return NULL;
    tm->tm_mon = mon;

    // Set other time components
    tm->tm_mday = day;
    tm->tm_year = year - 1900;
    tm->tm_hour = hour;
    tm->tm_min = minute;
    tm->tm_sec = second;

    // Validate GMT
    if (strcmp(gmt, "GMT") != 0) return NULL;

    return (char*) (buf + strlen(buf));
}

#if R_USE_RUN
/*
    Build Windows command string from argv array, quoting arguments with spaces
 */
static char *buildCommandString(char **argv)
{
    RBuf *buf;
    char *result, *arg;
    int  i;

    buf = rAllocBuf(ME_BUFSIZE);
    for (i = 0; argv[i]; i++) {
        arg = argv[i];
        if (i > 0) {
            rPutCharToBuf(buf, ' ');
        }
        if (schr(arg, ' ') || schr(arg, '"')) {
            rPutCharToBuf(buf, '"');
            for (; *arg; arg++) {
                if (*arg == '"') {
                    rPutCharToBuf(buf, '\\');
                }
                rPutCharToBuf(buf, *arg);
            }
            rPutCharToBuf(buf, '"');
        } else {
            rPutStringToBuf(buf, arg);
        }
    }
    rAddNullToBuf(buf);
    result = rBufToStringAndFree(buf);
    return result;
}

PUBLIC int rRun(cchar *command, char **output)
{
    SECURITY_ATTRIBUTES sa;
    STARTUPINFO         si;
    PROCESS_INFORMATION pi;
    HANDLE              stdoutRead, stdoutWrite;
    HANDLE              stderrRead, stderrWrite;
    RBuf                *buf;
    char                **argv, *cmdString, readBuf[ME_BUFSIZE];
    DWORD               bytesRead, exitCode;
    int                 rc;

    if (!command || *command == '\0') {
        return R_ERR_BAD_ARGS;
    }
    if (output) {
        *output = NULL;
    }
    if (rMakeArgs(command, &argv, 0) <= 0) {
        rError("run", "Failed to parse command: %s", command);
        return R_ERR_BAD_ARGS;
    }
    cmdString = buildCommandString(argv);
    if (!cmdString) {
        rFree(argv);
        return R_ERR_MEMORY;
    }

    // Create pipes for stdout and stderr with inheritable child handles
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0)) {
        rError("run", "Failed to create stdout pipe");
        rFree(argv);
        rFree(cmdString);
        return R_ERR_CANT_OPEN;
    }
    if (!CreatePipe(&stderrRead, &stderrWrite, &sa, 0)) {
        rError("run", "Failed to create stderr pipe");
        CloseHandle(stdoutRead);
        CloseHandle(stdoutWrite);
        rFree(argv);
        rFree(cmdString);
        return R_ERR_CANT_OPEN;
    }

    // Ensure read handles are not inherited by child
    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);

    // Configure process startup info
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = stdoutWrite;
    si.hStdError = stderrWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    ZeroMemory(&pi, sizeof(pi));

    // Create the process
    if (!CreateProcess(NULL, cmdString, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        rError("run", "Failed to create process: %s", cmdString);
        CloseHandle(stdoutRead);
        CloseHandle(stdoutWrite);
        CloseHandle(stderrRead);
        CloseHandle(stderrWrite);
        rFree(argv);
        rFree(cmdString);
        return R_ERR_CANT_CREATE;
    }

    // Close write ends of pipes in parent
    CloseHandle(stdoutWrite);
    CloseHandle(stderrWrite);

    // Read output from pipes while waiting for process
    buf = rAllocBuf(ME_BUFSIZE);

    // Read output while process is running to prevent pipe buffer overflow deadlock
    while (1) {
        DWORD waitResult, available;

        // Check if process is still running
        waitResult = WaitForSingleObject(pi.hProcess, 0);

        // Read from stdout if data available
        if (PeekNamedPipe(stdoutRead, NULL, 0, NULL, &available, NULL) && available > 0) {
            if (available > sizeof(readBuf)) {
                available = sizeof(readBuf);
            }
            if (ReadFile(stdoutRead, readBuf, available, &bytesRead, NULL) && bytesRead > 0) {
                if (rGetBufLength(buf) + bytesRead <= R_RUN_MAX_OUTPUT && output) {
                    rPutBlockToBuf(buf, readBuf, bytesRead);
                }
            }
        }

        // Read from stderr if data available
        if (PeekNamedPipe(stderrRead, NULL, 0, NULL, &available, NULL) && available > 0) {
            if (available > sizeof(readBuf)) {
                available = sizeof(readBuf);
            }
            if (ReadFile(stderrRead, readBuf, available, &bytesRead, NULL) && bytesRead > 0) {
                if (rGetBufLength(buf) + bytesRead <= R_RUN_MAX_OUTPUT && output) {
                    rPutBlockToBuf(buf, readBuf, bytesRead);
                }
            }
        }

        // Exit loop if process has terminated
        if (waitResult == WAIT_OBJECT_0) {
            break;
        }

        // Small delay to prevent busy-waiting
        Sleep(10);
    }

    // Read any remaining data in pipes after process exit
    while (PeekNamedPipe(stdoutRead, NULL, 0, NULL, &bytesRead, NULL) && bytesRead > 0) {
        if (bytesRead > sizeof(readBuf)) {
            bytesRead = sizeof(readBuf);
        }
        if (ReadFile(stdoutRead, readBuf, bytesRead, &bytesRead, NULL) && bytesRead > 0) {
            if (rGetBufLength(buf) + bytesRead <= R_RUN_MAX_OUTPUT && output) {
                rPutBlockToBuf(buf, readBuf, bytesRead);
            }
        } else {
            break;
        }
    }
    while (PeekNamedPipe(stderrRead, NULL, 0, NULL, &bytesRead, NULL) && bytesRead > 0) {
        if (bytesRead > sizeof(readBuf)) {
            bytesRead = sizeof(readBuf);
        }
        if (ReadFile(stderrRead, readBuf, bytesRead, &bytesRead, NULL) && bytesRead > 0) {
            if (rGetBufLength(buf) + bytesRead <= R_RUN_MAX_OUTPUT && output) {
                rPutBlockToBuf(buf, readBuf, bytesRead);
            }
        } else {
            break;
        }
    }
    rAddNullToBuf(buf);

    // Get exit code
    if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
        rError("run", "Failed to get exit code");
        rc = R_ERR_CANT_COMPLETE;
    } else if (exitCode != 0) {
        rError("run", "Command failed with status: %d", exitCode);
        rc = (int) exitCode;
    } else {
        rc = 0;
    }

    // Cleanup
    CloseHandle(stdoutRead);
    CloseHandle(stderrRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    rFree(argv);
    rFree(cmdString);

    if (rc != 0) {
        rFreeBuf(buf);
        return rc;
    }

    if (output) {
        *output = rBufToStringAndFree(buf);
    } else {
        rFreeBuf(buf);
    }
    return 0;
}
#endif /* R_USE_RUN */

#else
void winDummy(void)
{
}
#endif /* ME_WIN_LIKE || CYGWIN */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

#else
void dummyR(){}
#endif /* ME_COM_R */
