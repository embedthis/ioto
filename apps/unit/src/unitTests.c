/*
    unit.c - Unit tests

    This file contains tests and examples that exercise various Ioto components.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

/*********************************** Locals ***********************************/

#define UNIT_TIMEOUT (2 * 60 * TPS)

/************************************ Forwards ********************************/

static void testTimeout(JsonNode *test);
#if SERVICES_DATABASE
static void dbLocalTest(Json *json);
#if SERVICES_SYNC
static void storeSyncTest(Json *json);
#endif
#endif
#if SERVICES_MQTT
static void metricTest(Json *json);
static void storeMqttGetTest(Json *json);
static void storeMqttSetTest(Json *json);
static void mqttPingTest(Json *json);
static void mqttRequestTest(Json *json);
#endif
#if SERVICES_SHADOW
static void shadowTest(Json *json);
#endif
#if SERVICES_URL
static void urlTest(Json *json);
#endif
#if SERVICES_WEB
static void webStreamTest(Json *json);
#endif
#if SERVICES_KEYS
static void s3Test(Json *json);
#endif

/************************************* Code ***********************************/
/*
    Main test harness. This is invoked if --test is supplied on the command line and is triggered
    when the "mqtt:connected" event is fired.
 */
PUBLIC void unitTest(cchar *suite)
{
    REvent   timeoutEvent;
    Json     *json;
    JsonNode *run, *test;
    char     *error, *path, sbuf[80];
    bool     exit, parallel;
    int      delay, i, count, sid;

    if (smatch(suite, "none")) {
        return;
    }
    path = rGetFilePath("@config/test.json5");
    if ((json = jsonParseFile(path, &error, 0)) == 0) {
        rError("test", "Cannot parse test.json5. Error %s", error);
        rFree(path);
        return;
    }
    rFree(path);

    if ((sid = jsonGetId(json, 0, SFMT(sbuf, "suites.%s", suite))) < 0) {
        rError("test", "Cannot find test suite '%s'", suite);
        return;
    }
    run = jsonGetNode(json, sid, "run");
    parallel = jsonGetBool(json, sid, "parallel", 0);
    delay = jsonGetInt(json, sid, "delay", 0);
    exit = jsonGetBool(json, sid, "exit", 1);

    count = ioto->cmdCount ? ioto->cmdCount : jsonGetInt(json, sid, "count", 1);
    timeoutEvent = 0;

    rInfo("test", "Running %d tests %s", count, parallel ? "in parallel" : "");

    jsonPrint(json);

    for (i = 0; i < count; i++) {
        rInfo("test", "Iteration %d", i);
        //  Iterate over the "run" list of tests
        for (ITERATE_JSON(json, run, test, nid)) {
            rInfo("test", "Running %s tests", test->value);
            if (rGetTimeouts()) {
                timeoutEvent = rStartEvent((REventProc) testTimeout, test, UNIT_TIMEOUT);
            }
#if SERVICES_DATABASE
#if SERVICES_SYNC
            if (smatch(test->value, "store-sync")) {
                storeSyncTest(json);
            } else
#endif
            if (smatch(test->value, "db-local")) {
                dbLocalTest(json);
            }
#endif
#if SERVICES_MQTT
            if (smatch(test->value, "metric-api")) {
                metricTest(json);
            } else if (smatch(test->value, "store-mqtt")) {
                storeMqttSetTest(json);
                storeMqttGetTest(json);
            } else if (smatch(test->value, "mqtt-ping")) {
                mqttPingTest(json);
            } else if (smatch(test->value, "mqtt-request")) {
                mqttRequestTest(json);
            }
#endif
#if SERVICES_URL
            if (smatch(test->value, "url")) {
                urlTest(json);
            }
#endif
#if SERVICES_SHADOW
            if (smatch(test->value, "shadow")) {
                shadowTest(json);
            }
#endif
#if SERVICES_WEB
            if (smatch(test->value, "stream")) {
                webStreamTest(json);
            }
#endif
#if SERVICES_KEYS
            if (smatch(test->value, "s3")) {
                s3Test(json);
            }
#endif
            if (smatch(test->value, "debug")) {
                // debugTest(json);
            }
            if (rGetTimeouts()) {
                rStopEvent(timeoutEvent);
            }
            if (delay) {
                rSleep(delay * TPS);
            }
        }
    }
    jsonFree(json);
    if (exit) {
        //  Wait a little to allow any residual cloud messages to be received (retransmits)
        rSleep(5 * TPS);
        rSignal("test:complete");
        rStop();
    }
}

