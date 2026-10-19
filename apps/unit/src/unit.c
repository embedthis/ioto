/*
    unit.c -- Support for unit tests
 */

/********************************** Includes **********************************/

#include "ioto.h"

#include <fcntl.h>
#if MACOSX
#include <mach/mach.h>
#endif

#define UNIT_TIMEOUT (2 * 60 * TPS)

/************************************ Forwards ********************************/

static void unitTestAction(Web *web);
static cchar *getParam(Web *web, cchar *name, cchar *defaultValue);
static void testTimeout(void);
#if SERVICES_DATABASE
static int dbUpdateTest(void);
#endif
#if SERVICES_SYNC
static int storeSyncTest(void);
#endif
#if SERVICES_MQTT
static int metricTest(void);
static int storeMqttGetTest(void);
static int storeMqttSetTest(void);
static int mqttPingTest(void);
static int mqttRequestTest(void);
#endif
#if SERVICES_SHADOW
static int shadowTest(void);
#endif
#if SERVICES_URL
static int urlTest(void);
#endif
#if SERVICES_WEB
static int webStreamTest(void);
#endif
#if SERVICES_KEYS
static int awsCloudWatch(void);
#endif

static int healthCheck(void);
static int invokeTest(cchar *name);

/*********************************** Code *************************************/
/*
    Scenario fixtures for test/soak and test/stress. They exist so those groups can reach the
    upload filter, the WebSocket upgrade, the database and the session table through the
    assembled agent. Every reply contains only what the server itself produced, and nothing is
    written to disk under a name taken from a request.
 */
#if SERVICES_WEB
/*
    Report the authenticated user. Reached through a route that requires the "user" role, so it
    only answers a request carrying a live session.
 */
static void whoamiAction(Web *web)
{
    webWriteFmt(web, "{\"username\":\"%s\",\"role\":\"%s\"}", web->username ? web->username : "",
                web->role ? web->role : "");
    webFinalize(web);
}

/*
    Resident set of this process in KB, as the operating system accounts it - the figure ps
    prints. Zero where the platform is not supported, and a soak skips rather than assert on it.
 */
static int64 residentKb(void)
{
#if MACOSX
    struct mach_task_basic_info info;
    mach_msg_type_number_t      count = MACH_TASK_BASIC_INFO_COUNT;

    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &info, &count) == KERN_SUCCESS) {
        return (int64) (info.resident_size / 1024);
    }
#elif LINUX
    FILE *fp;
    long pages;

    if ((fp = fopen("/proc/self/statm", "r")) != 0) {
        if (fscanf(fp, "%*s %ld", &pages) == 1) {
            fclose(fp);
            return (int64) pages * sysconf(_SC_PAGESIZE) / 1024;
        }
        fclose(fp);
    }
#endif
    return 0;
}

/*
    Open descriptors in this process. A descriptor never closed is the leak that takes a device
    down after a week, and it is invisible to the allocator.
 */
static int openDescriptors(void)
{
#if ME_UNIX_LIKE
    int fd, count, max;

    max = (int) sysconf(_SC_OPEN_MAX);
    if (max <= 0 || max > 65536) {
        max = 65536;
    }
    for (fd = 0, count = 0; fd < max; fd++) {
        if (fcntl(fd, F_GETFD) != -1) {
            count++;
        }
    }
    return count;
#else
    return 0;
#endif
}

/*
    Report the operating system's view of this process and the live session count, so a soak
    can sample them either side of its workload without ps, lsof or a pid.
 */
static void statsAction(Web *web)
{
    webWriteFmt(web, "{\"rss\":%lld,\"fds\":%d,\"sessions\":%d}", (long long) residentKb(), openDescriptors(),
                rGetHashLength(web->host->sessions));
    webFinalize(web);
}

#if ME_WEB_UPLOAD
/*
    Report what the upload filter staged: one line per file, then one per form variable
 */
static void uploadAction(Web *web)
{
    WebUpload *file;
    RName     *np;
    JsonNode  *node;

    if (web->uploads) {
        for (ITERATE_NAME_DATA(web->uploads, np, file)) {
            webWriteFmt(web, "FILE %s %s %d\n", file->name, file->clientFilename, (int) file->size);
        }
    }
    if (web->vars) {
        for (ITERATE_JSON(web->vars, NULL, node, nid)) {
            webWriteFmt(web, "VAR %s=%s\n", node->name, node->value);
        }
    }
    webFinalize(web);
}
#endif

