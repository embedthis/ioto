/*
 * Ioto Library Source
 */

#include "ioto.h"



/********* Start of file ../../../src/config.c ************/

/*
    config.c - Library stub for config()
 */

/********************************** Includes **********************************/



/************************************ Code ************************************/

PUBLIC int ioConfig(Json *config)
{
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/cron.c ************/

/*
   cron.c -- cron management

    The cron module provides routines to test if cron specs are due to
    be run and the time to wait till they can be run.

    Note:  cron specs use the following ranges

        Minutes			0-59
        Hours			0-23
        Days			1-31
        Months			1-12
        Day of week		0-6 (sunday is 0)
 */

/********************************* Includes ***********************************/



/********************************* Defines ************************************/

#define MINUTE  60L
#define HOUR    (60L * 60L)
#define DAY     (24L * 60L * 60L)
#define INVALID 100

typedef struct Cron {
    char *minute;
    char *hour;
    char *day;
    char *month;
    cchar *dayofweek;
} Cron;

static int perMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/*************************** Forward Declarations *****************************/

static int nextValue(int current, cchar *str);
static int atoia(cchar **ptr);
static int between(int m1, int d1, int y1, int m2, int d2, int y2);
static int daysPerMonth(int m, int y);

/*********************************** Code *************************************/
/*
    Convert a string to a cron spec. Return -1 if any part is null. This is a
    rudimentary check to see if the cron spec is OK. It could (should?) be improved.
 */
static Cron *cronAlloc(cchar *spec)
{
    Cron *cp;
    char *buf, *rest;
    int  mth;

    cp = rAllocType(Cron);

    /*
        Convenient aliases
     */
    if (scmp(spec, "all") == 0) {
        spec = "* * * * *";
    } else if (scmp(spec, "weekdays") == 0) {
        spec = "* * * * 1-5";
    } else if (scmp(spec, "workhours") == 0) {
        spec = "* 9-17 * * 1-5";
    } else if (scmp(spec, "midnight") == 0) {
        spec = "* 0 * * *";
    }
    buf = sclone(spec);

    /*
        cp->minute holds the values
     */
    cp->minute = stok(buf, " ", &rest);
    cp->hour = stok(rest, " ", &rest);
    cp->day = stok(rest, " ", &rest);
    cp->month = stok(rest, " ", &rest);
    cp->dayofweek = stok(rest, " ", &rest);

    mth = (int) stoi(cp->month);
    if (cp->minute == NULL || cp->hour == NULL || cp->day == NULL ||
        cp->month == NULL || cp->dayofweek == NULL ||
        ((mth < 1 || mth > 12) && !smatch(cp->month, "*"))) {
        return 0;
    }
    return cp;
}

static void cronFree(Cron *cs)
{
    assert(cs);
    /*
        The other elements point into the minute array, so we only need to free it
     */
    if (cs->minute) {
        rFree(cs->minute);
    }
    rFree(cs);
}

/*
    Return the time to wait till the next valid time to run a cron entry.
 */
Ticks cronUntil(cchar *spec, Time when)
{
    Cron      *cp;
    struct tm *tm;
    Time      now;
    time_t    t;
    int       tm_mon, tm_mday, tm_wday, wday, m, min, h, hr, carry, day,
              days, d1, day1, carry1, d2, day2, carry2, daysahead, mon,
              yr, db, wd, today;

    assert(spec);

    cp = cronAlloc(spec);

    now = rGetTime() / TPS;
    t = (time_t) when / TPS;
    tm = localtime(&t);

    tm_mon = nextValue(tm->tm_mon + 1, cp->month) - 1;
    tm_mday = nextValue(tm->tm_mday, cp->day);
    tm_wday = nextValue(tm->tm_wday, cp->dayofweek);
    today = 1;

    if ((scmp(cp->day, "*") == 0 && tm->tm_wday != tm_wday)
        || (scmp(cp->dayofweek, "*") == 0 && tm->tm_mday != tm_mday)
        || (tm->tm_mday != tm_mday && tm->tm_wday != tm_wday)
        || (tm->tm_mon != tm_mon)) {
        today = 0;
    }
    m = tm->tm_min;
    if ((tm->tm_hour + 1) <= nextValue(tm->tm_hour % 24, cp->hour)) {
        m = 0;
    }
    min = nextValue(m % 60, cp->minute);
    carry = (min < m) ? 1 : 0;
    h = tm->tm_hour + carry;
    hr = nextValue(h % 24, cp->hour);
    carry = (hr < h) ? 1:0;

    /*
        Today's events
     */
    if (!carry && today) {
        if (tm->tm_min > min) {
            t += (Ticks) (hr - tm->tm_hour - 1) * HOUR + (Ticks) (60 - tm->tm_min + min) * MINUTE;
        } else {
            t += (Ticks) (hr - tm->tm_hour) * HOUR + (Ticks) (min - tm->tm_min) * MINUTE;
        }
        t -= (long) tm->tm_sec + now;
        if (t < 0) {
            t = 0;
        }
        cronFree(cp);
        return (Ticks) t * TPS;
    }

    min = nextValue(0, cp->minute);
    hr = nextValue(0, cp->hour);

    /*
        Get the due date of this event
     */
    d1 = tm->tm_mday + 1;
    days = daysPerMonth(tm->tm_mon, tm->tm_year);
    day1 = nextValue((d1 - 1) % days + 1, cp->day);
    carry1 = (day1 < d1) ? 1 : 0;

    d2 = tm->tm_wday + 1;
    wday = nextValue(d2 % 7, cp->dayofweek);
    if (wday < d2) {
        daysahead = 7 - d2 + wday;
    } else {
        daysahead = wday - d2;
    }
    day2 = (d1 + daysahead - 1) % days + 1;
    carry2 = (day2 < d1) ? 1:0;

    /*
        Work out whether to use the day of month or day of week specs
     */
    if ((scmp(cp->day, "*") == 0) && (scmp(cp->dayofweek, "*") != 0)) {
        day1 = day2;
        carry1 = carry2;
    }
    if ((scmp(cp->day, "*") != 0) && (scmp(cp->dayofweek, "*") == 0)) {
        day2 = day1;
        carry2 = carry1;
    }
    yr = tm->tm_year;
    if ((carry1 && carry2) || (tm->tm_mon != tm_mon)) {
        /*
            Events that don't occur this month
         */
        m = tm->tm_mon + 1;
        mon = nextValue(m % 12 + 1, cp->month) - 1;
        carry = (mon < m) ? 1 : 0;
        yr += carry;
        day1 = nextValue(1, cp->day);
        db = between(tm->tm_mon, tm->tm_mday, tm->tm_year, mon, 1, yr) + 1;
        wd = (tm->tm_wday + db) % 7;
        wday = nextValue(wd, cp->dayofweek);
        if (wday < wd) {
            day2 = 1 + 7 - wd + wday;
        } else {
            day2 = 1 + wday - wd;
        }
        if ((scmp(cp->day, "*") != 0) &&
            (scmp(cp->dayofweek, "*") == 0)) {
            day2 = day1;
        }
        if ((scmp(cp->day, "*") == 0) &&
            (scmp(cp->dayofweek, "*") != 0)) {
            day1 = day2;
        }
        day = (day1 < day2) ? day1 : day2;

    } else {
        /*
            Events that occur this month
         */
        mon = tm->tm_mon;
        if (!carry1 && !carry2) {
            day = (day1 < day2) ? day1 : day2;
        } else if (!carry1) {
            day = day1;
        } else {
            day = day2;
        }
    }
    days = between(tm->tm_mon, tm->tm_mday, tm->tm_year, mon, day, yr);

    t += (Ticks) (23 - tm->tm_hour) * HOUR + (Ticks) (60 - tm->tm_min) * MINUTE
         + (Ticks) hr * HOUR + (Ticks) min * MINUTE + (Ticks) days * DAY;

    t -= (long) tm->tm_sec + now;
    if (t < 0) {
        t = 0;
    }
    cronFree(cp);
    return (Ticks) t * TPS;
}

/*
    Return the next valid value for a particular cron field that is greater or equal
    to the current field.  If no valid values exist the smallest of the list is returned.
 */
static int nextValue(int current, cchar *str)
{
    cchar *ptr;
    int   n, n2, min, min_gt;

    if (scmp(str, "*") == 0) {
        return current;
    }
    ptr = str;
    min = INVALID;
    min_gt = INVALID;
    while (1) {
        if ((n = atoia(&ptr)) == current) {
            return current;
        }
        if (n < min) {
            min = n;
        }
        if ((n > current) && (n < min_gt)) {
            min_gt = n;
        }
        if (*ptr == '-') {
            ptr++;
            if ((n2 = atoia(&ptr)) > n) {
                if ((current > n) && (current <= n2)) {
                    return current;
                }
            } else {
                if (current > n || current <= n2) {
                    return current;
                }
            }
        }
        if (*ptr == '\0') {
            break;
        }
        ptr += 1;
    }
    if (min_gt != INVALID) {
        return min_gt;
    }else return min;
}

/*
    ascii to integer with pointer advance
 */
static int atoia(cchar **ptr)
{
    Ticks n;

    n = 0;
    while (isdigit((int) **ptr)) {
        n = n * 10 + (**ptr - '0');
        *ptr += 1;
    }
    return (int) n;
}

/*
    Return the number of complete days between two dates
 */
static int between(int m1, int d1, int y1, int m2, int d2, int y2)
{
    int days, m;

    if ((m1 == m2) && (d1 == d2) && (y1 == y2))
        return 0;
    if ((m1 == m2) && (d1 < d2))
        return d2 - d1 - 1;
    /*
        These are not in the same month
     */
    days = (daysPerMonth(m1, y1) - d1) + (d2 - 1);
    m = (m1 + 1) % 12;
    while (m != m2) {
        if (m == 0)
            y1++;
        days += daysPerMonth(m, y1);
        m = (m + 1) % 12;
    }
    return days;
}

/*
    Return the number of days in a month

    Leap year rule: a year is a leap year if it is divisible by 4, but not
    by 100, except that years divisible by 400 ARE leap years.
 */
static int daysPerMonth(int m, int y)
{
    return perMonth[m] + (((m == 1) && ((y % 4) == 0) && ((y % 100) != 0)) ||
                          ((m == 1) && (y % 400 == 0)) ? 1 : 0);
}


/********* Start of file ../../../src/database.c ************/

/*
    database.c - Embedded database

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/********************************** Forwards **********************************/

#if SERVICES_DATABASE

static void dbService(void);

/************************************* Code ***********************************/

PUBLIC int ioInitDb(void)
{
    Ticks maxAge, service;
    char  *path, *schema;
    ssize maxSize;
    int   flags;

    schema = rGetFilePath(jsonGet(ioto->config, 0, "database.schema", "@config/schema.json5"));
    path = rGetFilePath(jsonGet(ioto->config, 0, "database.path", "@db/device.db"));

    flags = ioto->nosave ? DB_READ_ONLY : 0;
    if ((ioto->db = dbOpen(path, schema, flags)) == 0) {
        rError("database", "Cannot open database %s with schema %s", path, schema);
        rFree(path);
        rFree(schema);
        return R_ERR_CANT_OPEN;
    }
    rFree(path);
    rFree(schema);

    maxAge = svalue(jsonGet(ioto->config, 0, "database.maxJournalAge", "1min")) * TPS;
    maxSize = svalue(jsonGet(ioto->config, 0, "database.maxJournalSize", "1mb"));
    service = svalue(jsonGet(ioto->config, 0, "database.service", "1hour")) * TPS;
    dbSetJournalParams(ioto->db, maxAge, maxSize);

    dbAddContext(ioto->db, "deviceId", ioto->id);
#if SERVICES_CLOUD
    if (ioto->account) {
        dbAddContext(ioto->db, "accountId", ioto->account);
    }
#endif
#if SERVICES_SYNC
    if (dbGet(ioto->db, "SyncState", NULL, NULL) == 0) {
        dbCreate(ioto->db, "SyncState", DB_PROPS("lastSyncDown", "0", "lastUpdate", "0"), NULL);
    }
    if (ioto->syncService && (ioInitSync() < 0)) {
        return R_ERR_CANT_READ;
    }
#endif
    ioUpdateDevice();
    if (service) {
        rStartEvent((RFiberProc) dbService, 0, service);
    }
    return 0;
}

PUBLIC void ioTermDb(void)
{
    if (ioto->db) {
        if (ioto->nosave) {
            dbSave(ioto->db, NULL);
        }
        dbClose(ioto->db);
        ioto->db = 0;
    }
}

PUBLIC void ioRestartDb(void)
{
    ioTermDb();
    ioInitDb();
}

static void dbService(void)
{
    Ticks service;

    dbRemoveExpired(ioto->db, 1);
    service = svalue(jsonGet(ioto->config, 0, "database.service", "1day")) * TPS;
    rStartEvent((RFiberProc) dbService, 0, service);
}

/*
    Update the ioto-device Device entry with properties from device.json
 */
PUBLIC void ioUpdateDevice(void)
{
    Json *json;

    assert(ioto->id);

    json = jsonAlloc(0);
    jsonSet(json, 0, "id", ioto->id, JSON_STRING);
#if SERVICES_CLOUD
    if (ioto->account) {
        jsonSet(json, 0, "accountId", ioto->account, JSON_STRING);
    }
#endif
    jsonSet(json, 0, "description", jsonGet(ioto->config, 0, "device.description", 0), JSON_STRING);
    jsonSet(json, 0, "model", jsonGet(ioto->config, 0, "device.model", 0), JSON_STRING);
    jsonSet(json, 0, "name", jsonGet(ioto->config, 0, "device.name", 0), JSON_STRING);
    jsonSet(json, 0, "product", jsonGet(ioto->config, 0, "device.product", 0), JSON_STRING);
    jsonSet(json, 0, "state", "online", JSON_STRING);

    if (rEmitLog("trace", "sync")) {
        rInfo("database", "Update device item");
    }
    if (dbCreate(ioto->db, "Device", json, DB_PARAMS(.upsert = 1)) == 0) {
        rError("sync", "Cannot update device item in database: %s", dbGetError(ioto->db));
    }
    jsonFree(json);
}

#else
void dummyDatabase()
{
}
#endif /* SERVICES_DATABASE */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/esp32.c ************/

/**
    esp32.c - Initialization on ESP32 microcontrollers

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



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


/********* Start of file ../../../src/ioto.c ************/

/*
    ioto.c - Primary Ioto control

    This code runs on a fiber and can block, yield and create fibers.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/*********************************** Globals **********************************/

PUBLIC Ioto *ioto = NULL;

/*********************************** Defines **********************************/

#define TRACE_FILTER         "stderr:raw,error,info,!debug:all,!mbedtls"
#define TRACE_VERBOSE_FILTER "stdout:raw,error,info,trace,!debug:all,!mbedtls"
#define TRACE_DEBUG_FILTER   "stdout:all:all,!mbedtls"
#define TRACE_FORMAT         "%A: %M"

/*********************************** Forwards *********************************/

static int initServices(void);

/************************************* Code ***********************************/
/*
    Allocate the global Ioto singleton
 */
PUBLIC Ioto *ioAlloc(void)
{
    ioto = rAllocType(Ioto);
    return ioto;
}

PUBLIC void ioFree(void)
{
    if (ioto) {
        rFree(ioto);
    }
    ioto = 0;
}

/*
    Initialize after ioInitSetup
 */
PUBLIC void ioInit(void)
{
    assert(!rIsMain());

    if (initServices() < 0) {
        rError("ioto", "Exiting ...");
        rStop();
        return;
    }
    if (rGetState() != R_INITIALIZED) {
        return;
    }
    ioto->ready = 1;
    rSetState(R_READY);
    rInfo("ioto", "Ioto ready");
    rSignal("app:ready", 0);
    ioStart();
}

/*
    Terminate Ioto.
    If doing a reset, run the script "scripts/reset" before leaving.
 */
PUBLIC void ioTerm(void)
{
#if ME_UNIX_LIKE
    char *output, *script;
    int  status;

    script = 0;
    if (rGetState() == R_RESTART) {
        //  TermServices will release ioto->config. Get persistent reference to script.
        script = jsonGetClone(ioto->config, 0, "scripts.reset", 0);
    }
#endif
    ioto->ready = 0;
    ioStop();
#if SERVICES_WEB
    ioTermWeb();
#endif
#if SERVICES_CLOUD
    ioTermCloud();
#endif
#if SERVICES_DATABASE
    ioTermDb();
#endif
    ioTermConfig();

#if ME_UNIX_LIKE
    if (script && *script) {
        output = rRun(script, &status);
        if (status != 0) {
            rError("ioto", "Reset script failure: %d, %s", status, output);
        }
    }
    rFree(script);
#endif
}

/*
    Start the Ioto runtime
 */
PUBLIC int ioStartRuntime(int verbose)
{
    cchar *filter;

    if (rInit((RFiberProc) NULL, NULL) < 0) {
        return R_ERR_CANT_INITIALIZE;
    }
    if (verbose == 1) {
        filter = TRACE_VERBOSE_FILTER;
    } else if (verbose >= 2) {
        filter = TRACE_DEBUG_FILTER;
    } else {
        filter = NULL;
    }
    if (filter && rSetLog(filter, NULL, 1) < 0) {
        rTerm();
        return R_ERR_CANT_INITIALIZE;
    }
    ioAlloc();
    return 0;
}

PUBLIC void ioStopRuntime(void)
{
    rTerm();
}

/*
    Run Ioto. This will block and service events forever (or till instructed to stop)
    Should be called from main()
    The fn argument is not used, but helps build systems ensure it is included in the build.
 */
PUBLIC int ioRun(void *fn)
{
    rSleep(0);

    while (rGetState() < R_STOPPING) {
        if (ioInitConfig() < 0) {
            rFatal("ioto", "Cannot initialize Ioto\n");
        }
        if (rSpawnFiber("ioInit", (RFiberProc) ioInit, NULL) < 0) {
            rFatal("ioto", "Cannot initialize runtime");
        }
        if (rGetState() < R_STOPPING) {
            //  Service events until instructed to exit
            rServiceEvents();
        }
        ioTerm();
        if (rGetState() == R_RESTART) {
            rTerm();
            rInit(NULL, NULL);
        }
    }
    ioFree();
    rInfo("ioto", "Ioto exiting");
    return 0;
}

/*
    Start services. Order of initialization matters.
    We initialize MQTT early so that on-demand connections and provisioning may take place.
    Return false if initialization failed. Note: some services may trigger a R_RESTART.
 */
static int initServices(void)
{
#ifdef SERVICES_SERIALIZE
    if (ioto->serializeService) {
        ioSerialize();
    }
#endif
#if SERVICES_REGISTER
    /*
        One-time device registration during manufacturer or first connect.
        NOTE: The Ioto volume license requires that if this code is removed or disabled, you must manually
        enter and update accurate device volumes using the Embedthis Builder at https://admin.embedthis.com.
     */
    if (ioto->registerService) {
        if (ioRegister() == R_ERR_BAD_ARGS) {
            return R_ERR_BAD_ARGS;
        }
    } else
#endif
    rInfo("ioto", "The LICENSE requires that you declare device volumes at https://admin.embedthis.com");
#if SERVICES_DATABASE
    if (ioto->dbService && ioInitDb() < 0) {
        return R_ERR_CANT_READ;
    }
#endif
#if SERVICES_WEB
    if (ioto->webService && ioInitWeb() < 0) {
        return R_ERR_CANT_INITIALIZE;
    }
#endif
#if SERVICES_MQTT
    if (ioto->mqttService && (ioInitMqtt() < 0)) {
        return R_ERR_CANT_INITIALIZE;
    }
#endif
#if SERVICES_CLOUD
    if (ioto->provisionService && ioInitCloud() < 0) {
        return R_ERR_CANT_INITIALIZE;
    }
#endif

#if SERVICES_UPDATE
    if (ioto->updateService) {
        //  Delay to allow provisioning to complete
        rStartEvent((RFiberProc) ioUpdate, 0, 15 * TPS);
    }
#endif
#if ME_DEBUG
    //  Used to test memory leaks after running for a period of time
    if (getenv("VALGRIND")) {
        rStartEvent((REventProc) rStop, 0, 60 * TPS);
    }
#endif
    return 0;
}

/*
    Update the log output configuration. This may be called at startup and after cloud provisioning to
    redirect the device log to the cloud.
 */
PUBLIC int ioUpdateLog(bool force)
{
    Json  *json;
    cchar *dir, *format, *path, *source, *types;
    char  *fullPath;

    json = ioto->config;
    types = source = 0;

    format = jsonGet(json, 0, "log.format", "%T %S: %M");
    path = jsonGet(json, 0, "log.path", NULL);
    source = jsonGet(json, 0, "log.sources", "all,!mbedtls");
    types = jsonGet(json, 0, "log.types", "error,info");
    dir = jsonGet(json, 0, "directories.log", "");

    rSetLogFormat(format, force);
    rSetLogFilter(types, source, force);

    if (smatch(path, "default")) {
        path = IO_LOG_FILE;

#if SERVICES_CLOUD
    } else if (smatch(path, "cloud")) {
        if (ioto->awsAccess) {
            //  This will register a new log handler
            ioEnableCloudLog();
        }
        return 0;
#endif
    }
    fullPath = rJoinFile(dir, path);
    if (rSetLogPath(fullPath, force) < 0) {
        rError("ioto", "Cannot open log %s", fullPath);
        rFree(fullPath);
        return R_ERR_CANT_OPEN;
    }
    rFree(fullPath);
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/mqtt.c ************/

/*
    mqtt.c - MQTT service setup

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



#if SERVICES_MQTT

/*********************************** Locals **********************************/

#define RR_DEFAULT_TIMEOUT  (30 * TPS);
#define CONNECT_MAX_RETRIES 5

/*
    Mqtt request/response support
 */
typedef struct RR {
    char *topic;        /* Subscribed topic */
    REvent timeout;     /* Timeout event */
    RFiber *fiber;      /* Wait fiber */
    int seq;            /* Unique request sequence number (can wrap) */
} RR;

static ssize nextRr = 99;
static Time  lastDisconnect = 0;

/************************************ Forwards ********************************/

static int attachSocket(int retry);
static void onEvent(Mqtt *mq, int event);
static void rrResponse(MqttRecv *rp);
static void throttle(MqttRecv *rp);

/************************************* Code ***********************************/

PUBLIC int ioInitMqtt(void)
{
    Ticks timeout;

    if ((ioto->mqtt = mqttAlloc(ioto->id, onEvent)) == NULL) {
        rError("mqtt", "Cannot create MQTT instance");
        return R_ERR_MEMORY;
    }
    ioto->rr = rAllocList(0, 0);
    mqttSetMessageSize(ioto->mqtt, IO_MESSAGE_SIZE);

    timeout = svalue(jsonGet(ioto->config, 0, "mqtt.timeout", "1 min")) * TPS;
    mqttSetTimeout(ioto->mqtt, timeout);
    ioScheduleConnect();
    return 0;
}

PUBLIC void ioTermMqtt(void)
{
    RR  *rp;
    int index;

    if (ioto->mqtt) {
        mqttFree(ioto->mqtt);
        ioto->mqtt = 0;
        ioto->connected = 0;
    }
    if (ioto->mqttSocket) {
        rFreeSocket(ioto->mqttSocket);
        ioto->mqttSocket = 0;
    }
    for (ITERATE_ITEMS(ioto->rr, rp, index)) {
        rFree(rp->topic);
    }
    rFreeList(ioto->rr);
}

/*
    Schedule a cloud connection according to the mqtt schedule
 */
PUBLIC void ioScheduleConnect(void)
{
    Time  delay, jitter, now, when, wait;
    cchar *schedule;

    schedule = jsonGet(ioto->config, 0, "mqtt.schedule", 0);
    if (!ioto->scheduledConnect) {
        delay = svalue(jsonGet(ioto->config, 0, "mqtt.delay", "0")) * TPS;
        now = rGetTime();
        when = lastDisconnect + delay;
        if (when < now) {
            when = now;
        }
        if (schedule) {
            wait = cronUntil(schedule, when);
        } else {
            wait = 0;
        }
        if (wait <= 0) {
            ioConnect();
        } else {
            jitter = svalue(jsonGet(ioto->config, 0, "mqtt.jitter", "0")) * TPS;
            if (jitter) {
                jitter = rand() % jitter;
                wait += jitter;
            }
            if (ioto->scheduledConnect) {
                rStopEvent(ioto->scheduledConnect);
            }
            rInfo("mqtt", "Schedule MQTT connect in %d secs", (int) wait / TPS);
            ioto->scheduledConnect = rStartEvent((REventProc) ioConnect, 0, wait);
        }
    }
}

/*
    Connect to the cloud. This will provision if required and may block a long time.
    NOTE: there may be multiple callers and so as fiber code, it used rEnter/rLeave.
 */
PUBLIC int ioConnect(void)
{
    static int  reprovisions = 0;
    static bool connecting = 0;
    int         i, maxReprovision, maxRetries;

    if (ioto->connected) {
        return 0;
    }
    maxRetries = CONNECT_MAX_RETRIES;

    ioWakeProvisioner();
    rEnter(&connecting, 0);

    /*
        Retry connection attempts
     */
    for (i = 0; i < maxRetries && !ioto->connected; i++) {
        if (!ioto->endpoint) {
            //  This may block a long time waiting for the device to be claimed
            ioProvision();
        }
        if (ioto->endpoint && attachSocket(i) == 0) {
            break;
        }
        /*
            If the connection failed and we have a valid internet connection, the device may have
            been released, so reprovision.
         */
        if (rCheckInternet()) {
            maxReprovision = jsonGetInt(ioto->config, 0, "limits.reprovision", 5);
            if (reprovisions++ >= maxReprovision) {
                rError("mqtt", "Too many reprovision requests");
                i = maxRetries;
                break;
            }
            if (i >= 2) {
                rInfo("mqtt", "Device cloud connection failed despite good internet connection");
                ioDeprovision();
            }
        }
        rSleep(TPS * i);
    }
    rLeave(&connecting);
    if (i >= maxRetries) {
        lastDisconnect = rGetTime();
        ioScheduleConnect();
        return R_ERR_CANT_CONNECT;
    }
    return 0;
}

/*
    Called to force a disconnect
 */
PUBLIC void ioDisconnect()
{
    if (ioto->mqttSocket) {
        rCloseSocket(ioto->mqttSocket);
    }
    ioto->connected = 0;
}

/*
    Attach a socket to the MQTT object. Called only from ioConnect().
 */
static int attachSocket(int retry)
{
    char    topic[MQTT_TOPIC_SIZE];
    RSocket *sock;
    Json    *config;
    cchar   *alpn, *endpoint;
    char    *authority, *certificate, *key;
    int     mid, pid, port;

    if (ioto->mqttSocket) {
        rFreeSocket(ioto->mqttSocket);
        ioto->mqttSocket = 0;
    }
    config = ioto->config;
    if ((mid = jsonGetId(config, 0, "mqtt")) < 0) {
        rError("mqtt", "Cannot find Mqtt configuration");
        return R_ERR_CANT_INITIALIZE;
    }
    endpoint = jsonGet(config, mid, "endpoint", 0);
    port = jsonGetInt(config, mid, "port", 443);
    alpn = jsonGet(config, mid, "alpn", "x-amzn-mqtt-ca");

    authority = rGetFilePath(jsonGet(config, mid, "authority", 0));

    if ((pid = jsonGetId(config, 0, "provision")) >= 0) {
        certificate = rGetFilePath(jsonGet(config, pid, "certificate",
                                           jsonGet(config, mid, "certificate", 0)));
        key = rGetFilePath(jsonGet(config, pid, "key", jsonGet(config, mid, "key", 0)));
        endpoint = jsonGet(config, pid, "endpoint", endpoint);
        port = jsonGetInt(config, pid, "port", port);
    } else {
        certificate = rGetFilePath(jsonGet(config, mid, "certificate", 0));
        key = rGetFilePath(jsonGet(config, mid, "key", 0));
    }
    if (!endpoint || !port) {
        rInfo("mqtt", "Mqtt endpoint:port not yet defined or provisioned");
        return 0;
    }
    if ((sock = rAllocSocket()) == 0) {
        rError("mqtt", "Cannot allocate socket");
        return R_ERR_MEMORY;
    }
    if (key || certificate || authority) {
        rSetSocketCerts(sock, authority, key, certificate, NULL);
        rFree(key);
        rFree(certificate);
        rFree(authority);
        rSetSocketVerify(sock, 1, 1);
        if (alpn) {
            rSetTlsAlpn(sock->tls, alpn);
        }
    }
    if (rConnectSocket(sock, endpoint, port, 0) < 0) {
        if (retry == 0) {
            rError("mqtt", "Cannot connect to socket at %s:%d %s", endpoint, port,
                   sock->error ? sock->error : "");
        }
        rFreeSocket(sock);
        return R_ERR_CANT_CONNECT;
    }
    if (mqttConnect(ioto->mqtt, sock, 0, MQTT_WAIT_ACK) < 0) {
        rDebug("mqtt", "Cannot connect with MQTT");
        rFreeSocket(sock);
        return R_ERR_CANT_CONNECT;
    }
    ioto->mqttSocket = sock;
    ioto->connected = 1;
    ioto->mqttErrors = 0;

    /*
        Setup a master subscription for ioto/device/ID
        Subsequent subscriptions that use this prefix will not incurr a cloud MQTT subscription
     */
    mqttSubscribeMaster(ioto->mqtt, 1, MQTT_WAIT_NONE, "ioto/device/%s/#", ioto->id);
    mqttSubscribeMaster(ioto->mqtt, 1, MQTT_WAIT_NONE, "ioto/account/all/#");
    mqttSubscribeMaster(ioto->mqtt, 1, MQTT_WAIT_NONE, "ioto/account/%s/#", ioto->account);

    /*
        Setup the device cloud throttle indicator. This is vital to ensure well behaved clients.
     */
    mqttSubscribe(ioto->mqtt, throttle, 1, MQTT_WAIT_NONE | MQTT_WAIT_FAST,
                  SFMT(topic, "ioto/device/%s/mqtt/throttle", ioto->id));

    rSignal("mqtt:connected", 0);
    rInfo("mqtt", "Connected to mqtt://%s:%d", endpoint, port);
    return 0;
}

static void throttle(MqttRecv *rp)
{
    mqttThrottle(ioto->mqtt);
    rSignal("mqtt:throttle", 0);
}

static void onEvent(Mqtt *mqtt, int event)
{
    if (rGetState() != R_READY) {
        return;
    }
    switch (event) {
    case MQTT_EVENT_ATTACH:
        ioConnect();
        break;

    case MQTT_EVENT_DISCONNECT:
        rInfo("mqtt", "Cloud connection closed");
        rFreeSocket(ioto->mqttSocket);
        ioto->mqttSocket = 0;
        ioto->connected = 0;
        rSignal("mqtt:disconnect", 0);
        lastDisconnect = rGetTime();
        ioScheduleConnect();
        break;

    case MQTT_EVENT_TIMEOUT:
        //  Respond to timeout and force a disconnection
        rCloseSocket(ioto->mqttSocket);
    }
}

/*
    Alloc a request/response. This manages the MQTT subscriptions for specific topics.
 */
static RR *allocRR(Mqtt *mq, cchar *topic)
{
    char subscribe[MQTT_TOPIC_SIZE];
    RR   *rr, *rp;
    int  index;

    rr = rAllocType(RR);
    rr->fiber = rGetFiber();
    if (++nextRr >= MAXINT) {
        nextRr = 1;
    }
    rr->seq = (int) nextRr;
    for (ITERATE_ITEMS(ioto->rr, rp, index)) {
        if (smatch(rp->topic, topic)) {
            break;
        }
    }
    if (!rp) {
        //  Subscribe to all sequence numbers on this topic
        SFMT(subscribe, "%s/+", topic);
        mqttSubscribe(mq, rrResponse, 1, MQTT_WAIT_NONE, subscribe);
        rr->topic = sclone(topic);
    }
    rPushItem(ioto->rr, rr);
    return rr;
}

static void freeRR(Mqtt *mq, cchar *topic)
{
    RR  *rp;
    int index;

    for (ITERATE_ITEMS(ioto->rr, rp, index)) {
        if (smatch(rp->topic, topic)) {
            break;
        }
    }
    if (rp) {
        mqttUnsubscribe(mq, topic, MQTT_WAIT_NONE);
        rFree(rp->topic);
        rFree(rp);
    }
}

/*
    Process a response
 */
static void rrResponse(MqttRecv *rp)
{
    RR  *rr;
    int index, seq;

    seq = (int) stoi(rBasename(rp->topic));
    for (ITERATE_ITEMS(ioto->rr, rr, index)) {
        if (rr->seq == seq) {
            if (rr->timeout) {
                rStopEvent(rr->timeout);
            }
            rResumeFiber(rr->fiber, (void*) sclone(rp->data));
            rRemoveItem(ioto->rr, rr);
            return;
        }
    }
    rDebug("mqtt", "Got unmatched RR response: %d", seq);
}

/*
    Timeout a request
 */
static void rrTimeout(RR *rr)
{
    RFiber *fiber;

    rInfo("mqtt", "MQTT request timed out");
    rRemoveItem(ioto->rr, rr);
    fiber = rr->fiber;
    rResumeFiber(fiber, 0);
}

/*
    Initiate a request
 */
PUBLIC char *mqttRequest(Mqtt *mq, cchar *body, Ticks timeout, cchar *topicFmt, ...)
{
    va_list ap;
    RR      *rr;
    char    publish[MQTT_TOPIC_SIZE], subscribe[MQTT_TOPIC_SIZE], topic[MQTT_TOPIC_SIZE];

    va_start(ap, topicFmt);
    sfmtbufv(topic, sizeof(topic), topicFmt, ap);
    va_end(ap);

    SFMT(subscribe, "ioto/device/%s/%s", ioto->id, topic);
    if ((rr = allocRR(mq, subscribe)) == 0) {
        return NULL;
    }
    timeout = timeout > 0 ? timeout : RR_DEFAULT_TIMEOUT;
    if (!rGetTimeouts()) {
        timeout = MAXINT;
    }
    rr->timeout = rStartEvent((REventProc) rrTimeout, rr, timeout);

    SFMT(publish, "ioto/service/%s/%s/%d", ioto->id, topic, rr->seq);
    if (mqttPublish(ioto->mqtt, body, -1, 1, MQTT_WAIT_NONE, publish) < 0) {
        return NULL;
    }
    //  Returns null on a timeout. Caller must free result.
    return rYieldFiber(0);
}

PUBLIC void mqttRequestFree(Mqtt *mq, cchar *topicFmt, ...)
{
    va_list ap;
    char    subscribe[MQTT_TOPIC_SIZE], topic[MQTT_TOPIC_SIZE];

    va_start(ap, topicFmt);
    sfmtbufv(topic, sizeof(topic), topicFmt, ap);
    va_end(ap);

    SFMT(subscribe, "ioto/device/%s/%s", ioto->id, topic);
    freeRR(mq, subscribe);
}

/*
    Get an accumulated metric value for a period
    Dimensions is a JSON object.
 */
PUBLIC double ioGetMetric(cchar *metric, cchar *dimensions, cchar *statistic, int period)
{
    char   *result;
    char   msg[1024];
    double num;

    if (dimensions == NULL || *dimensions == '\0') {
        dimensions = "{\"Device\":\"deviceId\"}";
    }
    SFMT(msg, "{\"metric\":\"%s\",\"dimensions\":%s,\"period\":%d,\"statistic\":\"%s\"}",
         metric, dimensions, period, statistic);
    result = mqttRequest(ioto->mqtt, msg, 0, "metric/get");
    num = stod(result);
    rFree(result);
    return num;
}

/*
    Define a metric in the Embedthis/Device namespace.
    Dimensions is a JSON array of objects. Each object can contain multiple properties.
    that define the dimension. The {} object, means all dimensions,
 */
PUBLIC void ioSetMetric(cchar *metric, double value, cchar *dimensions, int elapsed)
{
    char msg[160];

    if (dimensions == NULL || *dimensions == '\0') {
        dimensions = "[{\"Device\":\"deviceId\"}]";
    }
    SFMT(msg,
         "{\"metric\":\"%s\",\"value\":%lf,\"dimensions\":%s,\"buffer\":{\"elapsed\":%d}}",
         metric, value, dimensions, elapsed);
    mqttPublish(ioto->mqtt, msg, -1, 1, MQTT_WAIT_NONE,
                "$aws/rules/IotoDevice/ioto/service/%s/metric/set", ioto->id);
}

PUBLIC void ioSet(cchar *key, cchar *value)
{
    char msg[160];

    SFMT(msg, "{\"key\":\"%s\",\"value\":\"%s\",\"type\":\"string\"}", key, value);
    //  Optimize by using AWS basic ingest topic
    mqttPublish(ioto->mqtt, msg, -1, 1, MQTT_WAIT_NONE,
                "$aws/rules/IotoDevice/ioto/service/%s/store/set", ioto->id);
}

PUBLIC void ioSetNum(cchar *key, double value)
{
    char msg[160];

    SFMT(msg, "{\"key\":\"%s\",\"value\":%lf,\"type\":\"number\"}", key, value);
    //  Optimize by using AWS basic ingest topic
    mqttPublish(ioto->mqtt, msg, -1, 1, MQTT_WAIT_NONE,
                "$aws/rules/IotoDevice/ioto/service/%s/store/set", ioto->id);
}

//  Caller must free
PUBLIC char *ioGet(cchar *key)
{
    char msg[160];

    SFMT(msg, "{\"key\":\"%s\"}", key);
    //  Must not use basic-ingest for mqttRequest
    return mqttRequest(ioto->mqtt, msg, 0, "store/get");
}

PUBLIC double ioGetNum(cchar *key)
{
    char   msg[160], *result;
    double num;

    SFMT(msg, "{\"key\":\"%s\"}", key);
    //  Must not use basic-ingest for mqttRequest
    result = mqttRequest(ioto->mqtt, msg, 0, "store/get");
    num = stod(result);
    rFree(result);
    return num;
}

PUBLIC void ioOnConnect(RWatchProc fn, void *arg, bool direct)
{
    //  Watch for future connections
    rWatch("mqtt:connected", fn, arg);

    //  Invoke if already connected
    if (ioto->connected) {
        if (direct) {
            fn(NULL, arg);
        } else {
            rSpawnFiber("onconnect", (RFiberProc) fn, arg);
        }
    }
}

PUBLIC void ioOnConnectOff(RWatchProc fn, void *arg)
{
    rWatchOff("mqtt:connected", fn, arg);
}

#else
void dummyMqttImport(void)
{
}
#endif /* SERVICES_MQTT */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/register.c ************/

/*
    register.c - One-time device registration during manufacturer or first connect.

    NOTE: The Ioto license requires that if this code is removed or disabled, you must
    manually enter and maintain device volumes using Embedthis Builder (https://admin.embedthis.com).

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



#if SERVICES_REGISTER
/*********************************** Forwards *********************************/

static int parseRegisterResponse(Json *json);

/************************************* Code ***********************************/
/*
    Send a registration request to the builder
 */
PUBLIC int ioRegister(void)
{
    Json       *params, *response;
    char       *data, *path, url[160];
    int        rc;
    static int once = 0;

#if SERVICES_CLOUD
    if (ioto->api && ioto->apiToken) {
        rInfo("register", "Device registered and claimed by %s cloud \"%s\" in %s",
              jsonGet(ioto->config, 0, "provision.cloudType", 0),
              jsonGet(ioto->config, 0, "provision.cloudName", 0),
              jsonGet(ioto->config, 0, "provision.cloudRegion", 0)
              );
        return 0;
    }
#else
    if (ioto->registered) {
        rInfo("register", "Device already registered");
        return 0;
    }
#endif
    if (!ioto->product || smatch(ioto->product, "PUT-YOUR-PRODUCT-ID-HERE")) {
        rError("register", "Cannot register device, missing \"product\" in config/device.json5");
        return R_ERR_BAD_ARGS;
    }
    if (!ioto->id || *ioto->id == '\0') {
        rError("register", "Cannot register device, missing device \"id\" in config/device.json5");
        return R_ERR_BAD_ARGS;

    } else if (smatch(ioto->id, "auto")) {
        ioto->id = cryptID(10);
        rInfo("register", "Generated device claim ID %s", ioto->id);

        jsonUnlock(ioto->config);
        jsonSet(ioto->config, 0, "device.id", ioto->id, JSON_STRING);
        jsonLock(ioto->config);

        if (!ioto->nosave) {
            path = rGetFilePath(IO_DEVICE_FILE);
            if (jsonSave(ioto->config, 0, "device", path, 0600, JSON_PRETTY | JSON_QUOTES) < 0) {
                rError("register", "Cannot save device registration to %s", path);
                return R_ERR_CANT_WRITE;
            }
        }
    }
    params = jsonAlloc(0);
    jsonBlend(params, 0, 0, ioto->config, 0, "device", 0);

#if SERVICES_CLOUD
    /*
        If the device.json5 "account" and "cloud" properties are set to the user's device manager
        account and cloud (Account Settings) then auto-claim.
     */
    if (ioto->account) {
        jsonSet(params, 0, "account", ioto->account, JSON_STRING);
    }
    if (ioto->cloud) {
        jsonSet(params, 0, "cloud", ioto->cloud, JSON_STRING);
    }
#endif
    jsonSetDate(params, 0, "created", 0);

    data = jsonToString(params, 0, 0, JSON_QUOTES | JSON_PRETTY);
    jsonFree(params);

    if (once++ == 0) {
        rInfo("register", "Device registering with %s\n%s", ioto->builder, data);
    }

    sfmtbuf(url, sizeof(url), "%s/device/register", ioto->builder);
    response = urlPostJson(url, data, -1, "Authorization: bearer %s\r\nContent-Type: application/json\r\n",
                           ioto->product);
    rFree(data);

    rc = parseRegisterResponse(response);
    jsonFree(response);
    return rc;
}

/*
    Parse register response
 */
static int parseRegisterResponse(Json *json)
{
    static int once = 0;
    char       *path;

    if (json == 0 || json->count < 2) {
        rError("register", "Cannot register device");
        return R_ERR_CANT_COMPLETE;
    }
    if (rEmitLog("debug", "register")) {
        rDebug("register", "Device register response: %s", jsonString(json));
    }
    /*
        Response will have 2 elements when registered but not claimed
     */
    if (json->count < 2) {
        return 0;
    } else if (json->count == 2) {
        if (ioto->provisionService) {
            //  Registered but not yet claimed
            if (once++ == 0) {
                rInfo("register", "Device not yet claimed for management via the cloud");
            }
        }
    }

    /*
        Update registration info in the provision.json5 and in-memory config.
     */
    jsonRemove(ioto->config, 0, "provision");
    jsonBlend(ioto->config, 0, "provision", json, 0, 0, 0);

    if (rEmitLog("debug", "register")) {
        rDebug("register", "Provisioning: %s", jsonString(json));
    }

    if (!ioto->nosave) {
        path = rGetFilePath(IO_PROVISION_FILE);
        if (jsonSave(ioto->config, 0, "provision", path, 0600, JSON_PRETTY | JSON_QUOTES) < 0) {
            rError("register", "Cannot save device provisioning to %s", path);
            rFree(path);
            return R_ERR_CANT_WRITE;
        }
        rFree(path);
    }

#if SERVICES_CLOUD
    rFree(ioto->api);
    ioto->api = jsonGetClone(ioto->config, 0, "provision.api", 0);
    rFree(ioto->apiToken);
    ioto->apiToken = jsonGetClone(ioto->config, 0, "provision.token", 0);
#endif

    ioto->registered = jsonGetBool(ioto->config, 0, "provision.registered", 0);
    rSignal("device:registered", 0);
    return 0;
}

#endif /* SERVICES_REGISTER */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/serialize.c ************/

/*
    serialize.c - Manufacture dynamic serialization

    This module gets a Unique device claim ID (10 character UDI).
    If the "services.serialize" is set to "auto", this module will dynamically create a random device ID.
    If set to "factory", ioSerialize() will call the factory serialization service defined via
    the "api.serialize" URL setting. The resultant deviceId is saved in the config/device.json5 file.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/*********************************** Locals ***********************************/
#ifdef SERVICES_SERIALIZE

#define SERIALIZE_TIMEOUT 30 * 1000

/*********************************** Forwards *********************************/

static bool getSerial(void);

/************************************* Code ***********************************/
/*
    Factory serialization. WARNING: this blocks ioto.
 */
PUBLIC void ioSerialize(void)
{
    while (!ioto->id) {
        getSerial();
        if (ioto->id) {
            break;
        }
        rSleep(2 * 1000);
    }
#if SERVICES_CLOUD
    if (ioto->id) {
        rInfo("app", "Device Claim ID: %s", ioto->id);
    }
#endif
}

/*
    Get a Unique device claim ID (UDI)
    This issues a request to the factory serialization service if the "services.serialize" is set to "factory"
    Otherwise is allocates a 10 character claim ID here.
 */
static bool getSerial(void)
{
    Url   *up;
    Json  *config, *result;
    cchar *id, *mode, *serialize;
    char  *def, *path, *saveId;
    int   did;

    id = saveId = 0;
    config = ioto->config;

    /*
        The allocation omde can be: factory, auto, none. Defaults to "auto"
     */
    mode = jsonGet(config, 0, "services.serialize", 0);
    did = jsonGetId(config, 0, "device");
    if (did < 0) {
        jsonSet(config, 0, "device", "", JSON_OBJECT);
        did = jsonGetId(config, 0, "device");
    }
    if (smatch(mode, "factory")) {
        //  Get serialize API endpoint
        if ((serialize = jsonGet(config, 0, "api.serialize", 0)) == 0) {
            rError("serialize", "Missing api.serialize endpoint in config.json");
            return 0;
        }
        up = urlAlloc(0);
        did = jsonGetId(config, 0, "device");

        if (sstarts(serialize, "http")) {
            //  Ask manufacturing controller for device ID
            def = jsonToString(config, did, 0, JSON_QUOTES);
            urlSetTimeout(up, SERIALIZE_TIMEOUT);
            if ((result = urlJson(up, "POST", serialize, def, -1, 0)) == 0) {
                rError("serialize", "Cannot fetch device ID from %s: %s", serialize, urlGetError(up));

            } else if ((id = jsonGet(result, 0, "id", 0)) == 0) {
                rError("serialize", "Cannot find device ID in response");
            }
            rFree(def);

#if ME_UNIX_LIKE
        } else {
            cchar *product;
            char  *command, *output;
            int   status;
            
            product = jsonGet(config, did, "product", 0);
            command = sfmt("serialize \"%s\"", product);
            output = rRun(command, &status);
            if (status != 0) {
                rError("serialize", "Cannot serialize %s", command);
                rFree(output);
            } else {
                id = saveId = output;
            }
            rFree(command);
#endif
        }
        urlFree(up);

    } else if (smatch(mode, "none")) {
        ;

    } else { /* Default "auto" */
        id = saveId = cryptID(10);
    }
    if (id) {
        jsonSet(config, did, "id", id, JSON_STRING);
        rFree(ioto->id);
        ioto->id = jsonGetClone(ioto->config, 0, "device.id", 0);
    }
    if (saveId) {
        path = rGetFilePath(IO_DEVICE_FILE);
        if (jsonSave(config, did, 0, path, 0600, JSON_PRETTY | JSON_QUOTES) < 0) {
            rError("serialize", "Cannot save serialization to %s", path);
            rFree(path);
            return R_ERR_CANT_WRITE;
        }
        rFree(path);
        rFree(saveId);
    }
    return id ? 1 : 0;
}

#else
void dummySerialize(void)
{
}
#endif /* SERVICES_SERIALIZE */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/setup.c ************/

/*
    setup.c - Setup for Ioto. Load configuration files.

    This code is intended to run from the main fiber and should not yield, block or create fibers.

    Users can use access most common fields via the "ioto->" object.
    Can also use jsonGet and jsonGet(ioto->config) to read config values.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/************************************ Forwards *********************************/

static int blendConditional(Json *json, cchar *property);
static int loadJson(Json *json, cchar *property, cchar *filename, bool optional);
static Json *makeTemplate(void);
static void enableServices(void);
static void reset(void);

/************************************* Code ***********************************/
/*
    Load config.json and provision.json into config
 */
PUBLIC int ioInitConfig(void)
{
    Json  *json;
    ssize stackSize;
    int   maxFibers;

    assert(rIsMain());

    if (!ioto) {
        ioAlloc();
    }
    if (ioLoadConfig() < 0) {
        return R_ERR_CANT_READ;
    }
    if (ioto->cmdReset) {
        reset();
    }
    /*
        Callback for users to modify config at runtime
     */
    json = ioto->config;
    ioConfig(json);

    stackSize = svalue(jsonGet(json, 0, "limits.stack", "0"));
    if (stackSize) {
        rSetFiberStack(stackSize);
    }
    maxFibers = (int) svalue(jsonGet(json, 0, "limits.fibers", "0"));
    if (maxFibers) {
        rSetFiberLimits(maxFibers);
    }

#if SERVICES_CLOUD
    if (ioto->cmdAccount) {
        jsonSet(json, 0, "device.account", ioto->cmdAccount, JSON_STRING);
    }
    if (ioto->cmdCloud) {
        jsonSet(json, 0, "device.cloud", ioto->cmdCloud, JSON_STRING);
    }
#endif
    if (ioto->cmdId) {
        jsonSet(json, 0, "device.id", ioto->cmdId, JSON_STRING);
    }
    if (ioto->cmdProduct) {
        jsonSet(json, 0, "device.product", ioto->cmdProduct, JSON_STRING);
    }
    if (ioto->cmdProfile) {
        rInfo("ioto", "Using environment IOTO_PROFILE %s", ioto->cmdProfile);
        jsonSet(json, 0, "profile", ioto->cmdProfile, JSON_STRING);
    }

#if SERVICES_CLOUD
    ioto->account = jsonGetClone(json, 0, "provision.accountId", 0);
    ioto->cloud = jsonGetClone(json, 0, "provision.cloud", 0);
    ioto->cloudType = jsonGetClone(json, 0, "provision.cloudType", 0);
    ioto->endpoint = jsonGetClone(json, 0, "provision.endpoint", 0);

    ioto->api = jsonGetClone(json, 0, "provision.api", 0);
    ioto->apiToken = jsonGetClone(json, 0, "provision.token", 0);
    ioto->provisioned = ioto->api && ioto->apiToken;

    if (!ioto->cloud) {
        ioto->cloud = jsonGetClone(json, 0, "device.cloud", 0);
    }
    if (!ioto->account) {
        ioto->account = jsonGetClone(json, 0, "device.account", 0);
    }
#endif

    ioto->builder = jsonGetClone(json, 0, "api.builder", "https://api.admin.embedthis.com/api");
    ioto->id = jsonGetClone(json, 0, "device.id", ioto->id);
    ioto->logDir = jsonGetClone(json, 0, "directories.log", ".");
    ioto->profile = jsonGetClone(json, 0, "profile", "dev");
    ioto->app = jsonGetClone(json, 0, "app", "empty");
    ioto->product = jsonGetClone(json, 0, "device.product", 0);
    ioto->registered = jsonGetBool(json, 0, "provision.registered", 0);
    ioto->version = jsonGetClone(json, 0, "version", "1.0.0");
    ioto->template = makeTemplate();

#if SERVICES_PROVISION
    cchar *id = jsonGet(json, 0, "provision.id", 0);
    if (id && !smatch(ioto->id, id)) {
        rError("setup", "Provisioning does not match configured device claim ID, reset provisioning");
        ioDeprovision();
    }
    if (!ioto->product || smatch(ioto->product, "")) {
        rError("setup", "Define your Builder \"product\" token in device.json5");
        return R_ERR_CANT_INITIALIZE;
    }
#endif

#if ME_COM_SSL
    /*
        Root CA to use for URL requests to external services
     */
    char *authority = (char*) jsonGet(json, 0, "tls.authority", 0);
    if (authority) {
        authority = rGetFilePath(authority);
        if (rAccessFile(authority, R_OK) == 0) {
            rSetSocketDefaultCerts(authority, NULL, NULL, NULL);
        } else {
            rError("setup", "Cannot access TLS root certificates \"%s\"", authority);
            rFree(authority);
            return R_ERR_CANT_INITIALIZE;
        }
        rFree(authority);
    }
#endif
    ioUpdateLog(0);
    rInfo("app", "Starting Ioto %s, with \"%s\" app %s, using \"%s\" profile",
          ME_VERSION, ioto->app, ioto->version, ioto->profile);
    enableServices();
    return 0;
}

PUBLIC void ioTermConfig(void)
{
    jsonFree(ioto->config);
    jsonFree(ioto->template);

    rFree(ioto->app);
    rFree(ioto->builder);
    rFree(ioto->id);
    rFree(ioto->logDir);
    rFree(ioto->profile);
    rFree(ioto->product);
    rFree(ioto->serializeService);
    rFree(ioto->version);

    ioto->app = 0;
    ioto->builder = 0;
    ioto->config = 0;
    ioto->id = 0;
    ioto->logDir = 0;
    ioto->profile = 0;
    ioto->product = 0;
    ioto->registered = 0;
    ioto->serializeService = 0;
    ioto->version = 0;
    ioto->template = 0;

#if SERVICES_CLOUD
    rFree(ioto->account);
    rFree(ioto->api);
    rFree(ioto->apiToken);
    rFree(ioto->cloud);
    rFree(ioto->cloudType);
    rFree(ioto->endpoint);
    rFree(ioto->awsAccess);
    rFree(ioto->awsSecret);
    rFree(ioto->awsToken);
    rFree(ioto->awsRegion);

    ioto->account = 0;
    ioto->api = 0;
    ioto->apiToken = 0;
    ioto->cloud = 0;
    ioto->cloudType = 0;
    ioto->endpoint = 0;
    ioto->awsAccess = 0;
    ioto->awsSecret = 0;
    ioto->awsToken = 0;
    ioto->awsRegion = 0;
#endif
}

/*
    Load the configuration from the config JSON files.
    This loads each JSON file and blends the results into the ioto->config JSON tree.
 */
PUBLIC int ioLoadConfig(void)
{
    Json *json;
    char *path;

    json = ioto->config = jsonAlloc(0);

    /*
        Command line --config, --state and --ioto can set the config/state and ioto.json paths.
     */
    rAddDirectory("state", ioto->cmdStateDir ? ioto->cmdStateDir : IO_STATE_DIR);

    if (ioto->cmdConfigDir) {
        rAddDirectory("config", ioto->cmdConfigDir);
    } else if (ioto->cmdIotoFile) {
        path = rDirname(sclone(ioto->cmdIotoFile));
        rAddDirectory("config", path);
        rFree(path);
    } else if (rAccessFile("ioto.json5", R_OK) == 0) {
        rAddDirectory("config", ".");
    } else {
        rAddDirectory("config", "@state/config");
    }
    rAddDirectory("db", "@state/db");
    rAddDirectory("certs", "@state/certs");
    rAddDirectory("site", "@state/site");

    if (loadJson(json, NULL, ioto->cmdIotoFile ? ioto->cmdIotoFile : IO_CONFIG_FILE, 0) < 0) {
        return R_ERR_CANT_READ;
    }
    if (json->count == 0) {
        rInfo("setup", "Cannot find valid \"%s\" config file", IO_CONFIG_FILE);
    }
    if (loadJson(json, NULL, IO_LOCAL_FILE, 1) < 0) {
        return R_ERR_CANT_READ;
    }
#if SERVICES_WEB
    if (loadJson(json, "web", IO_WEB_FILE, 1) < 0) {
        return R_ERR_CANT_READ;
    }
#endif
    if (loadJson(json, "device", IO_DEVICE_FILE, 1) < 0) {
        return R_ERR_CANT_READ;
    }
    if (!ioto->cmdReset) {
        if (loadJson(json, "provision", IO_PROVISION_FILE, 1) < 0) {
            return R_ERR_CANT_READ;
        }
    }
    //  Last chance local overrides
    if (loadJson(json, NULL, IO_LOCAL_FILE, 1) < 0) {
        return R_ERR_CANT_READ;
    }
    if (ioto->cmdStateDir) {
        //  Override state directory with command line
        jsonSet(json, 0, "directories.state", ioto->cmdStateDir, JSON_STRING);
    }
#if !ESP32 && !FREERTOS
    cchar *stateDir;
    //  Override state directory with ioto.json5
    if ((stateDir = jsonGet(json, 0, "directories.state", 0)) != 0) {
        rAddDirectory("state", stateDir);
    }
#endif
    //  Trace the resulting config
    if (rEmitLog("debug", "setup")) {
        rDebug("setup", "%s", jsonString(json));
    }
    return 0;
}

/*
    Convenience routine over jsonGet.
 */
PUBLIC cchar *ioGetConfig(cchar *key, cchar *defaultValue)
{
    return jsonGet(ioto->config, 0, key, defaultValue);
}

PUBLIC int ioGetConfigInt(cchar *key, int defaultValue)
{
    return jsonGetInt(ioto->config, 0, key, defaultValue);
}

/*
    Determine which services to enable
 */
static void enableServices(void)
{
    Json *config;
    int  sid;

    config = ioto->config;

    sid = jsonGetId(config, 0, "services");
    if (sid < 0) {
        ioto->webService = 1;

    } else {
        //  Defaults to true if no config.json
        ioto->webService = jsonGetBool(config, sid, "web", 1);
        ioto->dbService = jsonGetBool(config, sid, "database", 1);
        ioto->updateService = jsonGetBool(config, sid, "update", 0);
#if SERVICES_CLOUD
        ioto->logService = jsonGetBool(config, sid, "logs", 0);
        ioto->keyService = jsonGetBool(config, sid, "keys", 0);
        ioto->mqttService = jsonGetBool(config, sid, "mqtt", 0);
        ioto->provisionService = jsonGetBool(config, sid, "provision", 0);
        ioto->shadowService = jsonGetBool(config, sid, "shadow", 0);
        ioto->syncService = jsonGetBool(config, sid, "sync", 0);

        if (!ioto->provisionService && (ioto->keyService || ioto->mqttService)) {
            rError("setup", "Need provisioning service if key or mqtt service is required");
            ioto->provisionService = 1;
        }
        if ((ioto->provisionService || ioto->keyService || ioto->syncService) && !ioto->mqttService) {
            rError("setup", "Need MQTT service if provision, key or sync services are required");
            ioto->mqttService = 1;
        }
#endif

#ifdef SERVICES_SERIALIZE
        ioto->serializeService = jsonGetClone(config, sid, "serialize", ioto->provisionService ? "auto" : 0);
#endif
        ioto->testService = jsonGetBool(config, sid, "test", 0);

        /*
            NOTE: The Ioto license requires that if this code is removed or disabled, you must
            manually enter and maintain device volumes using Embedthis Builder (https://admin.embedthis.com).
         */
        ioto->registerService = jsonGetBool(config, sid, "register", ioto->provisionService ? 1 : 0);
    }
    rInfo("setup", "Enabling services: %s%s%s%s%s%s%s%s%s%s",
          ioto->dbService ? "db " : "",
          ioto->logService ? "log " : "",
          ioto->provisionService ? "provision " : "",
          ioto->registerService ? "register " : "",
          ioto->shadowService ? "shadow " : "",
          ioto->syncService ? "sync " : "",
          ioto->serializeService ? "serialize " : "",
          ioto->testService ? "test " : "",
          ioto->updateService ? "update " : "",
          ioto->webService ? "web" : ""
          );
}

/*
    Load a json "filename" from the "dir" and blend into the existing JSON tree at the given "property".
 */
static int loadJson(Json *json, cchar *property, cchar *filename, bool optional)
{
    Json *extra;
    char *path;

    path = rGetFilePath(filename);
    if (rAccessFile(path, F_OK) < 0) {
        if (!optional) {
            rError("setup", "Cannot find required file %s", path);
            rFree(path);
            return R_ERR_CANT_FIND;
        }
        rFree(path);
        return 0;
    }
    if ((extra = jsonParseFile(path, NULL, 0)) == 0) {
        rError("setup", "Cannot parse %s", path);
        rFree(path);
        return R_ERR_CANT_READ;
    }
    rInfo("setup", "Loading %s", path);

    if (jsonBlend(json, 0, property, extra, 0, 0, 0) < 0) {
        rError("setup", "Cannot blend %s", path);
        jsonFree(extra);
        rFree(path);
        return R_ERR_CANT_READ;
    }
    jsonFree(extra);
    if (blendConditional(json, property) < 0) {
        rFree(path);
        return R_ERR_CANT_READ;
    }
    rFree(path);
    return 0;
}

/*
    Blend conditional sections under "conditional[property]"
    Currently only supports "{conditional: { profile: { dev: {}, prod: {}}}}"
 */
static int blendConditional(Json *json, cchar *property)
{
    JsonNode *collection;
    cchar    *value;
    int      cid, nid, rootId, set;

    rootId = jsonGetId(json, 0, property);
    if (rootId < 0) {
        return 0;
    }
    cid = jsonGetId(json, rootId, "conditional");
    if (cid >= 0) {
        for (ITERATE_JSON_DYNAMIC(json, cid, collection, nid)) {
            //  Collection name: profile
            value = 0;
            if (smatch(collection->name, "profile")) {
                if ((value = ioto->cmdProfile) == 0) {
                    value = jsonGet(ioto->config, 0, "profile", "dev");
                }
            }
            if (!value) {
                value = jsonGet(json, 0, collection->name, 0);
            }
            if (value) {
                set = jsonGetId(json, jsonGetNodeId(json, collection), value);
                if (set >= 0) {
                    //  WARNING: property references are not stable over a blend
                    if (jsonBlend(json, 0, 0, json, set, 0, JSON_COMBINE) < 0) {
                        rError("setup", "Cannot blend %s", collection->name);
                        return R_ERR_CANT_COMPLETE;
                    }
                }
            }
        }
        jsonRemove(json, rootId, "conditional");
    }
    return 0;
}

/*
    Expand ${references} in the "str" using template variables in ioto->template
 */
PUBLIC char *ioExpand(cchar *str)
{
    return jsonTemplate(ioto->template, str);
}

/*
    Make a JSON collection of properties to be used with iotoExpand
 */
static Json *makeTemplate(void)
{
    Json *json;
    char hostname[ME_MAX_FNAME];

    json = jsonAlloc(0);
    if (gethostname(hostname, sizeof(hostname)) < 0) {
        scopy(hostname, sizeof(hostname), "localhost");
    }
    jsonSet(json, 0, "hostname", hostname, 0);
#if SERVICES_CLOUD
    jsonSet(json, 0, "id", ioto->id, 0);
    jsonSet(json, 0, "instance", ioto->instance ? ioto->instance : hostname, 0);
#endif
    return json;
}

/*
    Set a template variable in the ioto->template collection
 */
PUBLIC void ioSetTemplateVar(cchar *key, cchar *value)
{
    jsonSet(ioto->template, 0, key, value, 0);
}

static void removeFile(cchar *file)
{
    char *path;

    path = rGetFilePath(file);
    unlink(path);
    rFree(path);
}

/*
    Hardware reset (--reset)
 */
static void reset(void)
{
    char *dest, *path;

    rInfo("main", "Reset to factory defaults");

    removeFile(IO_PROVISION_FILE);
    removeFile(IO_SHADOW_FILE);
    removeFile(IO_CERTIFICATE);
    removeFile(IO_KEY);
    removeFile("@db/device.db.jnl");
    removeFile("@db/device.db.sync");

    path = rGetFilePath("@db/device.db.reset");
    if (rAccessFile(path, R_OK) == 0) {
        dest = rGetFilePath("@db/device.db");
        rCopyFile(path, dest, 0664);
        rFree(dest);
    } else {
        removeFile("@db/device.db");
    }
    rFree(path);
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/web.c ************/

/*
    web.c - Configure the embedded web server

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/*********************************** Forwards *********************************/
#if SERVICES_WEB

static int parseShow(cchar *arg);

/************************************* Code ***********************************/

PUBLIC int ioInitWeb(void)
{
    WebHost *webHost;
    cchar   *webShow;
    char    *errorMsg, *path;

    webInit();

    /*
        Parse a signatures.json file that is used to validate REST requests to the web server
     */
    path = rGetFilePath(IO_SIGNATURE_FILE);
    if (rAccessFile(path, R_OK) == 0) {
        if ((ioto->signatures = jsonParseFile(path, &errorMsg, 0)) == 0) {
            rError("web", "Cannot parse signatures file: %s", errorMsg);
        }
        ioto->strictSignatures = smatch(jsonGet(ioto->config, 0, "web.strict", 0), "true");
    }
    rFree(path);

    /*
        Rebase relative documents and upload directories under "state"
     */
    path = rGetFilePath(jsonGet(ioto->config, 0, "web.documents", "site"));
    jsonSet(ioto->config, 0, "web.documents", path, JSON_STRING);
    rFree(path);

    path = rGetFilePath(jsonGet(ioto->config, 0, "web.upload.dir", "tmp"));
    jsonSet(ioto->config, 0, "web.upload.dir", path, JSON_STRING);
    rFree(path);

    webShow = jsonGet(ioto->config, 0, "log.show", ioto->cmdWebShow);

    if ((webHost = webAllocHost(ioto->config, parseShow(webShow))) == 0) {
        return R_ERR_CANT_INITIALIZE;
    }
#if SERVICES_DATABASE
    {
        cchar *url;
        if ((url = jsonGet(ioto->config, 0, "web.auth.login", 0)) != 0) {
            webAddAction(webHost, url, webLoginUser, NULL);
        }
        if ((url = jsonGet(ioto->config, 0, "web.auth.logout", 0)) != 0) {
            webAddAction(webHost, url, webLogoutUser, NULL);
        }
    }
#endif
#if ESP32 || FREERTOS
    webSetHostDefaultIP(webHost, rGetIP());
#endif

    if (webStartHost(webHost) < 0) {
        webFreeHost(webHost);
        return R_ERR_CANT_OPEN;
    }
    ioto->webHost = webHost;
    return 0;
}

PUBLIC void ioTermWeb(void)
{
    if (ioto->signatures) {
        jsonFree(ioto->signatures);
        ioto->signatures = 0;
    }
    if (ioto->webHost) {
        webStopHost(ioto->webHost);
        webFreeHost(ioto->webHost);
    }
    ioto->webHost = 0;
    webTerm();
}

PUBLIC void ioRestartWeb(void)
{
    webStopHost(ioto->webHost);
    webStartHost(ioto->webHost);
}

/*
    Check a web request against the API signature for this controller.method.
 */
static void checkRequest(Web *web, JsonNode *signature)
{
    JsonNode *field, *schema, *var;
    cchar    *required, *role, *value;
    bool     hasWild;
    int      fid, sid, vid;

    hasWild = 0;
    for (ITERATE_JSON(ioto->signatures, signature, field, fid)) {
        required = jsonGet(ioto->signatures, fid, "required", 0);
        role = jsonGet(ioto->signatures, fid, "role", 0);

        if (smatch(field->name, "*")) {
            hasWild = 1;
        } else {
            if (role && !webCan(web, role)) {
                rDebug("web", "WARNING: removing %s for %s - Insufficient role: %s", field->name, web->url, role);
                webRemoveVar(web, field->name);
            } else {
                value = jsonGet(web->vars, 0, field->name, 0);
                if (required && !value && !jsonGet(ioto->signatures, fid, "default", 0)) {
                    webWriteResponse(web, 400, "Missing required request field '%s'", field->name);
                }
            }
        }
        schema = jsonGetNode(ioto->signatures, fid, "schema");
        if (schema) {
            checkRequest(web, schema);
        }
    }
    if (web->vars) {
        for (ITERATE_JSON(web->vars, 0, var, vid)) {
            sid = jsonGetId(ioto->signatures, jsonGetNodeId(ioto->signatures, signature), var->name);
            if (!hasWild) {
                if (sid < 0) {
                    rDebug("web", "WARNING: removing %s - not in signature for %s", var->name, web->url);
                    webRemoveVar(web, var->name);
                } else {
                    if (jsonGet(ioto->signatures, sid, "discard", 0) != 0) {
                        webRemoveVar(web, var->name);
                    }
                }
            }
        }
    }
}

/*
    Check the request signature for actions against a signatures.json file
 */
PUBLIC void webCheckRequest(Web *web, cchar *controller, cchar *action)
{
    JsonNode *signature;
    char     path[WEB_MAX_SIG];

    sfmtbuf(path, sizeof(path), "%s.%s", controller, action);

    if ((web->signature = jsonGetId(ioto->signatures, 0, path)) < 0) {
        if (ioto->strictSignatures) {
            webWriteResponse(web, 400, "Unsupported request");
        } else {
            rError("web", "Cannot find signature for %s", path);
        }
        return;
    }
    if ((signature = jsonGetNode(ioto->signatures, web->signature, "request")) != 0) {
        checkRequest(web, signature);
    } else {
        if (ioto->strictSignatures) {
            webWriteResponse(web, 400, "Unsupported request");
        } else {
            rError("web", "Cannot find signature for %s", path);
        }
    }
}

/*
    Parse the HTTP show command argument
 */
static int parseShow(cchar *arg)
{
    int show;

    show = 0;
    if (!arg) {
        return show;
    }
    if (schr(arg, 'H')) {
        show |= WEB_SHOW_REQ_HEADERS;
    }
    if (schr(arg, 'B')) {
        show |= WEB_SHOW_REQ_BODY;
    }
    if (schr(arg, 'h')) {
        show |= WEB_SHOW_RESP_HEADERS;
    }
    if (schr(arg, 'b')) {
        show |= WEB_SHOW_RESP_BODY;
    }
    return show;
}

#if SERVICES_DATABASE
/*
    Write a database item and check against the API signature
    Does not finalize
 */
PUBLIC ssize webWriteItem(Web *web, const DbItem *item)
{
    JsonNode *field, *signature;
    Json     *json;
    char     *buf;
    ssize    bytes;
    int      fid;

    if (!item) {
        return 0;
    }
    json = dbJson(item);
    if (web->signature >= 0) {
        if ((signature = jsonGetNode(ioto->signatures, web->signature, "response")) != 0) {
            //  OPT - could do dry run without clone and only clone if modification required
            //  OPT - or have test if modification will be required by this signature
            json = jsonClone(json, 0);
            for (ITERATE_JSON(ioto->signatures, signature, field, fid)) {
                if (jsonGet(ioto->signatures, fid, "discard", 0) != 0) {
                    jsonRemove(json, 0, field->name);
                }
            }
            buf = jsonToString(json, 0, 0, JSON_QUOTES);
            jsonFree(json);
            bytes = webWrite(web, buf, -1);
            rFree(buf);
            return bytes;
        }
    }
    buf = jsonToString(json, 0, 0, JSON_QUOTES);
    bytes = webWrite(web, buf, -1);
    rFree(buf);
    return bytes;
}

/*
    Write a database grid of items as a response. The items are validated against the API signature
    and the request is finalized.
 */
PUBLIC ssize webWriteItems(Web *web, RList *items)
{
    DbItem *item;
    ssize  index, rc, wrote;
    bool   prior;

    if (!items) {
        return 0;
    }
    rc = 0;
    prior = 0;
    webWrite(web, "[", 1);
    for (ITERATE_ITEMS(items, item, index)) {
        if (!item) {
            continue;
        }
        if (prior) {
            rc += webWrite(web, ",", -1);
        }
        if ((wrote = webWriteItem(web, item)) <= 0) {
            continue;
        }
        rc += wrote;
        prior = 1;
    }
    rc += webWrite(web, "]", 1);
    return rc;
}

/*
    Write a database item as a response. The item is validated against the API signature
    and the request is finalized.
 */
PUBLIC ssize webWriteItemResponse(Web *web, const DbItem *item)
{
    ssize rc;

    rc = webWriteItem(web, item);
    webFinalize(web);
    return rc;
}

/*
    Write a database grid of items as a response. The items are validated against the API signature
    and the request is finalized.
 */
PUBLIC ssize webWriteItemsResponse(Web *web, RList *items)
{
    ssize rc;

    rc = webWriteItems(web, items);
    webFinalize(web);
    return rc;
}

/*
    Default login action. This is designed for web page use and redirects as a response (i.e. not for SPAs).
 */
PUBLIC void webLoginUser(Web *web)
{
    CDbItem *user;
    cchar   *role, *username;

    username = webGetVar(web, "username", 0);

    user = dbFindOne(ioto->db, "User", DB_PROPS("username", username), NULL);
    if (!user) {
        webWriteResponse(web, 400, "Unknown user");
        return;
    }
    if (!cryptCheckPassword(webGetVar(web, "password", 0), dbField(user, "password"))) {
        rTrace("auth", "Password does not match");
        webWriteResponse(web, 401, "Password failed to authenticate");
        //  Security: throttle rate of attempts
        rSleep(500);
        return;

    }
    role = dbField(user, "role");
    if (!webLogin(web, username, role)) {
        webWriteResponse(web, 400, "Unknown user role");
    } else {
        webRedirect(web, 302, "/");
    }
}

PUBLIC void webLogoutUser(Web *web)
{
    webLogout(web);
    webRedirect(web, 302, "/");
}
#endif /* SERVICES_DATABASE */

#else
void dummyWeb(void)
{
}
#endif /* SERVICES_WEB */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/cloud/cloud.c ************/

/*
    cloud.c - Cloud services. Includes cloudwatch logs, log capture, shadow state and database sync.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.

    Docs re Sigv4 signing
        https://docs.aws.amazon.com/general/latest/gr/sigv4-signed-request-examples.html
        https://docs.aws.amazon.com/AmazonCloudWatchLogs/latest/APIReference/CommonParameters.html
        https://docs.aws.amazon.com/AmazonCloudWatchLogs/latest/APIReference/API_PutLogEvents.html
        https://github.com/fromkeith/awsgo/blob/master/cloudwatch/putLogEvents.go
 */

/********************************** Includes **********************************/



#if SERVICES_CLOUD
/*********************************** Forwards *********************************/

static void onCloudConnect(void);

/************************************* Code ***********************************/

PUBLIC int ioInitCloud(void)
{
#if SERVICES_KEYS
    if (ioto->keyService && smatch(ioto->cloudType, "dedicated")) {
        ioGetKeys();
    }
#endif /* SERVICE_KEYS */
#if SERVICES_SHADOW
    if (ioto->shadowService && (ioInitShadow() < 0)) {
        return R_ERR_CANT_INITIALIZE;
    }
#endif
#if SERVICES_LOGS
    if (ioto->logService && (ioInitLogs() < 0)) {
        return R_ERR_CANT_INITIALIZE;
    }
#endif
    ioOnConnect((RWatchProc) onCloudConnect, 0, 1);
    rSignal("cloud:ready", 0);
    return 0;
}

PUBLIC void ioTermCloud(void)
{
#if SERVICES_LOGS
    if (ioto->logService) {
        ioTermLogs();
    }
#endif
#if SERVICES_SYNC
    if (ioto->syncService) {
        ioTermSync();
    }
#endif
#if SERVICES_SHADOW
    if (ioto->shadowService) {
        ioTermShadow();
    }
#endif
#if SERVICES_MQTT
    ioTermMqtt();
#endif
    ioto->instance = 0;
}

static void onCloudConnect()
{
#if SERVICES_PROVISION
    mqttSubscribe(ioto->mqtt, ioRelease, 1, MQTT_WAIT_NONE, "ioto/device/%s/provision/+", ioto->id);
#endif
#if SERVICES_SYNC
    ioConnectSync();
#endif
}

#else
void dummyCloud(void)
{
}
#endif /* SERVICES_CLOUD */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/cloud/cloudwatch.c ************/

/*
    cloudwatch.c - Cloud based logging to CloudWatch logs

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/*********************************** Includes *********************************/



#if SERVICES_CLOUD
/*********************************** Defines **********************************/

#define DEFAULT_BUF_SIZE   1024
#define DEFAULT_LINGER     (5 * TPS)
#define MAX_LINGER         (3600 * TPS)
#define AWS_EVENT_OVERHEAD 26

/*
    AWS max is 10K
 */
#define MAX_AWS_EVENTS     1000

/*
    AWS max is 1MB
 */
#define MAX_AWS_BUF_SIZE   (256 * 1024)
#define MAX_BUFFERS        4

static RBuf *logBuf;

/*********************************** Forwards *********************************/

static void bufferTimeout(IotoLog *log);
static int commitMessage(IotoLog *log);
static void flushBuf(IotoLog *log);
static int finalizeBuf(IotoLog *log);
static int getLogGroup(IotoLog *log);
static int getLogStream(IotoLog *log);
static void logMessageLine(IotoLog *log, cchar *value);
static void logHandler(cchar *type, cchar *source, cchar *msg);
static int logMessageEnd(IotoLog *log);
static int logMessageStart(IotoLog *log, Time time);
static void prepareBuf(IotoLog *log);
static void queueBuf(IotoLog *log);
static void serviceQueue(IotoLog *log, int count);
static void startTimeout(IotoLog *log);
static void stopTimeout(IotoLog *log);

/************************************* Code ***********************************/

PUBLIC IotoLog *ioAllocLog(cchar *path, cchar *region, int create, cchar *group, cchar *stream,
                           int maxEvents, int size, Ticks linger)
{
    IotoLog *log;

    assert(group && *group);

    log = rAllocType(IotoLog);
    if (maxEvents <= 0 || maxEvents > MAX_AWS_EVENTS) {
        maxEvents = MAX_AWS_EVENTS;
    }
    if (size <= 0 || size > MAX_AWS_BUF_SIZE) {
        size = MAX_AWS_BUF_SIZE;
    }
    if (linger < 0) {
        linger = DEFAULT_LINGER;
    }
    if (linger > MAX_LINGER) {
        linger = MAX_LINGER;
    }
    log->path = sclone(path);
    log->region = sclone(region);
    log->group = sclone(group);
    log->stream = sclone(stream);
    log->buffers = rAllocList(0, 0);
    log->create = create;

    // Set HIW to 80% to leave room to finalize buffer before sending
    log->eventsHiw = maxEvents / 100 * 80;
    log->maxEvents = maxEvents;
    log->max = size - 3;
    log->hiw = size / 100 * 80;
    log->linger = linger;

    prepareBuf(log);

    if (getLogGroup(log) < 0) {
        ioFreeLog(log);
        return 0;
    }
    return log;
}

PUBLIC void ioFreeLog(IotoLog *log)
{
    RBuf *buf;
    int  i;

    if (!log) {
        return;
    }
    for (ITERATE_ITEMS(log->buffers, buf, i)) {
        rFreeBuf(buf);
    }
    rFreeList(log->buffers);
    rFreeBuf(log->buf);
    rStopEvent(log->event);
    rFree(log->path);
    rFree(log->region);
    rFree(log->group);
    rFree(log->stream);
    rFree(log->sequence);
    rFree(log);
}

PUBLIC int ioEnableCloudLog(void)
{
    cchar *group;
    char  *stream;

    logBuf = rAllocBuf(256);
    group = jsonGet(ioto->config, 0, "log.group", IO_LOG_GROUP);
    stream = ioExpand(jsonGet(ioto->config, 0, "log.stream", IO_LOG_STREAM));

    ioto->log = ioAllocLog("ioto", ioto->awsRegion, 1, group, stream, -1, -1, -1);
    rSetLogHandler(logHandler);
    rFree(stream);
    return 0;
}

static void logHandler(cchar *type, cchar *source, cchar *msg)
{
    cchar *str;

    if (rEmitLog(type, source)) {
        rFormatLog(logBuf, type, source, msg);
        str = rBufToString(logBuf);
        if (ioto->log) {
            ioLogMessage(ioto->log, 0, msg);
        } else {
            write(rGetLogFile(), str, (int) rGetBufLength(logBuf));
        }
    }
}

/*
    Log a single message
 */
PUBLIC int ioLogMessage(IotoLog *log, Time time, cchar *msg)
{
    if (!log) {
        return R_ERR_BAD_STATE;
    }
    if (logMessageStart(log, time) < 0) {
        return R_ERR_TIMEOUT;
    }
    logMessageLine(log, msg);
    return logMessageEnd(log);
}

static int logMessageStart(IotoLog *log, Time time)
{
    Time now;

    //  Do not use asserts here
    if (!log || !log->buf) {
        return R_ERR_BAD_STATE;
    }
    if (!ioto->awsAccess) {
        rError("log", "AWS keys not configured for CloudWatch logging");
        return R_ERR_NOT_READY;
    }
    now = rGetTime();
    if (time == 0) {
        time = now;

    } else if (time > (now + 2 * 3600 * TPS) || time < (now - (14 * 86400 * TPS) + 3600 * TPS)) {
        // Ignore events more than 2 hrs in the future or almost 14 days old
        rTrace("log", "Ignore out of range event %s", rFormatLocalTime(NULL, time));
        return R_ERR_TIMEOUT;
    }
    if (!log->bufStarted) {
        log->bufStarted = time;

#if UNUSED
    } else if ((time - log->bufStarted) >= (23 * 3600 * TPS)) {
        //  Message is more than 23 hours after first message in buffer. AWS won't accept a span of > 24 hours.
        flushBuf(log);
#endif
    }
    rPutToBuf(log->buf, "{\"timestamp\":%lld,\"message\":", time);
    return 0;
}

/*
    Add a message line to the buffer
 */
static void logMessageLine(IotoLog *log, cchar *value)
{
    //  Do not use asserts here
    if (!log || !log->buf) {
        return;
    }
    jsonToBuf(log->buf, value, JSON_STRICT);
}

static int logMessageEnd(IotoLog *log)
{
    //  Do not use asserts here
    if (!log || !log->buf) {
        return R_ERR_BAD_STATE;
    }
    log->events++;
    rPutStringToBuf(log->buf, "},");
    return commitMessage(log);
}

/*
    Commit a message to AWS
 */
static int commitMessage(IotoLog *log)
{
    if (!log || !log->buf) {
        return R_ERR_BAD_STATE;
    }
    if (log->events >= log->eventsHiw || rGetBufLength(log->buf) >= log->hiw) {
        flushBuf(log);
        return 0;
    }
    startTimeout(log);
    return 0;
}

static void startTimeout(IotoLog *log)
{
    assert(log);

    if (!log->event) {
        log->event = rStartEvent((REventProc) bufferTimeout, log, log->linger);
    }
}

static void stopTimeout(IotoLog *log)
{
    if (log->event) {
        rStopEvent(log->event);
        log->event = 0;
    }
}

static void bufferTimeout(IotoLog *log)
{
    assert(log);

    if (log->event) {
        log->event = 0;
        flushBuf(log);
    }
}

static void flushBuf(IotoLog *log)
{
    if (!log->sending) {
        stopTimeout(log);
        finalizeBuf(log);
        queueBuf(log);
    }
}

static void queueBuf(IotoLog *log)
{
    RBuf *buf;

    //  Must immediately create a new buffer to capture any messages from here
    buf = log->buf;
    log->buf = 0;
    prepareBuf(log);

    if (rGetListLength(log->buffers) >= MAX_BUFFERS) {
        rDebug("log", "Discarding buffer due to queue overflow %d/%d", rGetListLength(log->buffers), MAX_BUFFERS);
        rFreeBuf(buf);
    } else {
        rPushItem(log->buffers, buf);
        serviceQueue(log, 0);
    }
}

static void serviceQueue(IotoLog *log, int count)
{
    RBuf  *buf;
    Url   *up;
    Json  *json;
    cchar *data;
    ssize len;
    int   status;

    assert(log);

    if (log->sending || ((buf = rPopItem(log->buffers)) == 0)) {
        return;
    }
    if (++count > 10) {
        return;
    }
    log->sending = buf;

    data = rBufToString(buf);
    len = rGetBufLength(buf);
    up = urlAlloc(0);

    // print("Sending log data for %s to cloudwatch %s %s/%s", log->path, log->region, log->group, log->stream);

    status = aws(up, ioto->awsRegion, "logs", "Logs_20140328.PutLogEvents", data, len, 0);

    if (status != URL_CODE_OK) {
        rError("log", "AWS request error, status code %d, response %s", up->status, urlGetResponse(up));
        //  Try to repair
        if (up->status == URL_CODE_BAD_REQUEST && scontains("Bad sequence", up->rx->start)) {
            getLogGroup(log);
        }
    } else {
        if ((json = urlGetJsonResponse(up)) == 0) {
            rError("log", "Cannot parse AWS response for log message: %s\n", urlGetResponse(up));
        } else {
            rFree(log->sequence);
            log->sequence = jsonGetClone(json, 0, "nextSequenceToken", 0);
            jsonFree(json);
        }
    }
    urlFree(up);

    rFreeBuf(log->sending);
    log->sending = 0;
    serviceQueue(log, count);
}

static void prepareBuf(IotoLog *log)
{
    assert(log);

    if (!log->buf) {
        log->buf = rAllocBuf(DEFAULT_BUF_SIZE);
    } else {
        rFlushBuf(log->buf);
    }
    log->events = 0;
    log->bufStarted = 0;
    rPutStringToBuf(log->buf, "{\"logEvents\":[");
}

static int finalizeBuf(IotoLog *log)
{
    RBuf *buf;

    assert(!log->sending);

    buf = log->buf;

    // Erase trailing comma after last event (JSON Ugh!)
    rAdjustBufEnd(buf, -1);

    if (log->sequence && *log->sequence) {
        rPutToBuf(log->buf, "],\"logGroupName\":\"%s\",\"logStreamName\":\"%s\",\"sequenceToken\":\"%s\"}",
                  log->group, log->stream, log->sequence);
    } else {
        rPutToBuf(log->buf, "],\"logGroupName\":\"%s\",\"logStreamName\":\"%s\"}", log->group, log->stream);
    }
    return 0;
}

static int createLogGroup(IotoLog *log)
{
    Url  *up;
    char *data;
    int  status;

    assert(log);

    up = urlAlloc(0);
    data = sfmt("{\"logGroupName\":\"%s\"}", log->group);

    status = aws(up, log->region, "logs", "Logs_20140328.CreateLogGroup", data, -1, NULL);

    rFree(data);
    urlFree(up);

    if (status != URL_CODE_OK) {
        rError("log", "Cannot create group %s, %s", log->group, urlGetResponse(log->up));
        return R_ERR_CANT_CREATE;
    }
    return 0;
}

static int describeLogGroup(IotoLog *log)
{
    Url      *up;
    Json     *json;
    JsonNode *groups, *child;
    cchar    *name, *nextToken;
    char     *data;
    int      id, sid, status;

    assert(log);

    up = urlAlloc(0);
    json = 0;
    nextToken = 0;
    do {
        if (nextToken) {
            data = sfmt("{\"logGroupNamePrefix\":\"%s\",\"nextToken\":\"%s\"}", log->group, nextToken);
        } else {
            data = sfmt("{\"logGroupNamePrefix\":\"%s\"}", log->group);
        }
        status = aws(up, log->region, "logs", "Logs_20140328.DescribeLogGroups", data, -1, 0);
        rFree(data);

        if (status != URL_CODE_OK) {
            rError("log", "Cannot describe log groups");
            urlFree(up);
            return R_ERR_BAD_STATE;
        }
        json = urlGetJsonResponse(up);

        if ((sid = jsonGetId(json, 0, "logGroups")) <= 0) {
            rError("log", "Cannot find logSGroups in response");
            jsonFree(json);
            urlFree(up);
            return R_ERR_BAD_FORMAT;
        }
        groups = jsonGetNode(json, sid, 0);
        for (ITERATE_JSON(json, groups, child, id)) {
            name = jsonGet(json, id, "logGroupName", 0);
            if (smatch(name, log->group)) {
                urlFree(up);
                jsonFree(json);
                return 0;
            }
        }
        nextToken = jsonGet(json, 0, "nextToken", 0);
        jsonFree(json);
    } while (nextToken);

    urlFree(up);
    return R_ERR_CANT_FIND;
}

static int getLogGroup(IotoLog *log)
{
    int rc;

    if ((rc = describeLogGroup(log)) < 0) {
        if (rc == R_ERR_CANT_FIND) {
            if (log->create) {
                if (createLogGroup(log) < 0) {
                    return R_ERR_CANT_CREATE;
                }
            } else {
                rError("log", "Cannot find log group %s", log->group);
                return R_ERR_CANT_FIND;
            }
        } else {
            return R_ERR_BAD_STATE;
        }
    }
    return getLogStream(log);
}

static int createLogStream(IotoLog *log)
{
    Url  *up;
    char *data;
    int  status;

    assert(log);

    up = urlAlloc(0);
    data = sfmt("{\"logGroupName\":\"%s\",\"logStreamName\":\"%s\"}", log->group, log->stream);

    status = aws(up, log->region, "logs", "Logs_20140328.CreateLogStream", data, -1, 0);
    rFree(data);

    if (status != URL_CODE_OK) {
        rError("log", "Cannot create stream %s in group %s, %s", log->stream, log->group, urlGetResponse(log->up));
        urlFree(up);
        return R_ERR_CANT_CREATE;
    }
    urlFree(up);
    return 0;
}

static int describeStream(IotoLog *log)
{
    Url      *up;
    Json     *json;
    JsonNode *streams, *child;
    cchar    *name, *nextToken;
    char     *data;
    int      id, sid, status;

    assert(log);

    up = urlAlloc(0);
    nextToken = 0;
    do {
        if (nextToken) {
            data = sfmt("{\"logGroupName\":\"%s\",\"logStreamNamePrefix\":\"%s\",\"nextToken\":\"%s\"}",
                        log->group, log->stream, nextToken);
        } else {
            data = sfmt("{\"logGroupName\":\"%s\",\"logStreamNamePrefix\":\"%s\"}", log->group, log->stream);
        }
        status = aws(up, log->region, "logs", "Logs_20140328.DescribeLogStreams", data, -1, 0);
        rFree(data);

        if (status != URL_CODE_OK) {
            rError("log", "Cannot describe log streams for group %s", log->group);
            urlFree(up);
            return R_ERR_BAD_STATE;
        }
        json = urlGetJsonResponse(up);
        if ((sid = jsonGetId(json, 0, "logStreams")) <= 0) {
            rError("log", "Cannot find logStreams in response");
            jsonFree(json);
            urlFree(up);
            return R_ERR_BAD_FORMAT;
        }
        streams = jsonGetNode(json, sid, 0);

        for (ITERATE_JSON(json, streams, child, id)) {
            name = jsonGet(json, id, "logStreamName", 0);
            if (smatch(name, log->stream)) {
                rFree(log->sequence);
                log->sequence = jsonGetClone(json, id, "uploadSequenceToken", 0);
                jsonFree(json);
                urlFree(up);
                return 0;
            }
        }
        nextToken = jsonGet(json, 0, "nextToken", 0);
        jsonFree(json);
    } while (nextToken);

    urlFree(up);
    return R_ERR_CANT_FIND;
}

/*
    Describe the stream and get the sequence number for submitting events
 */
static int getLogStream(IotoLog *log)
{
    int rc;

    assert(log);

    rFree(log->sequence);
    log->sequence = 0;

    if ((rc = describeStream(log)) < 0) {
        if (rc != R_ERR_BAD_STATE && createLogStream(log) < 0) {
            return R_ERR_CANT_CREATE;
        }
    }
    return 0;
}

#else
void dummyCloudLog(void)
{
}
#endif /* SERVICES_CLOUD */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/cloud/helpers.c ************/

/*
    helpers.c - AWS API helper routines that support SigV4 signed HTTP REST requests.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.

    Docs re Sigv4 signing
        https://docs.aws.amazon.com/general/latest/gr/sigv4-create-canonical-request.html
        https://docs.aws.amazon.com/general/latest/gr/sigv4-signed-request-examples.html
        https://docs.aws.amazon.com/AmazonCloudWatchLogs/latest/APIReference/CommonParameters.html
        https://docs.aws.amazon.com/AmazonCloudWatchLogs/latest/APIReference/API_PutLogEvents.html
        https://github.com/fromkeith/awsgo/blob/master/cloudwatch/putLogEvents.go
 */

/********************************** Includes **********************************/



#if SERVICES_CLOUD

#if ME_COM_MBEDTLS
    #include "mbedtls/mbedtls_config.h"
    #include "mbedtls/ssl.h"
    #include "mbedtls/ssl_cache.h"
    #include "mbedtls/ssl_ticket.h"
    #include "mbedtls/ctr_drbg.h"
    #include "mbedtls/net_sockets.h"
    #include "psa/crypto.h"
    #include "mbedtls/debug.h"
    #include "mbedtls/error.h"
    #include "mbedtls/check_config.h"
#endif

#if ME_COM_OPENSSL
    #include <openssl/evp.h>
    #include <openssl/hmac.h>
#endif

/*********************************** Forwards *********************************/

static char *hashToString(char hash[CRYPT_SHA256_SIZE]);
static char *getHeader(cchar *headers, cchar *header, cchar *defaultValue);

/************************************* Code ***********************************/

static void sign(char hash[CRYPT_SHA256_SIZE], cchar *key, ssize keyLen, cchar *payload, ssize payloadLen)
{
    if (payloadLen < 0) {
        payloadLen = slen(payload);
    }
    if (keyLen < 0) {
        keyLen = slen(key);
    }
#if ME_COM_MBEDTLS
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t    md_type = MBEDTLS_MD_SHA256;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts(&ctx, (cuchar*) key, keyLen);
    mbedtls_md_hmac_update(&ctx, (cuchar*) payload, payloadLen);
    mbedtls_md_hmac_finish(&ctx, (uchar*) hash);
    mbedtls_md_free(&ctx);

#elif ME_COM_OPENSSL
    uint len;
    HMAC(EVP_sha256(), (uchar*) key, (int) keyLen, (uchar*) payload, (int) payloadLen, (uchar*) hash, &len);
#endif
}

static void genKey(char result[CRYPT_SHA256_SIZE], cchar *secret, cchar *date, cchar *region, cchar *service)
{
    char cacheKey[256], kDate[CRYPT_SHA256_SIZE], kRegion[CRYPT_SHA256_SIZE], kService[CRYPT_SHA256_SIZE];
    char *asecret;

    sfmtbuf(cacheKey, sizeof(cacheKey), "%s-%s-%s-%s", secret, date, region, service);
#if FUTURE
    if ((key = rLookupName(signHash, cacheKey)) != 0) {
        memcpy(result, key, CRYPT_SHA256_SIZE);
        return;
    }
#endif
    asecret = sfmt("AWS4%s", secret);
    sign(kDate, asecret, -1, date, -1);
    sign(kRegion, (cchar*) kDate, CRYPT_SHA256_SIZE, region, -1);
    sign(kService, (cchar*) kRegion, CRYPT_SHA256_SIZE, service, -1);
    sign(result, (cchar*) kService, CRYPT_SHA256_SIZE, "aws4_request", -1);
    rFree(asecret);

#if FUTURE
    key = rAlloc(CRYPT_SHA256_SIZE);
    memcpy(key, result, CRYPT_SHA256_SIZE);
    rAddName(signHash, cacheKey, key, R_TEMPORAL_NAME | R_DYNAMIC_VALUE);
#endif
}

static void getHash(cchar *buf, ssize buflen, char hash[CRYPT_SHA256_SIZE])
{
    if (buflen < 0) {
        buflen = slen(buf);
    }
    cryptGetSha256Block((cuchar*) buf, buflen, (uchar*) hash);
}

static char *hashToString(char hash[CRYPT_SHA256_SIZE])
{
    return cryptSha256HashToString((uchar*) hash);
}

PUBLIC char *awsSign(cchar *region, cchar *service, cchar *target, cchar *method, cchar *path, cchar *query,
                     cchar *body, ssize bodyLen, cchar *headersFmt, ...)
{
    va_list args;
    Ticks   now;
    RBuf    *buf;
    cchar   *algorithm;
    char    *canonicalHeaders, *finalHeaders, *signedHeaders;
    char    *authorization, *canonicalRequest, *contentType, *date;
    char    *headers, *host, *isoDate, *scope, *time, *toSign;
    char    *hash, *payloadHash, *signature;
    char    shabuf[CRYPT_SHA256_SIZE], key[CRYPT_SHA256_SIZE];

    if (!service || !region) {
        rError("cloud.aws", "Missing service or region");
        return 0;
    }
    if (!ioto->awsToken || !ioto->awsAccess) {
        rError("cloud.aws", "AWS access keys not defined");
        return 0;
    }
    va_start(args, headersFmt);
    headers = headersFmt ? sfmtv(headersFmt, args) : 0;
    va_end(args);

    if (body && bodyLen < 0) {
        bodyLen = slen(body);
    }
    if (!query) {
        query = "";
    }
    if (!target) {
        target = getHeader(headers, "x-amz-target", 0);
    }
    host = getHeader(headers, "Host", 0);
    if (!host) {
        host = sfmt("%s.%s.amazonaws.com", service, region);
    }
    if (smatch(service, "s3")) {
        target = 0;
        contentType = getHeader(headers, "content-type", "application/octet-stream");
    } else {
        contentType = getHeader(headers, "content-type", "application/x-amz-json-1.1");
    }

    /*
        Get date and time in required formats
     */
    now = rGetTime();
    time = rFormatUniversalTime("%Y%m%dT%H%M%SZ", now);
    date = rFormatUniversalTime("%Y%m%d", now);
    isoDate = rFormatUniversalTime("%a, %d %b %Y %T GMT", now);

    /*
        Get body hash
     */
    memset(shabuf, 0, sizeof(shabuf));
    if (body) {
        getHash(body, bodyLen, shabuf);
    } else {
        getHash("", 0, shabuf);
    }
    payloadHash = hashToString(shabuf);

    /*
        Create a canonical request to sign. Does not include all headers. Lower case, no spaces, sorted.
        Headers must be lowercase, no spaces, and be in alphabetic order
     */
    buf = rAllocBuf(0);
    rPutToBuf(buf,
              "content-type:%s\n"
              "host:%s\n",
              contentType, host);
    rPutToBuf(buf, "x-amz-date:%s\n", time);

    //  S3 requires x-amz-content-sha256 - hash of payload -- must be before x-amz-date
    if (ioto->awsToken) {
        rPutToBuf(buf, "x-amz-security-token:%s\n", ioto->awsToken);
    }
    if (target) {
        rPutToBuf(buf, "x-amz-target:%s\n", target);
    }
    canonicalHeaders = sclone(rBufToString(buf));

    /*
        Create headers to sign
     */
    rFlushBuf(buf);

    //  S3 sha must be before amz-date
    rPutToBuf(buf, "content-type;host;x-amz-date");
    if (ioto->awsToken) {
        rPutStringToBuf(buf, ";x-amz-security-token");
    }
    if (target) {
        rPutStringToBuf(buf, ";x-amz-target");
    }
    signedHeaders = sclone(rBufToString(buf));

    canonicalRequest =
        sfmt("%s\n/%s\n%s\n%s\n%s\n%s", method, path, query, canonicalHeaders, signedHeaders, payloadHash);
    rDebug("aws", "Canonical Headers\n%s\n", canonicalHeaders);
    rDebug("aws", "Canonical Request\n%s\n\n", canonicalRequest);

    getHash(canonicalRequest, -1, shabuf);
    hash = hashToString(shabuf);

    algorithm = "AWS4-HMAC-SHA256";
    scope = sfmt("%s/%s/%s/aws4_request", date, region, service);

    toSign = sfmt("%s\n%s\n%s\n%s", algorithm, time, scope, hash);
    rDebug("aws", "ToSign\n%s\n", toSign);

    genKey(key, ioto->awsSecret, date, region, service);

    sign(shabuf, key, CRYPT_SHA256_SIZE, toSign, -1);
    signature = hashToString(shabuf);

    authorization = sfmt("%s Credential=%s/%s, SignedHeaders=%s, Signature=%s",
                         algorithm, ioto->awsAccess, scope, signedHeaders, signature);
    rFlushBuf(buf);
    rPutToBuf(buf,
              "Authorization: %s\r\n"
              "Date: %s\r\n"
              "X-Amz-Content-sha256: %s\r\n"
              "X-Amz-Date: %s\r\n",
              authorization, isoDate, payloadHash, time);

    if (ioto->awsToken) {
        rPutToBuf(buf, "X-Amz-Security-Token: %s\r\n", ioto->awsToken);
    }
    if (target && *target) {
        rPutToBuf(buf, "X-Amz-Target: %s\r\n", target);
    }
    if (!getHeader(headers, "content-type", 0)) {
        rPutToBuf(buf, "Content-Type: %s\r\n", contentType);
    }
    if (headers) {
        rPutStringToBuf(buf, headers);
    }
    finalHeaders = sclone(rBufToString(buf));

    rFree(authorization);
    rFreeBuf(buf);
    rFree(canonicalHeaders);
    rFree(canonicalRequest);
    rFree(contentType);
    rFree(date);
    rFree(hash);
    rFree(host);
    rFree(isoDate);
    rFree(payloadHash);
    rFree(scope);
    rFree(signature);
    rFree(signedHeaders);
    rFree(time);
    rFree(toSign);
    rFree(headers);

    return finalHeaders;
}

/*
    Convenience routine to issue an AWS API request
 */
PUBLIC int aws(Url *up, cchar *region, cchar *service, cchar *target, cchar *body, ssize bodyLen, cchar *headersFmt,
               ...)
{
    va_list args;
    char    *headers, *signedHeaders, *url;
    int     status;

    if (!ioto->awsToken || !ioto->awsAccess) {
        rError("cloud.aws", "AWS access keys not defined");
        return R_ERR_BAD_STATE;
    }
    va_start(args, headersFmt);
    headers = headersFmt ? sfmtv(headersFmt, args) : 0;
    va_end(args);

    signedHeaders = awsSign(region, service, target, "POST", "", NULL, body, bodyLen, headers);

    urlSetTimeout(up, svalue(jsonGet(ioto->config, 0, "timeouts.aws", "60 secs")) * TPS);
    url = sfmt("https://%s.%s.amazonaws.com/", service, region);
    status = urlFetch(up, "POST", url, body, bodyLen, signedHeaders);

    if (status != 200) {
        rError("aws", "AWS request failed: %s, status: %d, error: %s", url, status, urlGetResponse(up));
    }
    rFree(url);
    rFree(signedHeaders);
    rFree(headers);
    return status;
}

/*
    Simple header extraction. Must be no spaces after ":"
    Caller must free result.
 */
static char *getHeader(cchar *headers, cchar *header, cchar *defaultValue)
{
    cchar *end, *start;

    if (headers && header) {
        if (sncaselesscontains(headers, header, -1) != 0) {
            if ((start = schr(headers, ':')) != 0) {
                while (*start++ == ' ') start++;
                if ((end = schr(start, '\r')) != 0) {
                    return snclone(start, end - start);
                }
            }
        }
    }
    if (defaultValue) {
        return sclone(defaultValue);
    }
    return 0;
}

/*
    https://docs.aws.amazon.com/AmazonS3/latest/API/API_PutObject.html
    https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-authenticating-requests.html

    If key is null, the basename of the file is used
 */
PUBLIC int awsPutFileToS3(cchar *region, cchar *bucket, cchar *key, cchar *file)
{
    char  *data;
    ssize length;
    int   rc;

    if (!ioto->awsToken || !ioto->awsAccess) {
        rError("cloud.aws", "AWS access keys not defined");
        return R_ERR_BAD_STATE;
    }
    if (!key) {
        key = rBasename(file);
    }
    data = rReadFile(file, &length);
    rc = awsPutToS3(region, bucket, key, data, length);
    rFree(data);
    return rc;
}

PUBLIC int awsPutToS3(cchar *region, cchar *bucket, cchar *key, cchar *data, ssize dataLen)
{
    Url   *up;
    cchar *error;
    char  *path, *headers, *host, *url;
    int   status;

    if (!ioto->awsToken || !ioto->awsAccess) {
        rError("cloud.aws", "AWS access keys not defined");
        return R_ERR_BAD_STATE;
    }
    if (dataLen < 0) {
        dataLen = slen(data);
    }
    up = urlAlloc(0);

    if (schr(bucket, '.')) {
        /*
            Path style is deprecated (almost) by AWS but preserved because virtual style does not (yet) work with
            buckets containing dots.
         */
        host = sfmt("s3.%s.amazonaws.com", region);
        path = sfmt("%s/%s", bucket, key);
        headers = awsSign(region, "s3", NULL, "PUT", path, NULL, data, dataLen, "Host:%s\r\n", host);
        url = sfmt("https://%s/%s", host, path);
        rFree(path);

    } else {
        /*
            Virtual style s3 host request
            Ref: https://docs.aws.amazon.com/AmazonS3/latest/userguide/RESTAPI.html
         */
        host = sfmt("%s.s3.%s.amazonaws.com", bucket, region);
        headers = awsSign(region, "s3", NULL, "PUT", key, NULL, data, dataLen, "Host:%s\r\n", host);
        url = sfmt("https://%s/%s", host, key);
    }
    status = urlFetch(up, "PUT", url, data, dataLen, headers);

    if (status != URL_CODE_OK) {
        error = up->error ? up->error : urlGetResponse(up);
        rError("cloud", "Cannot put to S3 %s%s. %s", host, key, error);
    }
    rFree(headers);
    rFree(url);
    rFree(host);
    return status == URL_CODE_OK ? 0 : R_ERR_CANT_WRITE;
}

#else
void dummyHelpers(void)
{
}
#endif /* SERVICES_CLOUD */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/cloud/logs.c ************/

/*
    logs.c - Capture logs, files or command output to the cloud

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



#if SERVICES_LOGS
/************************************ Locals **********************************/
/*
    Initial input buffer size
 */
#define MAX_LINE 2048

typedef struct Log {
    IotoLog *log;                   /* Log capture */
    char *path;                     /* Log filename */
    RBuf *buf;                      /* Input buffer */
    RWait *wait;                    /* Wait on IO */
    FILE *fp;                       /* File pointer for the file or command pipe */
    Offset pos;                     /* File position */
    ino_t inode;                    /* File inode number */
    dev_t dev;                      /* File dev number */
    cchar *command;                 /* Command to run */
    cchar *continuation;            /* Continuation line prefix */
    bool lines : 1;                 /* Output is composed of one line entries (with continuations) */
    bool tail : 1;                  /* Capture from the file tail */
#if LINUX && HAS_INOTIFY
    int notifyFd;                   /* inotify fd */
    RWait *notifyWait;              /* Wait on IO */
#endif
    int wfd;                        /* Watch fd */
} Log;

typedef struct WalkContext {
    RList *list;
    cchar *filename;
    Time latest;
} WalkContext;

static Log *allocLog(Json *json, int id, cchar *path);
static void closeLog(Log *lp);
static void freeLog(Log *lp);
static int openLog(Log *lp);
static void readLog(Log *lp);
static void setWaitMask(Log *lp);
static int startLog(Log *lp);
static int startLogService(void);
static void logEvent(Log *lp);
#if LINUX && HAS_INOTIFY
static void logNotify(Log *lp, int mask, int fd);
#endif

/************************************* Code ***********************************/

PUBLIC int ioInitLogs(void)
{
    ioto->logs = rAllocList(0, 0);

    startLogService();
    return 0;
}

PUBLIC void ioTermLogs(void)
{
    Log *lp;
    int next;

    for (ITERATE_ITEMS(ioto->logs, lp, next)) {
        freeLog(lp);
    }
    rFreeList(ioto->logs);
    ioto->logs = 0;
}

static Log *allocLog(Json *json, int id, cchar *path)
{
    Log   *lp;
    cchar *group;
    char  *stream;
    bool  create;
    int   linger, maxEvents, maxSize;

    lp = rAllocType(Log);

    lp->command = jsonGet(json, id, "command", 0);
    lp->continuation = jsonGet(json, id, "continuation", " \t");
    lp->lines = jsonGetBool(json, id, "lines", lp->command ? 0 : 1);
    lp->tail = smatch(jsonGet(json, id, "position", "end"), "end");

#if LINUX && HAS_INOTIFY
    lp->notifyFd = -1;
#endif
    lp->wfd = -1;
    lp->path = sclone(path);

    create = jsonGetBool(json, id, "create", 1);
    maxEvents = jsonGetInt(json, id, "maxEvents", -1);
    maxSize = jsonGetInt(json, id, "maxSize", -1);
    linger = jsonGetInt(json, id, "linger", -1);
    group = jsonGet(json, id, "group", 0);

    ioSetTemplateVar("filename", rBasename(path));
    stream = ioExpand(jsonGet(json, id, "stream", "${hostname}-${filename}"));

    lp->log = ioAllocLog(lp->path, ioto->awsRegion, create, group, stream, maxEvents, maxSize, linger);
    rFree(stream);

#if LINUX && HAS_INOTIFY
    if ((lp->notifyFd = inotify_init()) < 0) {
        rError("logs", "Cannot initialize inotify");
        return 0;
    }
    lp->notifyWait = rAllocWait(lp->notifyFd);
    rSetWaitHandler(lp->notifyWait, (RWaitProc) logNotify, lp, R_MODIFIED);
#endif
    return lp;
}

PUBLIC void freeLog(Log *lp)
{
    assert(lp);

    if (!lp) {
        return;
    }
    ioFreeLog(lp->log);
    if (lp->fp) {
        rFreeWait(lp->wait);
        if (lp->command) {
            pclose(lp->fp);
        } else {
            fclose(lp->fp);
        }
        lp->fp = 0;
    }
#if LINUX && HAS_INOTIFY
    if (lp->notifyFd >= 0) {
        rFreeWait(lp->notifyWait);
        // This automatically releases all watch descriptors
        close(lp->notifyFd);
        lp->notifyFd = -1;
    }
#endif
    rFree(lp->path);
    rFreeBuf(lp->buf);
    rFree(lp);
}

static int startLogService(void)
{
    Log      *lp;
    Json     *json;
    JsonNode *child, *files;
    RList    *list;
    cchar    *path;
    int      id, next;

    if (!ioto->logs) return 0;

    json = ioto->config;
    files = jsonGetNode(json, 0, "files");

    if (files) {
        for (ITERATE_JSON(json, files, child, id)) {
            if (jsonGetBool(json, id, "enable", 1) == 1) {
                path = jsonGet(json, id, "path", 0);
                list = rGetFiles("/", path, R_WALK_FILES);
                for (ITERATE_ITEMS(list, path, next)) {
                    if ((lp = allocLog(json, id, path)) != 0) {
                        rAddItem(ioto->logs, lp);
                    }
                }
            }
        }
    }
    for (ITERATE_ITEMS(ioto->logs, lp, next)) {
        startLog(lp);
    }
    return 0;
}

static int startLog(Log *lp)
{
    assert(lp);

#if LINUX && HAS_INOTIFY
    struct stat info;
    /*
        On Linux, delay opening the file until we get an inotify event.
        This scales better as we can watch many files without consuming a file descriptor.
        Note: this is a watch fd and not a real file descriptor.
     */
    if (lp->command) {
        if (openLog(lp) < 0) {
            return R_ERR_CANT_OPEN;
        }
    } else {
        if ((lp->wfd = inotify_add_watch(lp->notifyFd, lp->path, IN_CREATE | IN_MOVE | IN_MODIFY)) < 0) {
            int err = errno;
            if (rAccessFile(lp->path, R_OK) == 0) {
                rError("logs", "Cannot add watch for %s, errno %d", lp->path, err);
            }
        } else if (stat(lp->path, &info) == 0) {
            lp->inode = info.st_ino;
            lp->pos = info.st_size;
        }
    }
#else
    /*
        Need to open the file here as we use the open file descriptor on BSD to wait for I/O events
     */
    if (openLog(lp) < 0) {
        return R_ERR_CANT_OPEN;
    }
#endif
    return 0;
}

#if LINUX && HAS_INOTIFY
static void logNotify(Log *lp, int mask, int fd)
{
    struct inotify_event *event;
    char                 buf[ME_BUFSIZE];
    ssize                len;
    int                  i;

    assert(lp);

    len = read(fd, buf, sizeof(buf));
    for (i = 0; i < len; ) {
        event = (struct inotify_event*) &buf[i];
        if (lp->wfd == event->wd) {
            logEvent(lp);
            break;
        }
        i += sizeof(struct inotify_event) + event->len;
    }
}
#endif

static void logEvent(Log *lp)
{
    assert(lp);

#if LINUX && HAS_INOTIFY
    if (!lp->fp && openLog(lp) < 0) {
        return;
    }
#endif
    readLog(lp);
    setWaitMask(lp);
}

static void setWaitMask(Log *lp)
{
    if (lp->command) {
        rSetWaitMask(lp->wait, R_READABLE, lp->wait->deadline);
#if MACOSX
    } else {
        Ticks deadline = lp->wait ? lp->wait->deadline : 0;
        rSetWaitMask(lp->wait, ((int64) R_MODIFIED) | ((int64) NOTE_WRITE << 32) | R_READABLE, deadline);
#endif
    }
}

static int openLog(Log *lp)
{
    struct stat info;

    assert(lp);

    if (lp->command) {
        rTrace("logs", "Run command: %s", lp->command);
        assert(lp->fp == 0);
        if ((lp->fp = popen(lp->command, "r")) == 0) {
            rError("logs", "Cannnot open command \"%s\", errno %d", lp->command, errno);
            return R_ERR_CANT_OPEN;
        }
    } else {
        if (lp->fp) {
            if (ferror(lp->fp)) {
                fclose(lp->fp);
                lp->fp = 0;
            }
        }
        if (!lp->fp) {
            if ((lp->fp = fopen(lp->path, "r")) == 0) {
                //  Continue
                rTrace("logs", "Cannot open \"%s\"", lp->path);
                return 0;
            }
        }
        if (lp->pos == 0) {
            if (lp->tail) {
                fseek(lp->fp, 0, SEEK_END);
            } else {
                fseek(lp->fp, 0, SEEK_SET);
            }
        } else {
            // Check if the file is the same inode as the last open. If so, use the last know position
            if (fstat(fileno(lp->fp), &info) == 0 && info.st_ino == lp->inode) {
                if (fseek(lp->fp, lp->pos, SEEK_SET) < 0) {
                    fseek(lp->fp, 0, SEEK_END);
                }
            }
        }
        lp->pos = ftell(lp->fp);
        if (fstat(fileno(lp->fp), &info) == 0) {
            lp->inode = info.st_ino;
        }
    }
    if (!lp->buf) {
        lp->buf = rAllocBuf(ME_BUFSIZE);
    }
    lp->wait = rAllocWait(fileno(lp->fp));

    if (lp->command) {
        rSetWaitHandler(lp->wait, (RWaitProc) logEvent, lp, R_READABLE);
#if MACOSX
    } else {
        rSetWaitHandler(lp->wait, (RWaitProc) logEvent, lp,
                        ((int64) R_MODIFIED) | ((int64) NOTE_WRITE << 32) | R_READABLE);
#endif
    }
    return 0;
}

static void closeLog(Log *lp)
{
    struct stat info;
    int         fd, status;

    assert(lp);

    if (lp->fp) {
        if (lp->command) {
            fd = fileno(lp->fp);
            if ((status = pclose(lp->fp)) != 0) {
                rError("logs", "Bad exit status for command \"%s\", status %d, fd %d, errno %d ECHILD %d",
                       lp->command, status, fd, errno, ECHILD);
            }
        } else {
            lp->pos = ftell(lp->fp);
            fd = fileno(lp->fp);
            if (fstat(fd, &info) == 0) {
                lp->inode = info.st_ino;
            }
            rFreeWait(lp->wait);
            lp->wait = 0;
            fclose(lp->fp);
            lp->wfd = -1;
        }
        lp->fp = 0;
    }
}

static void readLog(Log *lp)
{
    RBuf  *buf;
    FILE  *fp;
    char  *eol, *pos, *sol;
    ssize nbytes;

    assert(lp);
    buf = lp->buf;
    assert(buf);
    fp = lp->fp;
    assert(fp);
    clearerr(fp);

    while (1) {
        if (rGetBufSpace(buf) < (ME_BUFSIZE / 2)) {
            rGrowBuf(buf, max(buf->buflen / 2, ME_BUFSIZE));
        }
        /*
            This will not block. This function is only ever called get here as the result of an I/O event.
            We test feof/ferror bbelow before we read more data.
         */
        if ((nbytes = fread(rGetBufEnd(buf), 1, (int) rGetBufSpace(buf) - 1, fp)) == 0) {
            break;
        }
        rAdjustBufEnd(buf, nbytes);
        rAddNullToBuf(buf);

        if (lp->lines) {
            eol = 0;
            for (sol = pos = rGetBufStart(buf); rGetBufLength(buf) > 0; pos = eol) {
                if ((eol = strchr(pos, '\n')) == 0) {
                    if (rGetBufLength(buf) < MAX_LINE) {
                        // No new line - need more data
                        break;
                    }
                    rGrowBuf(buf, 2);
                    eol = rGetBufEnd(buf);
                }
                if (eol[1] && schr(lp->continuation, eol[1]) != 0) {
                    // Continuation
                    eol++;
                    continue;
                }
                *eol++ = '\0';
                /*
                    Capture log message
                 */
                ioLogMessage(lp->log, 0, sol);
                rAdjustBufStart(buf, eol - sol);
                sol = eol;
            }
            // len = eol ? (eol - sol) : rGetBufLength(buf);
            rCompactBuf(buf);
        }
        if (ferror(fp) || feof(fp)) {
            if (rGetBufLength(buf) > 0 && !lp->lines) {
                rFlushBuf(buf);
            }
            break;
        }
    }
    if (ferror(fp) || (feof(fp) && lp->command)) {
        closeLog(lp);
    } else {
        lp->pos = ftell(lp->fp);
    }
}

#else
void dummyLogs(void)
{
}
#endif /* SERVICES_LOGS */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/cloud/provision.c ************/

/*
    provision.c - Provision the device with MQTT certificates and API endpoints

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



#if SERVICES_PROVISION
/*********************************** Locals ***********************************/

static REvent provisioningEvent;

/*********************************** Forwards *********************************/

static Ticks backoff(Ticks delay);
static void extractKeys(void);
static void parseProvisioningResponse(Json *json);
static bool provisionDevice(void);
static void postProvisionSync(void);

/************************************* Code ***********************************/
/*
    If another call is active, wake it up and defer to it
 */
PUBLIC void ioWakeProvisioner()
{
    if (provisioningEvent) {
        rRunEvent(provisioningEvent);
    }
}

/*
    Provision the device from the device cloud. This blocks until claimed and provisioned.
    This code is serialized but currently is only ever called by ioConnect which is also serialized.
    May block for a long time.
 */
PUBLIC int ioProvision()
{
    Ticks       delay;
    static bool provisioning = 0;

    //  Wake any long sleeper in backoff()
    ioWakeProvisioner();
    rEnter(&provisioning, 0);

    /*
        Wait for device to be claimed (will set api)
     */
    for (delay = TPS; !ioto->api; delay = backoff(delay)) {
        if (ioRegister() == R_ERR_BAD_ARGS) {
            return R_ERR_BAD_ARGS;
        }
        if (ioto->api) break;
    }
    for (delay = TPS; !ioto->endpoint; delay = backoff(delay)) {
        //  This can be long, waiting for the device to be claimed
        if (provisionDevice()) {
            break;
        }
        rInfo("provision", "Device not yet claimed, waiting %lld secs ...", delay / 1000);
    }
    rLeave(&provisioning);
    return 0;
}

/*
    Exponential backoff. This can be awakened via ioWakeProvisioner()
 */
static Ticks backoff(Ticks delay)
{
    delay += TPS;
    if (delay > 86400 * TPS) {
        delay = 86400 * TPS;
    }
    /*
        Use this rather than sleep so that we can wakeup prematurely if required
     */
    provisioningEvent = rStartEvent(NULL, 0, delay);
    rYieldFiber(0);
    provisioningEvent = 0;
    return delay;
}

/*
    Send a provisioning request to the device cloud
 */
static bool provisionDevice(void)
{
    Json *json;
    char data[80], url[160];

    /*
        Talk to the device cloud to get certificates
     */
    SFMT(url, "%s/tok/provision/getCerts", ioto->api);
    SFMT(data, "{\"id\":\"%s\"}", ioto->id);
    json = urlPostJson(url, data, -1, "Authorization: bearer %s\r\nContent-Type: application/json\r\n", ioto->apiToken);

    if (json == 0 || json->count == 0) {
        rError("provision", "Cannot provision device");
        jsonFree(json);
        return 0;
    }
    parseProvisioningResponse(json);
    jsonFree(json);
    return 1;
}

/*
    Parse provisioning response payload from the device cloud.
    This saves the response in provision.json5 and sets ioto->api if provisioned.
 */
static void parseProvisioningResponse(Json *json)
{
    cchar *certificate, *key;
    char  *certMem, *keyMem, *path;

    key = certificate = path = 0;
    rInfo("provision", "Device claimed");

    /*
        Extract provisioning certificates for MQTT communications with AWS IoT
     */
    certificate = jsonGet(json, 0, "certificate", 0);
    key = jsonGet(json, 0, "key", 0);
    if (!certificate || !key) {
        rError("provision", "Provisioning is missing certificate");
        return;
    }
    if (ioto->nosave) {
        certMem = sfmt("@%s", certificate);
        keyMem = sfmt("@%s", key);
        jsonSet(json, 0, "certificate", certMem, JSON_STRING);
        jsonSet(json, 0, "key", keyMem, JSON_STRING);
        rFree(certMem);
        rFree(keyMem);

    } else {
        path = rGetFilePath(IO_CERTIFICATE);
        if (rWriteFile(path, certificate, -1, 0600) < 0) {
            rError("provision", "Cannot save certificate to %s", path);
        } else {
            jsonSet(json, 0, "certificate", path, JSON_STRING);
        }
        rFree(path);

        path = rGetFilePath(IO_KEY);
        if (rWriteFile(path, key, -1, 0600) < 0) {
            rError("provision", "Cannot save key to %s", path);
        } else {
            jsonSet(json, 0, "key", path, JSON_STRING);
        }
        rFree(path);
    }
    jsonRemove(json, 0, "cert");
    jsonBlend(ioto->config, 0, "provision", json, 0, 0, 0);

    if (rEmitLog("debug", "provision")) {
        rDebug("provision", "%s", jsonString(json));
    }

    if (!ioto->nosave) {
        path = rGetFilePath(IO_PROVISION_FILE);
        if (jsonSave(ioto->config, 0, "provision", path, 0600, JSON_PRETTY | JSON_QUOTES) < 0) {
            rError("provision", "Cannot save provisioning state to %s", path);
            rFree(path);
            return;
        }
        rFree(path);
    }
    rFree(ioto->account);
    ioto->account = jsonGetClone(ioto->config, 0, "provision.accountId", 0);
    dbAddContext(ioto->db, "accountId", ioto->account);

    rFree(ioto->cloudType);
    ioto->cloudType = jsonGetClone(ioto->config, 0, "provision.cloudType", 0);
    rFree(ioto->endpoint);
    ioto->endpoint = jsonGetClone(ioto->config, 0, "provision.endpoint", 0);

    rInfo("provision", "Device provisioned for %s cloud \"%s\" in %s",
          jsonGet(ioto->config, 0, "provision.cloudType", 0),
          jsonGet(ioto->config, 0, "provision.cloudName", 0),
          jsonGet(ioto->config, 0, "provision.cloudRegion", 0)
          );
#if SERVICES_SYNC
    rWatch("mqtt:connected", (RWatchProc) postProvisionSync, 0);
#endif
    rSignal("device:provisioned", 0);

#if SERVICES_KEYS
    ioGetKeys();
#endif
}

/*
    One-time db sync after provisioning
 */
static void postProvisionSync(void)
{
    ioSyncUp(1);
    rWatchOff("mqtt:connected", (RWatchProc) postProvisionSync, 0);
}

/*
    Receive provisioning command (release)
 */
PUBLIC void ioRelease(MqttRecv *rp)
{
    Time  timestamp;
    cchar *cmd;

    cmd = rBasename(rp->topic);
    if (smatch(cmd, "release")) {
        timestamp = stoi(rp->data);
        if (timestamp == 0) {
            timestamp = rGetTime();
        }
        //  Ignore stale release commands, we may have reprovisioned since then
        if (rGetTime() < (timestamp + 10 * TPS)) {
            //  Unit tests may get a stale restart command
            if (!ioto->cmdTest) {
                rInfo("sync", "Received provisioning command %s", rp->topic);
                ioDisconnect();
                ioDeprovision();
            }
        }
    } else {
        rError("provision", "Unknown provision command %s", cmd);
    }
}

PUBLIC void ioDeprovision(void)
{
    char *path;

    rFree(ioto->api);
    ioto->api = 0;
    rFree(ioto->apiToken);
    ioto->apiToken = 0;
    rFree(ioto->account);
    ioto->account = 0;
    rFree(ioto->endpoint);
    ioto->endpoint = 0;
    rFree(ioto->cloudType);
    ioto->cloudType = 0;
    ioto->registered = 0;

    jsonSet(ioto->config, 0, "provision.certificate", 0, 0);
    jsonSet(ioto->config, 0, "provision.key", 0, 0);
    jsonSet(ioto->config, 0, "provision.endpoint", 0, 0);
    jsonSet(ioto->config, 0, "provision.accountId", 0, 0);
    jsonSet(ioto->config, 0, "provision.cloudType", 0, 0);

    //  Remove certificates
    path = rGetFilePath(IO_CERTIFICATE);
    unlink(path);
    rFree(path);

    path = rGetFilePath(IO_KEY);
    unlink(path);
    rFree(path);

    //  Remove provisioning state
    jsonRemove(ioto->config, 0, "provision");

    path = rGetFilePath(IO_PROVISION_FILE);
    unlink(path);
    rFree(path);
    rInfo("provision", "Device deprovisioned");
}

/*
    Renew device IAM credentials
 */
PUBLIC void ioGetKeys(void)
{
    Json  *json, *config;
    char  url[160];
    Ticks delay;

    SFMT(url, "%s/tok/provision/getCreds", ioto->api);

    json = urlPostJson(url, 0, -1, "Authorization: bearer %s\r\nContent-Type: application/json\r\n", ioto->apiToken);
    if (json == 0) {
        rError("provision", "Cannot get credentials");
        return;
    }
    /*
        Blend into in-memory config so we can keep persistent links to the key values.
     */
    config = ioto->config;
    jsonBlend(config, 0, "provision.keys", json, 0, 0, 0);

    extractKeys();
    jsonFree(json);

    delay = min((ioto->awsExpires - rGetTime()) - (20 * 60 * TPS), 5 * 60 * TPS);
    rStartEvent((REventProc) ioGetKeys, 0, delay);
}

static void extractKeys(void)
{
    cchar *prior;
    int   pid;

    pid = jsonGetId(ioto->config, 0, "provision.keys");

    prior = ioto->awsAccess;
    ioto->awsAccess = jsonGetClone(ioto->config, pid, "accessKeyId", 0);
    ioto->awsSecret = jsonGetClone(ioto->config, pid, "secretAccessKey", 0);
    ioto->awsToken = jsonGetClone(ioto->config, pid, "sessionToken", 0);
    ioto->awsRegion = jsonGetClone(ioto->config, pid, "region", 0);
    ioto->awsExpires = rParseIsoDate(jsonGet(ioto->config, pid, "expires", 0));

    /*
        Update logging on first time key fetch
     */
    if (!prior) {
        ioUpdateLog(0);
    }
    rSignal("device:keys", 0);
}

#else
void dummyProvision(void)
{
}
#endif /* SERVICES_PROVISION */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/cloud/shadow.c ************/

/*
    shadow.c - Shadow state management

    shadow.json contains control state and is saved to AWS IoT Shadows.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



#if SERVICES_SHADOW
/************************************ Forwards *********************************/

static void lazySave(Json *json, int delay);
static Json *loadShadow(void);
static void onShadowReceive(MqttRecv *rp);
static int publishShadow(Json *json);
static int saveShadow(Json *json);
static void subscribeShadow(void);

/************************************* Code ***********************************/

PUBLIC int ioInitShadow(void)
{
    if ((ioto->shadow = loadShadow()) == 0) {
        return R_ERR_CANT_READ;
    }
    ioto->shadowName = jsonGetClone(ioto->config, 0, "cloud.shadow", "default");
    ioto->shadowTopic = sfmt("$aws/things/%s/shadow/name/%s", ioto->thing, ioto->shadowName);
    ioOnConnect((RWatchProc) subscribeShadow, 1);
    return 0;
}

PUBLIC void ioTermShadow(void)
{
    Json *json;

    json = ioto->shadow;

    if (json) {
        if (json->event) {
            rStopEvent(json->event);
            saveShadow(json);
        }
        jsonFree(json);
    }
    rFree(ioto->shadowName);
    rFree(ioto->shadowTopic);
    ioto->shadowTopic = 0;
    ioto->shadowName = 0;
    ioto->shadow = 0;
}

static void subscribeShadow(void)
{
    if (!smatch(ioto->cloudType, "dedicated")) {
        rError("shadow", "Cloud type \"%s\" does not support AWS IoT shadows", ioto->cloudType);
    } else {
        //  OPT - could common up to just %s/
        mqttSubscribe(ioto->mqtt, onShadowReceive, 1, MQTT_WAIT_NONE, "%s/get/accepted", ioto->shadowTopic);
        mqttSubscribe(ioto->mqtt, onShadowReceive, 1, MQTT_WAIT_NONE, "%s/get/rejected", ioto->shadowTopic);
        mqttSubscribe(ioto->mqtt, onShadowReceive, 1, MQTT_WAIT_NONE, "%s/update/accepted", ioto->shadowTopic);
        mqttSubscribe(ioto->mqtt, onShadowReceive, 1, MQTT_WAIT_NONE, "%s/update/rejected", ioto->shadowTopic);
        mqttPublish(ioto->mqtt, "", 0, 1, MQTT_WAIT_ACK, "%s/get", ioto->shadowTopic);
        rInfo("shadow", "Connected to: AWS IOT core");
    }
}

PUBLIC void ioSaveShadow()
{
    lazySave(ioto->shadow, 0);
}

static void lazySave(Json *json, int delay)
{
    if (!json->event) {
        json->event = rStartEvent((REventProc) saveShadow, json, delay);
    }
}

static int saveShadow(Json *json)
{
    char *path;

    if (ioto->nosave) return 0;
    json->event = 0;

    path = rGetFilePath(IO_SHADOW_FILE);
    if (jsonSave(json, 0, 0, path, ioGetFileMode(), JSON_PRETTY) < 0) {
        rError("shadow", "Cannot save to %s, errno %d", json->path, errno);
        rFree(path);
        return R_ERR_CANT_WRITE;
    }
    rFree(path);
    return publishShadow(json);
}

PUBLIC char *ioGetShadow(cchar *key, cchar *defaultValue)
{
    return jsonGetClone(ioto->shadow, 0, key, defaultValue);
}

PUBLIC void ioSetShadow(cchar *key, cchar *value, bool save)
{
    jsonSet(ioto->shadow, 0, key, value, 0);
    if (save) {
        lazySave(ioto->shadow, IO_SAVE_DELAY);
    }
}

PUBLIC int ioGetFileMode(void)
{
    return smatch(ioto->profile, "dev") ?  0666 : 0600;
}

static Json *loadShadow()
{
    Json *json;
    char *errorMsg, *path;

    if (rAccessFile(path, R_OK) == 0) {
        path = rGetFilePath(IO_SHADOW_FILE);
        json = jsonParseFile(path, &errorMsg, 0);
        rFree(path);

        if ((json = jsonParseFile(path, &errorMsg, 0)) == 0) {
            rError("shadow", "%s", errorMsg);
            rFree(errorMsg);
            return 0;
        }
    } else {
        json = jsonAlloc(0);
    }
#if KEEP
    rPrintf("%s\n%s\n", path, jsonString(json));
#endif
    return json;
}

static void onShadowReceive(MqttRecv *rp)
{
    Json  *json;
    int   nid;
    char  *data, *msg, path[ME_MAX_FNAME];
    cchar *topic;

    topic = rp->topic;

    msg = snclone(rp->data, rp->dataSize);
    rTrace("shadow", "Received shadow: %s", msg);

    if (sends(topic, "/get/accepted")) {

        //  Extract state.reported
        json = jsonParse(msg, 0);
        nid = jsonGetId(json, 0, "state.reported");
        data = jsonToString(json, nid, 0, JSON_PRETTY);

        jsonFree(ioto->shadow);
        ioto->shadow = jsonParse(data, 0);

        //  Just to make local debugging easier.
        path = rGetFilePath(IO_SHADOW_FILE);
        rWriteFile(path, data, slen(data), ioGetFileMode());
        rFree(path);
        rFree(data);
        jsonFree(json);

    } else if (sends(topic, "/get/rejected")) {
        rError("shadow", "Get shadow rejected: %s", msg);

    } else if (sends(topic, "/update/accepted")) {
        ;

    } else if (sends(topic, "/update/rejected")) {
        rError("shadow", "Update shadow rejected: %s", msg);
    }
    rFree(msg);
}

/*
    Publish to AWS IOT core shadows
 */
static int publishShadow(Json *json)
{
    char *buf, *data;

    if (ioto->mqtt == 0) return R_ERR_BAD_STATE;

    if ((data = jsonToString(json, 0, 0, JSON_QUOTES)) == 0) {
        return R_ERR_BAD_STATE;
    }
    if (slen(data) > IO_MESSAGE_SIZE) {
        rError("shadow", "State is too big to save to AWS IOT");
        rFree(data);
        return R_ERR_WONT_FIT;
    }
    buf = sfmt("{\"state\":{\"reported\":%s}}", data);
    mqttPublish(ioto->mqtt, buf, slen(buf), 0, MQTT_WAIT_NONE, "%s/update", ioto->shadowTopic, ioto->shadowName);
    rFree(buf);
    rFree(data);
    return 0;
}

#else
void dummyShadow(void)
{
}
#endif /* SERVICES_CLOUD */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/cloud/sync.c ************/

/*
    sync.c - Sync database data to the cloud

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/*********************************** Locals ************************************/
#if SERVICES_SYNC

/*
    Delay waiting for an acknowledgement after sending a sync message to the cloud
 */
#define SYNC_DELAY (5 * TPS)

/**
    Database sync change record. One allocated for each mutation to the database.
    Changes implement a buffer cache for database changes. The ioto.json5 provides a maxSyncSize.
    For performance, change items are be buffered to aggregate multiple mutations into
    a single sync message to the cloud.
 */
typedef struct Change {
    DbItem *item;
    char *cmd;
    char *key;          //  Local database item key
    char *data;
    char *updated;      //  When updated
    Time due;           //  When change is due to be sent
    int seq;
} Change;

/*
    Sequence number for change sets sent to the cloud
 */
static int nextSeq = 1;

/************************************ Forwards *********************************/

static Change *allocChange(cchar *cmd, cchar *key, char *data, cchar *updated, Ticks due);
static void applySyncLog(void);
static void cleanChanges(Json *json);
static void dbCallback(void *arg, Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd, int events);
static void deviceCommand(void *arg, struct Db *db, struct DbModel *model, struct DbItem *item,
                          struct DbParams *params, cchar *cmd, int event);
static void freeChange(Change *change);
static void logChange(Change *change);
static void processDeviceCommand(DbItem *item);
static cchar *readBlock(FILE *fp, RBuf *buf);
static int readSize(FILE *fp);
static void receiveSync(MqttRecv *rp);
static void recreateSyncLog(void);
static void scheduleSync(Change *change);
static void syncItem(DbModel *model, DbItem *item, DbParams *params, cchar *cmd, bool guarantee);
static void updateChange(Change *change, cchar *cmd, char *data, cchar *updated, Ticks due);
static int writeBlock(cchar *buf);
static int writeSize(int len);

/************************************* Code ***********************************/

PUBLIC int ioInitSync(void)
{
    nextSeq = rand();
    ioto->syncDue = MAXINT64;
    ioto->syncHash = rAllocHash(0, 0);
    ioto->maxSyncSize = (int) svalue(jsonGet(ioto->config, 0, "database.maxSyncSize", "1k"));
    dbAddCallback(ioto->db, (DbCallbackProc) dbCallback, NULL, NULL, DB_ON_COMMIT | DB_ON_FREE);
    recreateSyncLog();
    return 0;
}

PUBLIC void ioTermSync(void)
{
    RName  *np;
    Change *change;
    char   path[ME_MAX_FNAME];

    for (ITERATE_NAMES(ioto->syncHash, np)) {
        change = np->value;
        freeChange(change);
    }
    rFreeHash(ioto->syncHash);
    ioto->syncHash = 0;

    /*
        The sync log is used to recover from crashes only
        As we're doing an orderly shutdown, can remove here
     */
    if (ioto->syncLog) {
        fclose(ioto->syncLog);
        ioto->syncLog = 0;
        unlink(SFMT(path, "%s.sync", ioto->db->path));
    }
}

/*
    Force a sync of ALL syncing items in the database
    This is called after provisioning to sync the entire database for the first time.
    Users can call this if necessary.
    If guarantee is true, then reliably save the change record until the cloud acknowledges receipt.
 */
PUBLIC void ioSyncUp(bool guarantee)
{
    RbNode  *node;
    DbModel *model;
    DbItem  *item;

    for (node = rbFirst(ioto->db->primary); node; node = rbNext(ioto->db->primary, node)) {
        item = node->data;
        model = dbGetItemModel(ioto->db, item);
        if (model && model->sync) {
            syncItem(model, item, NULL, "update", guarantee);
        }
    }
    ioFlushSync(0);
}

PUBLIC void ioSyncDown(Time when)
{
    char  *lastSync;
    char  msg[160];

    //  Send SyncDown message
    if (when < 0) {
        lastSync = sclone(dbGetField(ioto->db, "SyncState", "lastSyncDown", NULL, NULL));
    } else {
        lastSync = rGetIsoDate(when);
    }
    SFMT(msg, "{\"lastSync\":\"%s\"}", lastSync ? lastSync : "0");
    rFree(lastSync);
    //  DEPRECATE syncUp and rename syncDown
    mqttPublish(ioto->mqtt, msg, -1, 1, MQTT_WAIT_NONE, "$aws/rules/IotoDevice/ioto/service/%s/db/syncUp", ioto->id);
}

/*
    Process the sync log and re-create the change hash.
    Immediately send changes to the cloud.
    The sync log contains a fail-safe record of local database changes that must be replicated to
    the cloud. It is applied on Ioto restart after an unexpected exit. It is erased after processing.
 */
static void applySyncLog(void)
{
    FILE   *fp;
    RBuf   *buf;
    Change *change;
    Ticks  now;
    char   path[ME_MAX_FNAME];
    cchar  *cmd, *data, *key, *updated;
    int    bufsize;

    if (ioto->nosave) return;

    if (ioto->syncLog) {
        fclose(ioto->syncLog);
        ioto->syncLog = 0;
    }
    if ((fp = fopen(SFMT(path, "%s.sync", ioto->db->path), "r+")) != NULL) {
        ioto->syncLog = fp;
        now = rGetTicks();

        buf = rAllocBuf(ME_BUFSIZE);
        while ((bufsize = readSize(fp)) > 0) {
            if (rGrowBuf(buf, bufsize) < 0) {
                break;
            }
            cmd = readBlock(fp, buf);
            data = readBlock(fp, buf);
            key = readBlock(fp, buf);
            updated = readBlock(fp, buf);
            if (!cmd || !data || !key || !updated) {
                //  Log is corrupt
                recreateSyncLog();
                break;
            }
            if ((change = rLookupName(ioto->syncHash, key)) != NULL) {
                updateChange(change, cmd, sclone(data), updated, now);
            } else {
                change = allocChange(cmd, key, sclone(data), updated, now);
                rAddName(ioto->syncHash, change->key, change, 0);
            }
            rFlushBuf(buf);
        }
        rFreeBuf(buf);
    }
    if (rGetHashLength(ioto->syncHash) > 0) {
        ioFlushSync(0);
    }
}

static Change *allocChange(cchar *cmd, cchar *key, char *data, cchar *updated, Ticks now)
{
    Change *change;

    if ((change = rAllocType(Change)) == 0) {
        return 0;
    }
    change->cmd = sclone(cmd);
    change->key = sclone(key);
    change->updated = sclone(updated);
    change->data = data;
    change->due = now;
    return change;
}

static void freeChange(Change *change)
{
    rRemoveName(ioto->syncHash, change->key);
    rFree(change->cmd);
    rFree(change->data);
    rFree(change->key);
    rFree(change->updated);
    rFree(change);
}

static void updateChange(Change *change, cchar *cmd, char *data, cchar *updated, Ticks now)
{
    rFree(change->cmd);
    change->cmd = sclone(cmd);
    rFree(change->data);
    change->data = data;
    rFree(change->updated);
    change->updated = sclone(updated);
    change->due = now;
}

/*
    Read the size of a sync log item
 */
static int readSize(FILE *fp)
{
    int len;

    if (fread(&len, sizeof(len), 1, fp) != 1) {
        //  EOF
        return 0;
    }
    return len;
}

static cchar *readBlock(FILE *fp, RBuf *buf)
{
    cchar *result;
    int   len;

    //  The length includes a trailing null
    if (fread(&len, sizeof(len), 1, fp) != 1) {
        rError("sync", "Corrupt sync log");
        return 0;
    }
    if (len < 0 || len > DB_MAX_ITEM) {
        rError("sync", "Corrupt sync log");
        return 0;
    }
    if (fread(rGetBufStart(buf), len, 1, fp) != 1) {
        rError("sync", "Corrupt sync log");
        return 0;
    }
    result = rGetBufStart(buf);
    rAdjustBufEnd(buf, len);
    rAdjustBufStart(buf, rGetBufLength(buf));
    return result;
}

/*
    Database trigger invoked for local database changes
 */
static void dbCallback(void *arg, Db *db, DbModel *model, DbItem *item, DbParams *params,
                       cchar *cmd, int events)
{
    Change *change;

    if (events & DB_ON_FREE) {
        if ((change = rLookupName(ioto->syncHash, item->key)) != 0) {
            rRemoveName(ioto->syncHash, item->key);
            freeChange(change);
        }
    } else if (events & DB_ON_COMMIT) {
        //  Bypass is set for items that should not be sent to the cloud
        if (model->sync && !(params && params->bypass)) {
            syncItem(model, item, params, cmd, 1);
        }
    }
}

/*
    Synchronize state to the cloud and local disk.
    Don't prep a change record to sync to the cloud if the model does not want it, or
    if this update came from a cloud update (i.e. stop infinite looping updates).
    If guarantee is true, then reliably save the change record until the cloud acknowledges receipt.
 */
static void syncItem(DbModel *model, DbItem *item, DbParams *params, cchar *cmd, bool guarantee)
{
    Change *change;
    cchar  *updated;
    char   *data;
    Ticks  now;

    if (!model || !model->sync || (params && params->bypass)) {
        return;
    }
    /*
        Overwrite prior buffered change records if the item has changed.
        If change->seq is set, the change has been sent, but not acknowledged, so cannot overwrite.
        The prior ack will just be ignored and this change will have a new seq.
     */
    if ((change = rLookupName(ioto->syncHash, item->key)) == 0 || change->seq) {
        //  Item.json takes precedence over item.value
        data = item->json ? jsonToString(item->json, 0, 0, JSON_QUOTES) : sclone(item->value);
        updated = dbField(item, "updated");
        now = rGetTicks();

        if (change) {
            updateChange(change, cmd, data, updated, now);
        } else {
            change = allocChange(cmd, item->key, data, updated, now);
            rAddName(ioto->syncHash, change->key, change, 0);
        }
    }
    if (guarantee) {
        logChange(change);
    }
    if (ioto->mqtt) {
        scheduleSync(change);
    }
    rSignal("db:change", change);
}

/*
    Fail-safe sync. Write to the sync log and replay after a crash.
 */
static void logChange(Change *change)
{
    int len;

    if (!ioto->syncLog || ioto->nosave) {
        return;
    }
    len = (int) (slen(change->cmd) + slen(change->data) + slen(change->key) + slen(change->updated) + 4);

    writeSize(len);
    writeBlock(change->cmd);
    writeBlock(change->data);
    writeBlock(change->key);
    writeBlock(change->updated);
    fflush(ioto->syncLog);
    rFlushFile(fileno(ioto->syncLog));

    ioto->syncSize += len;
}

/*
    Write a string including the trailing null
 */
static int writeBlock(cchar *buf)
{
    int len;

    len = (int) slen(buf) + 1;

    if (fwrite(&len, sizeof(len), 1, ioto->syncLog) != 1) {
        rError("sync", "Cannot write to sync log");
        return R_ERR_CANT_WRITE;
    }
    if (fwrite(buf, len, 1, ioto->syncLog) != 1) {
        rError("sync", "Cannot write to sync log");
        return R_ERR_CANT_WRITE;
    }
    return 0;
}

static int writeSize(int len)
{
    if (fwrite(&len, sizeof(len), 1, ioto->syncLog) != 1) {
        rError("sync", "Cannot write to sync log");
        return R_ERR_CANT_WRITE;
    }
    return 0;
}

/*
    Schedule sync when sufficient changes or a change due
 */
static void scheduleSync(Change *change)
{
    Ticks delay, now;

    if (!ioto->connected) {
        ioScheduleConnect();
        return;
    }
    /*
        Changes come via db callback and set change->due to now
        Sync retransmits set change->due +5 secs
     */
    now = rGetTicks();
    if (change->due < ioto->syncDue) {
        ioto->syncDue = change->due;
        if (ioto->syncEvent) {
            rStopEvent(ioto->syncEvent);
            ioto->syncEvent = 0;
        }
    }
    if (ioto->syncSize >= ioto->maxSyncSize) {
        ioFlushSync(0);

    } else if (rGetHashLength(ioto->syncHash) > 0 && !ioto->syncEvent) {
        delay = (ioto->syncDue > now) ? ioto->syncDue - now : 0;
        ioto->syncDue = now + delay;
        ioto->syncEvent = rStartEvent((REventProc) ioFlushSync, 0, delay);
    }
}

/*
    Publish changes to cloud
 */
PUBLIC void ioFlushSync(bool force)
{
    RName  *np;
    RBuf   *buf;
    Ticks  now, nextDue;
    Change *change;
    ssize  len;
    int    count, pending, seq;

    now = rGetTicks();
    buf = 0;
    count = 0;
    pending = 0;
    nextDue = now + (60 * TPS);

    if (rGetHashLength(ioto->syncHash) > 0) {
        rTrace("sync", "Applying sync log with %d changes", rGetHashLength(ioto->syncHash));
    }
    for (ITERATE_NAME_DATA(ioto->syncHash, np, change)) {
        if (force || change->due <= now) {
            if (buf == 0) {
                buf = rAllocBuf(ME_BUFSIZE);
                seq = nextSeq++;
                rPutToBuf(buf, "{\"seq\":%d,\"changes\":[", seq);
            }
            len = slen(change->cmd) + slen(change->key) + slen(change->data) + 27;
            if (rGetBufLength(buf) + len > (IO_MESSAGE_SIZE - 1024)) {
                nextDue = now;
                break;
            }
            rPutToBuf(buf, "{\"cmd\":\"%s\",\"key\":\"%s\",\"item\":%s},", change->cmd, change->key, change->data);
            change->seq = seq;
            //  Set the delay to +5 secs to give time for sync to be received before retransmit
            change->due += SYNC_DELAY;
            count++;
        } else {
            pending++;
            rDebug("sync", "Change due in %d msecs, %s", (int) (change->due - now), change->key);
        }
        nextDue = min(nextDue, change->due);
    }
    ioto->syncEvent = 0;
    ioto->syncSize = 0;
    ioto->syncDue = nextDue;

    if (!buf) {
        return;
    }
    rAdjustBufEnd(buf, -1);
    rPutStringToBuf(buf, "]}");

    //  Pending changes are buffered and not yet due to be sent
    rTrace("sync", "Sending %d sync changes to the cloud, %d changes pending", count, pending);

    mqttPublish(ioto->mqtt, rBufToString(buf), -1, 1, force ? MQTT_WAIT_ACK : MQTT_WAIT_NONE,
                "$aws/rules/IotoDevice/ioto/service/%s/db/syncToDynamo", ioto->id);
    rFreeBuf(buf);

#if KEEP
    //  Alternate wait
    if (force) {
        Ticks deadline = rGetTicks() + 5 * TPS;
        while (rGetTicks() < deadline) {
            if ((count = mqttMsgsToSend(ioto->mqtt)) == 0) {
                break;
            }
            rSleep(20);
        }
    }
#endif
}

/*
    Remove changes that have been replicated to the cloud.
    Changes are acknowledged by sequence number.
 */
static void cleanChanges(Json *json)
{
    Change   *change;
    JsonNode *key, *keys;
    ssize    count;
    int      kid, seq;

    keys = jsonGetNode(json, 0, "keys");
    seq = jsonGetInt(json, 0, "seq", 0);
    if (!keys) {
        return;
    }
    count = rGetHashLength(ioto->syncHash);

    for (ITERATE_JSON(json, keys, key, kid)) {
        change = rLookupName(ioto->syncHash, key->value);
        if (change) {
            if (change->seq == seq) {
                freeChange(change);
            }
        }
    }
    if (count && rGetHashLength(ioto->syncHash) == 0) {
        recreateSyncLog();
    }
}

static void recreateSyncLog(void)
{
    char path[ME_MAX_FNAME];

    if (ioto->nosave) return;

    SFMT(path, "%s.sync", ioto->db->path);

    if (ioto->syncLog) {
        fclose(ioto->syncLog);
        ioto->syncLog = 0;
    }
    if ((ioto->syncLog = fopen(path, "w")) == NULL) {
        rError("sync", "Cannot open sync log '%s'", path);
    }
}

/*
    When connected to the cloud, subscribe for incoming sync changes.
    Also fetch database updates made in the cloud since the last sync down from the cloud.
    And then send pending changes to the cloud.
 */
PUBLIC void ioConnectSync(void)
{
    if (!ioto->syncService) return;

    dbAddCallback(ioto->db,  (DbCallbackProc) deviceCommand, "Command", NULL, DB_ON_CHANGE);

    //  The "+" matches the sync command: INSERT, REMOVE, UPSERT, SYNC (responses)
    mqttSubscribe(ioto->mqtt, receiveSync, 1, MQTT_WAIT_NONE, "ioto/device/%s/sync/+", ioto->id);
    mqttSubscribe(ioto->mqtt, receiveSync, 1, MQTT_WAIT_NONE, "ioto/account/all/sync/+");
    mqttSubscribe(ioto->mqtt, receiveSync, 1, MQTT_WAIT_NONE, "ioto/account/%s/#", ioto->account);

    /*
        Apply prior changes that have been made locally but not yet applied to the cloud
     */
    applySyncLog();

    //  Sync from cloud to device - non-blocking
    ioSyncDown(-1);
}

/*
    Receive sync down responses
 */
static void receiveSync(MqttRecv *rp)
{
    Db           *db;
    const DbItem *prior;
    cchar        *msg, *priorUpdated, *sk, *updated, *lastSyncDown;
    Json         *json;
    bool         stale;

    db = ioto->db;
    msg = rp->data;

    if ((json = jsonParse(msg, 0)) == 0) {
        rError("sync", "Cannot parse sync message: %s for %s", msg, rp->topic);
        return;
    }
    if (rEmitLog("debug", "sync")) {
        rDebug("sync", "Received sync response %s: %s", rp->topic, msg);
    } else {
        rInfo("sync", "Received %s", rp->topic);
    }
    if (sends(rp->topic, "SYNC")) {
        //  Response for a sync
        cleanChanges(json);
        rSignal("db:sync:done", 0);

    } else if (sends(rp->topic, "SYNCUP") || sends(rp->topic, "SYNCDOWN")) {
        //  DEPRECATED SYNCUP
        //  Response for syncdown (ioConnectSync)
        updated = jsonGet(json, 0, "updated", 0);
        lastSyncDown = dbGetField(ioto->db, "SyncState", "lastSyncDown", NULL, NULL);
        if (!lastSyncDown || scmp(lastSyncDown, updated) < 0) {
            rInfo("sync", "SyncDown complete for %s", updated);
            dbUpdate(ioto->db, "SyncState", DB_PROPS("lastSyncDown", updated), DB_PARAMS(.bypass = 1));
            //  DEPRECATED syncup event
            rSignal("db:syncup:done", 0);
            rSignal("db:syncdown:done", 0);
        }

    } else {
        sk = jsonGet(json, 0, "sk", "");
        prior = dbGet(db, NULL, DB_PROPS("sk", sk), NULL);
        stale = 0;
        if (prior) {
            updated = jsonGet(json, 0, "updated", 0);
            priorUpdated = dbField(prior, "updated");
            if (updated && priorUpdated) {
                if (scmp(updated, priorUpdated) < 0) {
                    rTrace("sync", "Discard stale sync update");
                    stale = 1;
                }
            }
        }
        if (!stale) {
            rInfo("db:sync", "Received database %s", rp->topic);
            if (sends(rp->topic, "REMOVE")) {
                dbRemove(db, NULL, json, DB_PARAMS(.bypass = 1));

            } else if (sends(rp->topic, "INSERT")) {
                dbCreate(db, NULL, json, DB_PARAMS(.bypass = 1));

            } else if (sends(rp->topic, "UPSERT") || sends(rp->topic, "MODIFY")) {
                dbUpdate(db, NULL, json, DB_PARAMS(.bypass = 1, .upsert = 1));

            } else {
                rError("db", "Bad sync topic %s", rp->topic);
            }
        }
        rSignal("db:sync", json);
    }
    jsonFree(json);
}


/*
    Watch updates to the command table
 */
static void deviceCommand(void *arg, struct Db *db, struct DbModel *model,
                          struct DbItem *item, struct DbParams *params, cchar *cmd, int event)
{
    if (event & DB_ON_CHANGE) {
        if (smatch(cmd, "create") || smatch(cmd, "upsert") || smatch(cmd, "update")) {
            processDeviceCommand(item);
        }
    }
}


/*
    Act on standard device commands
 */
static void processDeviceCommand(DbItem *item)
{
    cchar *cmd;
    char  *name;

    cmd = dbField(item, "command");

    rInfo("ioto", "Got device command %s\nData: %s", cmd, jsonString(dbJson(item)));
    if (smatch(cmd, "reboot")) {
        rSetState(R_RESTART);
#if SERVICES_PROVISION
    } else if (smatch(cmd, "release")) {
        ioDeprovision();
    } else if (smatch(cmd, "reprovision")) {
        ioDeprovision();
        ioProvision();
#endif
#if SERVICES_UPDATE
    } else if (smatch(cmd, "update")) {
        ioUpdate();
#endif
    } else {
        //  Signal other command watchers
        name = sfmt("device:command:%s", cmd);
        rSignalSync(name, item);
        rFree(name);
    }
}

#else
void dummySync(void)
{
}
#endif /* SERVICES_SYNC */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/cloud/update.c ************/

/*
    update.c - Check for software updates

    Update requires a device cloud and device registration but not provisioning.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



#if SERVICES_UPDATE

/*********************************** Forwards *********************************/

static void applyUpdate(char *path);
static bool checkSum(cchar *path, cchar *checksum);
static int download(cchar *url, cchar *path);

/************************************* Code ***********************************/
/*
    Check for updates with the device cloud.
    This maintains management for this device and checks for updates.
 */
PUBLIC bool ioUpdate(void)
{
    Json  *json;
    Url   *up;
    Time  delay, jitter, lastUpdate, period;
    cchar *apply, *checksum, *image, *response, *schedule, *version;
    char  *body, *date, *headers, *path, url[160];

    /*
        Protection incase update fails and device loops continually updating
     */
    schedule = jsonGet(ioto->config, 0, "update.schedule", "* * * * *");
    jitter = svalue(jsonGet(ioto->config, 0, "update.jitter", "0")) * TPS;
    period = svalue(jsonGet(ioto->config, 0, "update.period", "24 hrs")) * TPS;

    lastUpdate = (Time) rParseIsoDate(dbGetField(ioto->db, "SyncState", "lastUpdate", NULL, NULL));
    delay = lastUpdate + period - rGetTime();
    if (delay < 0) {
        delay = cronUntil(schedule, rGetTime());
    }
    if (!ioto->api && delay <= 0) {
        //  Not yet provisioned
        delay = 60 * TPS;
    }
    if (delay > 0) {
        if (jitter) {
            jitter = rand() % jitter;
            delay += jitter;
        }
        rStartEvent((REventProc) ioUpdate, 0, delay);
        return 0;
    }
    json = jsonAlloc(0);
    jsonBlend(json, 0, 0, ioto->config, 0, "device", 0);
    jsonSet(json, 0, "version", ioto->version, JSON_STRING);
    jsonSet(json, 0, "iotoVersion", ME_VERSION, JSON_STRING);
    body = jsonToString(json, 0, 0, JSON_QUOTES);
    jsonFree(json);

    SFMT(url, "%s/tok/provision/update", ioto->api);
    rTrace("update", "Builder at %s", ioto->api);

    up = urlAlloc(0);
    urlSetTimeout(up, svalue(jsonGet(ioto->config, 0, "timeouts.api", "30 secs")) * TPS);

    headers = sfmt("Authorization: bearer %s\r\nContent-Type: application/json\r\n", ioto->apiToken);
    rTrace("update", "Request \n%s\n%s\n%s\n\n", url, headers, body);
    if ((json = urlJson(up, "POST", url, body, -1, headers)) == 0) {
        response = urlGetResponse(up);
        rInfo("update", "\nResponse %s\n", response);
        if (smatch(response, "Cannot find device") || smatch(response, "Authentication failed")) {
            /*
                The device has either been removed or released. Release certs and re-provision after a restart
             */
            rInfo("update", "%s: releasing device ...", response);
            ioDeprovision();
            rSetState(R_RESTART);
        } else {
            //  Transitory error
            rError("update", "Cannot connect to device cloud");
        }
    }
    rFree(headers);
    rFree(body);
    urlFree(up);

    if (json && json->count > 1) {
        /*
            Got an update response with checksum, version and image url
         */
        checksum = jsonGet(json, 0, "checksum", 0);
        version = jsonGet(json, 0, "version", 0);
        image = jsonGet(json, 0, "url", 0);
        path = rGetFilePath("@state/update.bin");
        rInfo("update", "Device has updated firmware: %s", version);

        /*
            Download the update with throttling
         */
        if (download(image, path) == 0) {
            //  Validate
            if (!checkSum(path, checksum)) {
                rError("provision", "Checksum does not match for update image %s: %s", path, checksum);
            } else {
                //  Delayed application -- perhaps till off hours
                apply = jsonGet(ioto->config, 0, "update.apply", "* * * * *");
                delay = cronUntil(apply, rGetTime());
                rStartEvent((REventProc) applyUpdate, sclone(path), delay);
            }
        }
        rFree(path);

    } else if (json) {
        rInfo("update", "Device has no pending updates for version: %s", ioto->version);
    }
    jsonFree(json);

    date = rGetIsoDate(rGetTime());
    dbUpdate(ioto->db, "SyncState", DB_PROPS("lastUpdate", date), DB_PARAMS(.upsert = 1));

    delay = cronUntil(schedule, rGetTime() + period + jitter);
    rStartEvent((REventProc) ioUpdate, 0, delay);
    return json ? 1 : 0;
}

/*
    Apply the update by invoking the "scripts.update" script
    This may exit or reboot if instructed by the update script
    NOTE: This routine frees the path
 */
static void applyUpdate(char *path)
{
#if ME_UNIX_LIKE
    cchar *script;
    char  command[ME_MAX_FNAME + 32], *output;
    int   fd, status;

    //  Need hook/way for apps to prevent update
    rSignal("device:update", path);

    if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600)) < 0) {
        rError("provision", "Cannot open image temp file");
        rFree(path);
        return;
    }
    script = jsonGet(ioto->config, 0, "scripts.update", 0);
    if (script) {
        SFMT(command, "%s \"%s\"", script, path);
        output = rRun(command, &status);
        rInfo("update", "Update returned status %d, output: %s", status, output);

        if (status != 0) {
            rError("update", "Update command failed: %s", output);
        } else {
            if (smatch(output, "exit\n")) {
                rGracefulStop();
            } else if (smatch(output, "restart\n")) {
                rSetState(R_RESTART);
            }
        }
        rFree(output);
    }
    unlink(path);
    rFree(path);
    close(fd);
#endif
}