static void testTimeout(JsonNode *test)
{
    rError("test", "Test %s failed due to timeout", test->value);
    rSignal("test:complete");
    rStop();
}

#if SERVICES_DATABASE
/*
    Test the database by performing a simple update to the State entity
 */
static void dbLocalTest(Json *json)
{
    cchar *lastUpdate;

    rInfo("test", "Run test: dbLocal");
    lastUpdate = dbGetField(ioto->db, "SyncState", "lastUpdate", NULL, NULL);
    if (dbUpdate(ioto->db, "SyncState", DB_PROPS("lastUpdate", lastUpdate), NULL) == 0) {
        rError("provision", "Cannot update State: %s", dbGetError(ioto->db));
    }
}
#endif

#if SERVICES_MQTT
static void metricTest(Json *json)
{
    double value;

    value = ((double) random() / RAND_MAX) * 10;
    ioSetMetric("metric-test", value, "", 0);

    //  Allow metric set above to take
    rSleep(2 * TPS);
    value = ioGetMetric("metric-test", "", "sum", 3600);
    rInfo("test", "Got metric value %lf", value);

    //  Issue a metric with explicit dimensions that is aggregated for all devices
    value = ((double) random() / RAND_MAX) * 10;
    ioSetMetric("metric-test-fleet", value, "[{}]", 0);
}

/*
    Test the cloud Store. This sends update to the Store using MQTT.
 */
static void storeMqttSetTest(Json *json)
{
    static int counter = 0;

    rInfo("test", "Run test: store-mqtt: set counter %d", counter++);
    ioSetNum("counter", counter);
}

static void storeMqttGetTest(Json *json)
{
    int64 num;

    rSleep(500);
    num = ioGetNum("counter");
    rInfo("test", "store-mqtt: get result %lld", num);
}

#if FUTURE
/*
    Set a value in the cloud Store and trigger metric creation
 */
static void mqttWithMetrics(Json *json)
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
    mqttPublish(ioto->mqtt, msg, -1, 1, MQTT_WAIT_NONE,
                "$aws/rules/IotoDevice/ioto/service/%s/store/set", ioto->id);
}
#endif

/*
    Send a MQTT request ping. This is fire and forget. The server side does nothing.
 */
static void mqttPingTest(Json *json)
{
    rInfo("test", "Run test: mqttPing");
    mqttPublish(ioto->mqtt, "", -1, 1, MQTT_WAIT_NONE, "ioto/service/%s/test/ping", ioto->id);
}

/*
    Test the mqttRequest API. This sends a request and waits for a reponse.
 */
static void mqttRequestTest(Json *json)
{
    char *result;

    rInfo("test", "Run test: mqttRequest");
    result = mqttRequest(ioto->mqtt, "{\"key\":\"counter\"}", 0, "store/get");
    rInfo("test", "MQTT request result %s", result);
    rFree(result);
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
static void storeSyncTest(Json *json)
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
    } else {
        //  Wait for a response (just for testing so we can block for this test iteration)
        rYieldFiber(0);
    }
}
#endif

#if SERVICES_SHADOW
static void shadowTest(Json *json)
{
    rInfo("test", "Run test: shadow");
}
#endif

#if SERVICES_URL
static void urlTest(Json *json)
{
    rInfo("test", "Run test: url");
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
static void webStreamTest(Json *json)
{
    rInfo("test", "Run test: webStream");
    streamCount = 100;
    webAddAction(ioto->webHost, "/api/public/stream", streamAction, NULL);
}

#endif

#if SERVICES_KEYS
static void s3Test(Json *json)
{
    Url   *up;
    cchar *region;
    char  *data;

    region = "ap-northeast-1";
    up = urlAlloc(0);

    data = sfmt("{\"logGroupName\":\"%s\"}", "test-45");
    int rc = aws(up, region, "logs", "Logs_20140328.CreateLogGroup", data, -1, NULL);
    urlFree(up);
    assert(rc);

#if KEEP
    data = sfmt("{\"logGroupName\":\"%s\"}", "test-42");
    rc = aws(up, region, "logs", "Logs_20140328.CreateLogGroup", data, -1, NULL);
    urlFree(up);

    rc = awsPutToS3("ap-northeast-1", "ioto-test-43", "test.txt", "hello world\n", -1);
    print("RC %d", rc);

    rc = awsPutFileToS3(region, "ioto-test-43", "test.tmp", "a.tmp");
#endif
}
#endif
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