#if ME_COM_WEBSOCK
static void echoEvent(WebSocket *ws, int event, cchar *buf, size_t len, Web *web)
{
    (void) web;

    if (event == WS_EVENT_MESSAGE) {
        webSocketSendBlock(ws, ws->type, buf, len);
    }
}

/*
    Echo each WebSocket message back with its original type
 */
static void echoAction(Web *web)
{
    /*
        "upgraded", not "upgrade". web->upgrade is the Upgrade header's value, so it is non-null for
        any Upgrade at all - "h2c", "x", or an empty one - and handleRequest performs the upgrade
        only for "websocket". The guard therefore passed with web->webSocket still null, and
        webSocketRun stores through it. web->upgraded is set only after the upgrade completes.
     */
    if (!web->upgraded) {
        webError(web, 400, "Connection not upgraded to WebSocket");
        return;
    }
    webSocketRun(web->webSocket, (WebSocketProc) echoEvent, web, web->rx, web->host->inactivityTimeout);
}
#endif

#if SERVICES_DATABASE
#define CHURN_KEYS 100

/*
    Upsert one Churn item under a key that cycles through a fixed set, read it back, and report
    how many items the model holds. Repeated calls keep the table at CHURN_KEYS items while the
    journal takes every write.
 */
static void churnAction(Web *web)
{
    static int counter = 0;
    RList      *items;
    char       key[32], value[32];
    int        count;

    SFMT(key, "k%d", counter % CHURN_KEYS);
    SFMT(value, "%d", counter++);
    if (!dbUpdate(ioto->db, "Churn", DB_PROPS("key", key, "value", value), DB_PARAMS(.upsert = 1))) {
        webWriteResponse(web, 500, "Cannot update Churn: %s", dbGetError(ioto->db));
        return;
    }
    if (!dbFindOne(ioto->db, "Churn", DB_PROPS("key", key), NULL)) {
        webWriteResponse(web, 500, "Cannot read back Churn %s", key);
        return;
    }
    items = dbFind(ioto->db, "Churn", NULL, NULL);
    count = items ? rGetListLength(items) : 0;
    rFreeList(items);
    webWriteFmt(web, "{\"count\":%d}", count);
    webFinalize(web);
}

/*
    Change the User table the way a device's user-management UI would: create the named user, change its
    role or password hash, or remove it with "remove=1". Registered for the admin role only.
 */
static void userAction(Web *web)
{
    const DbItem *item;
    cchar        *username, *password, *role;
    bool         ok;

    if ((username = getParam(web, "username", NULL)) == 0) {
        webWriteResponse(web, 400, "Missing username");
        return;
    }
    item = dbFindOne(ioto->db, "User", DB_PROPS("username", username), NULL);
    role = getParam(web, "role", item ? dbField(item, "role") : "user");
    password = getParam(web, "password", item ? dbField(item, "password") : "");

    if (smatch(getParam(web, "remove", "0"), "1")) {
        ok = item && dbRemove(ioto->db, "User", DB_PROPS("id", dbField(item, "id")), NULL) == 1;
    } else if (item) {
        ok = dbUpdate(ioto->db, "User",
                      DB_PROPS("id", dbField(item, "id"), "username", username, "role", role, "password", password),
                      NULL) != 0;
    } else {
        ok = dbCreate(ioto->db, "User", DB_PROPS("username", username, "role", role, "password", password),
                      NULL) != 0;
    }
    webWriteResponse(web, ok ? 200 : 400, ok ? "OK" : "Cannot change user");
}

#define BLOB_COUNT 3
#define BLOB_SIZE  (200 * 1024)

/*
    Large fields force writes to yield so another request can serialize the same item.
 */
static bool initBlobs(void)
{
    char *data, key[32];
    int  i;

    if (dbFindOne(ioto->db, "Blob", NULL, NULL)) {
        return 1;
    }
    data = rAlloc(BLOB_SIZE + 1);
    memset(data, 'x', BLOB_SIZE);
    data[BLOB_SIZE] = '\0';
    for (i = 0; i < BLOB_COUNT; i++) {
        if (!dbCreate(ioto->db, "Blob", DB_PROPS("key", SFMT(key, "b%d", i), "data", data), NULL)) {
            rFree(data);
            return 0;
        }
    }
    rFree(data);
    return 1;
}


static void itemAction(Web *web)
{
    const DbItem *item;

    if (!initBlobs() || (item = dbFindOne(ioto->db, "Blob", DB_PROPS("key", "b0"), NULL)) == 0) {
        webWriteResponse(web, 500, "Cannot read Blob: %s", dbGetError(ioto->db));
        return;
    }
    webWriteItem(web, item);
    webFinalize(web);
}