/*
    Download a software update
 */
static int download(cchar *url, cchar *path)
{
    Url   *up;
    ssize total;
    int   fd, nbytes, throttle;
    char  *buf;

    if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600)) < 0) {
        rError("provision", "Cannot open image temp file");
        return R_ERR_CANT_OPEN;
    }
    up = urlAlloc(0);

    // If throttling, the download timeout may need to be increased
    urlSetTimeout(up, svalue(jsonGet(ioto->config, 0, "timeouts.download", "4 hrs")) * TPS);

    if (urlStart(up, "GET", url, -1) < 0 || urlWrite(up, NULL, 0) < 0 || urlGetStatus(up) != 200) {
        rError("update", "Cannot fetch %s\n%s", url, urlGetResponse(up));
        urlFree(up);
        return R_ERR_CANT_READ;
    }
    total = 0;
    throttle = (int) jsonGetNum(ioto->config, 0, "update.throttle", 0);
    if ((buf = rAlloc(ME_BUFSIZE)) == 0) {
        return R_ERR_CANT_ALLOCATE;
    }
    do {
        if ((nbytes = (int) urlRead(up, buf, ME_BUFSIZE)) < 0) {
            rError("update", "Cannot read response");
            urlFree(up);
            rFree(buf);
            return R_ERR_CANT_READ;
        }
        if (write(fd, buf, nbytes) < nbytes) {
            rError("update", "Cannot save response");
            urlFree(up);
            rFree(buf);
            return R_ERR_CANT_WRITE;
        }
        total += nbytes;
        if (throttle) {
            throttle = min(throttle, 5 * TPS);
            if (throttle > 0) {
                rSleep(throttle);
            }
        }
    } while (nbytes > 0);
    rFree(buf);
    rInfo("update", "Downloaded %d bytes", (int) total);

    urlFree(up);
    close(fd);
    return 0;
}

static bool checkSum(cchar *path, cchar *checksum)
{
    char *hash;
    int  match;

    hash = cryptGetFileSha256(path);
    match = smatch(hash, checksum);
    rFree(hash);
    return match;
}

#else
void dummyUpdate(void)
{
}
#endif /* SERVICES_UPDATE */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
