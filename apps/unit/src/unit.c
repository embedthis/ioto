/*
    unit.c -- Support for unit tests
 */

/********************************** Includes **********************************/

#include "ioto.h"

#define UNIT_TIMEOUT (2 * 60 * TPS)

/************************************ Forwards ********************************/

static void unitTestAction(Web *web);
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
    App setup called when Ioto starts.
 */
PUBLIC int ioStart(void)
{
    webAddAction(ioto->webHost, "/api/test", unitTestAction, NULL);
    return 0;
}

/*
    This is called when Ioto is shutting down
 */
PUBLIC void ioStop(void)
{
}

/*
    Web action handler for test requests
 */
static void unitTestAction(Web *web)
{
    REvent   timeoutEvent;
    cchar    *name;
    int64    delay, i, count;
    bool     exit;

    name = webGetVar(web, "name", 0);
    if (!name) {
        name = webGetQueryVar(web, "name", 0);
    }
    count = stoi(webGetVar(web, "count", "1"));
    delay = stoi(webGetVar(web, "delay", "0"));
    exit = stoi(webGetVar(web, "exit", "0"));

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
    /*
        Update the lastUpdate field in the SyncState entity. 
        This table is local to the device and is not synchronized to the cloud.
     */
    lastUpdate = dbGetField(ioto->db, "SyncState", "lastUpdate", NULL, NULL);
    if (dbUpdate(ioto->db, "SyncState", DB_PROPS("lastUpdate", lastUpdate), NULL) == 0) {
        rError("provision", "Cannot update State: %s", dbGetError(ioto->db));
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
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