static void itemsAction(Web *web)
{
    RList *items;

    if (!initBlobs() || (items = dbFind(ioto->db, "Blob", NULL, NULL)) == 0) {
        webWriteResponse(web, 500, "Cannot read Blob: %s", dbGetError(ioto->db));
        return;
    }
    webWriteItems(web, items);
    webFinalize(web);
    rFreeList(items);
}
#endif
#endif /* SERVICES_WEB */

/*
    App setup called when Ioto starts.
 */
PUBLIC int ioStart(void)
{
#if SERVICES_DATABASE
    char *path;

    //  Seed the test users the login fixtures authenticate against
    if (!dbFindOne(ioto->db, "User", NULL, NULL)) {
        path = rGetFilePath("@config/db.json5");
        if (rFileExists(path)) {
            rInfo("app", "Load db.json5");
            dbLoadData(ioto->db, path);
        }
        rFree(path);
    }
#endif
    webAddAction(ioto->webHost, "/api/test", unitTestAction, NULL);
#if SERVICES_WEB
    webAddAction(ioto->webHost, "/api/user/whoami", whoamiAction, NULL);
    webAddAction(ioto->webHost, "/api/public/stats", statsAction, NULL);
#if ME_WEB_UPLOAD
    webAddAction(ioto->webHost, "/api/public/upload", uploadAction, NULL);
#endif
#if ME_COM_WEBSOCK
    webAddAction(ioto->webHost, "/api/public/echo", echoAction, NULL);
#endif
#if SERVICES_DATABASE
    webAddAction(ioto->webHost, "/api/public/churn", churnAction, NULL);
    webAddAction(ioto->webHost, "/api/public/item", itemAction, NULL);
    webAddAction(ioto->webHost, "/api/public/items", itemsAction, NULL);
    webAddAction(ioto->webHost, "/api/admin/user", userAction, "admin");
#endif
#endif
    return 0;
}

/*
    This is called when Ioto is shutting down
 */
PUBLIC void ioStop(void)
{
}

/*
    Read a body parameter, falling back to the query string.
 */
static cchar *getParam(Web *web, cchar *name, cchar *defaultValue)
{
    cchar *value;

    value = webGetVar(web, name, 0);
    if (!value) {
        value = webGetQueryVar(web, name, 0);
    }
    return value ? value : defaultValue;
}

/*
    Web action handler for test requests
 */
static void unitTestAction(Web *web)
{
    REvent timeoutEvent;
    cchar  *name;
    int64  delay, i, count;
    bool   exit;

    name = getParam(web, "name", 0);
    count = stoi(getParam(web, "count", "1"));
    delay = stoi(getParam(web, "delay", "0"));
    exit = stoi(getParam(web, "exit", "0"));

    rInfo("test", "Running %s test", name);

    timeoutEvent = 0;
    if (rGetTimeouts()) {
        timeoutEvent = rStartEvent((REventProc) testTimeout, 0, UNIT_TIMEOUT);
    }
    for (i = 0; i < count; i++) {
        if (invokeTest(name) < 0) {
            webWriteResponse(web, 500, "✗ Test %s failed\n", name);
            break;
        } else {
            webWriteFmt(web, "✓ Test %s passed\n", name);
        }
        if (delay) {
            rSleep(delay * TPS);
        }
        if (count > 1) {
            rInfo("test", "Iteration %lld", i + 1);
        }
    }
    rStopEvent(timeoutEvent);
    webFinalize(web);
    if (exit) {
        rSignal("test:complete");
        rStop();
    }
}

/*
    Switch on test name
 */
static int invokeTest(cchar *name)
{
    if (smatch(name, "health")) {
        return healthCheck();
    } else if (smatch(name, "db.update")) {
#if SERVICES_DATABASE
        return dbUpdateTest();
#endif
    } else if (smatch(name, "sync.store")) {
#if SERVICES_SYNC
        return storeSyncTest();
#endif
    } else if (smatch(name, "metric.getset")) {
#if SERVICES_MQTT
        return metricTest();
#endif
    } else if (smatch(name, "mqtt.store")) {
#if SERVICES_MQTT
        if (storeMqttSetTest() < 0) {
            return R_ERR_CANT_COMPLETE;
        }
        return storeMqttGetTest();
#endif
    } else if (smatch(name, "mqtt.ping")) {
#if SERVICES_MQTT
        return mqttPingTest();
#endif
    } else if (smatch(name, "mqtt.request")) {
#if SERVICES_MQTT
        return mqttRequestTest();
#endif
    } else if (smatch(name, "url.get")) {
#if SERVICES_URL
        return urlTest();
#endif
    } else if (smatch(name, "shadow.basic")) {
#if SERVICES_SHADOW
        return shadowTest();
#endif
    } else if (smatch(name, "web.stream")) {
#if SERVICES_WEB
        return webStreamTest();
#endif
    } else if (smatch(name, "aws.logs")) {
#if SERVICES_KEYS
        return awsCloudWatch();
#endif
    }
    rError("test", "Unknown test: %s", name);
    return R_ERR_CANT_COMPLETE;
}

static void testTimeout(void)
{
    rError("test", "Test failed due to timeout");
    rSignal("test:complete");
    rStop();
}

static int healthCheck(void)
{
    rInfo("test", "Run test: health");
    return R_ERR_OK;
}

#if SERVICES_DATABASE
/*
    Test the database by performing a simple update to the State entity
 */
static int dbUpdateTest(void)
{
    cchar *lastUpdate;

    rInfo("test", "Run test: dbLocal");
    if (!ioto->db) {
        rError("test", "Database not initialized");
        return R_ERR_CANT_COMPLETE;
    }
    /*
        Update the lastUpdate field in the SyncState entity.
        This table is local to the device and is not synchronized to the cloud.
     */
    lastUpdate = dbGetField(ioto->db, "SyncState", "lastUpdate", NULL, NULL);
    rInfo("test", "dbUpdate: lastUpdate=%s", lastUpdate ? lastUpdate : "NULL");
    if (dbUpdate(ioto->db, "SyncState", DB_PROPS("lastUpdate", lastUpdate), DB_PARAMS(.upsert = 1)) == 0) {
        rError("test", "Cannot update SyncState: %s", dbGetError(ioto->db));
        return R_ERR_CANT_COMPLETE;
    }
    return R_ERR_OK;
}
#endif

#if SERVICES_MQTT
static int metricTest(void)
{
    double value;

    // SECURITY Acceptable: rand is used for the metric test
    value = ((double) rand() / RAND_MAX) * 10;
    if (ioSetMetric("metric-test", value, "", 0) < 0) {
        return R_ERR_CANT_COMPLETE;
    }

    //  Allow metric set above to take
    rSleep(2 * TPS);
    value = ioGetMetric("metric-test", "", "sum", 3600);
    rInfo("test", "Got metric value %lf", value);

    //  Issue a metric with explicit dimensions that is aggregated for all devices
    // SECURITY Acceptable: rand is used for the metric test
    value = ((double) rand() / RAND_MAX) * 10;
    if (ioSetMetric("metric-test-fleet", value, "[{}]", 0) < 0) {
        return R_ERR_CANT_COMPLETE;
    }
    return R_ERR_OK;
}

/*
    Test the cloud Store. This sends update to the Store using MQTT.
 */
static int storeMqttSetTest(void)
{
    static int counter = 0;

    rInfo("test", "Run test: store-mqtt: set counter %d", counter++);
    if (ioSetNum("counter", counter) < 0) {
        return R_ERR_CANT_COMPLETE;
    }
    return R_ERR_OK;
}

static int storeMqttGetTest(void)
{
    double num;

    rSleep(500);
    num = ioGetNum("counter");
    rInfo("test", "store-mqtt: get result %lf", num);
    return R_ERR_OK;
}

#if FUTURE
/*
    Set a value in the cloud Store and trigger metric creation
 */
static int mqttWithMetrics(void)
{
    char       metrics[160], msg[160];
    static int counter = 0;

    rInfo("test", "Run test: store with Metrics");

    SFMT(metrics, "{\"metrics\":{" \
         "\"namespace\":\"Embedthis/Device\"," \
         "\"fields\":[{\"COUNTER\": 'value'}]," \
         "\"dimensions\":[{Device: 'deviceId'}]" \
         "}}");
    SFMT(msg, "{\"key\":\"%s\",\"value\":%d,\"type\":\"number\"}", "counter", counter++);
    return mqttPublish(ioto->mqtt, msg, -1, 1, MQTT_WAIT_NONE,
                       "$aws/rules/IotoDevice/ioto/service/%s/store/set", ioto->id);
}
#endif

/*
    Send a MQTT request ping. This is fire and forget. The server side does nothing.
 */
static int mqttPingTest(void)
{
    rInfo("test", "Run test: mqttPing");
    return mqttPublish(ioto->mqtt, "", 0, 1, MQTT_WAIT_NONE, "ioto/service/%s/test/ping", ioto->id);
}

/*
    Test the mqttRequest API. This sends a request and waits for a reponse.
 */
static int mqttRequestTest(void)
{
    char *result;

    rInfo("test", "Run test: mqttRequest");
    result = mqttRequest(ioto->mqtt, "{\"key\":\"counter\"}", 0, "store/get");
    if (result == NULL) {
        return R_ERR_CANT_COMPLETE;
    }
    rFree(result);
    return R_ERR_OK;
}
#endif

#if SERVICES_SYNC
/*
    Callback that is invoked when the sync to the cloud has completed.
    This is normally not required. We use this to serialize test results.
 */
static void syncResponse(RFiber *fiber)
{
    rWatchOff("db:sync:done", (RWatchProc) syncResponse, fiber);
    rResumeFiber(fiber, 0);
}

/*
    Test updates to the cloud Store via database sync.
    This writes to the local database which is then transparently synchronized to the cloud.
    We then use a completion syncResponse function which is invoked when the "db:sync:done" event fires.
 */
static int storeSyncTest(void)
{
    CDbItem    *item;
    static int counter = 0;

    /*
        Flush pending sync changes to ensure a clean slate
     */
    ioFlushSync(1);
    rSleep(5 * TPS);

    rInfo("test", "Run test: store-sync: counter %d", counter++);
    /*
        Normally database sync updates are fire and forget.
        Just for testing, we wait for a sync response so we can block for this test iteration
     */
    rWatch("db:sync:done", (RWatchProc) syncResponse, rGetFiber());

    /*
        Update the database locally which will be transparently sync'd to the cloud
        The .delay forces the change to be flushed to the cloud immediately.
     */
    if ((item = dbUpdate(ioto->db, "Store",
                         DB_JSON("{key: 'counter', value: '%d', type: 'number'}", counter),
                         DB_PARAMS(.upsert = 1, .delay = 0))) == 0) {
        rError("provision", "Cannot update Value item in database: %s", dbGetError(ioto->db));
        return R_ERR_CANT_COMPLETE;
    }
    //  Wait for a response (just for testing so we can block for this test iteration)
    rYieldFiber(0);
    return R_ERR_OK;
}
#endif

#if SERVICES_KEYS
static int awsCloudWatch(void)
{
    Url   *up;
    cchar *region;
    char  *data;

    region = "ap-northeast-1";
    up = urlAlloc(0);

    data = sfmt("{\"logGroupName\":\"%s\"}", "test-45");
    if (aws(up, region, "logs", "Logs_20140328.CreateLogGroup", data, -1, NULL) < 0) {
        urlFree(up);
        rFree(data);
        return R_ERR_CANT_COMPLETE;
    }
    urlFree(up);
    rFree(data);

#if KEEP
    data = sfmt("{\"logGroupName\":\"%s\"}", "test-42");
    rc = aws(up, region, "logs", "Logs_20140328.CreateLogGroup", data, -1, NULL);
    urlFree(up);

    rc = awsPutToS3("ap-northeast-1", "ioto-test-43", "test.txt", "hello world\n", -1);
    print("RC %d", rc);

    rc = awsPutFileToS3(region, "ioto-test-43", "test.tmp", "a.tmp");
#endif
    return R_ERR_OK;
}
#endif

#if SERVICES_SHADOW
static int shadowTest(void)
{
    rInfo("test", "Run test: shadow");
    return R_ERR_OK;
}
#endif

#if SERVICES_URL
static int urlTest(void)
{
    rInfo("test", "Run test: url");
    return R_ERR_OK;
}
#endif


#if SERVICES_WEB
static int streamCount = 100;

/*
    Stream status back to the current http request
 */
static void streamStatus(Web *web)
{
    if (webWriteFmt(web, "{\"time\": %lld}\n", rGetTicks()) < 0) {
        rInfo("test", "Write status connection lost");
        rResumeFiber(web->fiber, 0);
    } else if (--streamCount <= 0) {
        rResumeFiber(web->fiber, 0);
    } else {
        rInfo("test", "Start stream status");
        rStartEvent((REventProc) streamStatus, web, TPS);
    }
}

/*
    Web server action routine to start streaming a response
 */
static void streamAction(Web *web)
{
    rStartEvent((REventProc) streamStatus, web, 0);
    //  Yield until complete
    rYieldFiber(0);
    rInfo("test", "Run test: web complete");
}

/*
    Web streaming request test. Thi sends 100 lines
 */
static int webStreamTest(void)
{
    rInfo("test", "Run test: webStream");
    streamCount = 100;
    webAddAction(ioto->webHost, "/api/public/stream", streamAction, NULL);
    return R_ERR_OK;
}

#endif

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

