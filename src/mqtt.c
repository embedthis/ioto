/*
    mqtt.c - MQTT service setup

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

#if SERVICES_MQTT
/*********************************** Locals **********************************/

#define RR_DEFAULT_TIMEOUT  (30 * TPS);
#define CONNECT_MAX_RETRIES 3

/*
    Mqtt request/response support
 */
typedef struct RR {
    char *topic;        /* Subscribed topic */
    REvent timeout;     /* Timeout event */
    RFiber *fiber;      /* Wait fiber */
    int seq;            /* Unique request sequence number (can wrap) */
} RR;

static ssize  nextRr = 99;
static REvent mqttBackoff;
static REvent mqttWindow;

/************************************ Forwards ********************************/

static int attachSocket(int retry);
static int connectMqtt(void);
static void freeRR(RR *rr);
static void onEvent(Mqtt *mq, int event);
static void rrResponse(const MqttRecv *rp);
static void startMqtt(Time lastConnect);
static void throttle(const MqttRecv *rp);

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

    rWatch("cloud:provisioned", (RWatchProc) startMqtt, 0);
    if (ioto->endpoint) {
        startMqtt(0);
    }
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
    rWatchOff("cloud:provisioned", (RWatchProc) startMqtt, 0);
    rStopEvent(ioto->scheduledConnect);
}

/*
    Schedule an mqtt cloud connection according to the mqtt schedule
    This is idempotent. Will cancel existing schedule and reestablish.
 */
static void startMqtt(Time lastConnect)
{
    Time  delay, jitter, now, when, wait;
    cchar *schedule;

    schedule = jsonGet(ioto->config, 0, "mqtt.schedule", 0);
    delay = svalue(jsonGet(ioto->config, 0, "mqtt.delay", "0")) * TPS;
    now = rGetTime();
    when = lastConnect + delay;
    if (when < now) {
        when = now;
    }
    wait = schedule ? cronUntil(schedule, when) : 0;
    if (wait > 0) {
        jitter = svalue(jsonGet(ioto->config, 0, "mqtt.jitter", "0")) * TPS;
        if (jitter) {
            /*
                SECURITY Acceptable: - the use of rand here is acceptable as it is only used for the
                mqtt schedule jitter and is not a security risk.
             */
            jitter = rand() % jitter;
            if (wait < MAXTIME - jitter) {
                wait += jitter;
            }
        }
    }
    if (ioto->scheduledConnect) {
        rStopEvent(ioto->scheduledConnect);
    }
    if (ioto->blockedUntil - rGetTime() > wait) {
        wait = ioto->blockedUntil - rGetTime();
    }
    if (wait >= MAXTIME) {
        rInfo("mqtt", "Using on-demand MQTT connections");
    } else {
        wait = max(0, wait);
        rInfo("mqtt", "Schedule MQTT connect in %d secs", (int) wait / TPS);
        ioto->scheduledConnect = rStartEvent((REventProc) connectMqtt, 0, wait);
    }
}

/*
    Connect to the cloud. This will provision if required and may block a long time.
    Called from scheduleConnect, processDeviceCommand and from ioProvision.
    NOTE: there may be multiple callers and so as fiber code, it used rEnter/rLeave.
 */
static int connectMqtt(void)
{
    static int  reprovisions = 0;
    static bool connecting = 0;
    cchar       *schedule;
    Ticks       delay, window;
    int         i, maxReprovision, rc;

    if (ioto->connected) {
        return 0;
    }
    if (!ioto->endpoint) {
        // Wait for provisioning to complete and we'll be recalled.
        return R_ERR_CANT_CONNECT;
    }
    // Wakeup an existing caller alseep in backoff
    ioResumeBackoff(&mqttBackoff);
    rEnter(&connecting, 0);

    /*
        Retry connection attempts
     */
    for (delay = TPS, i = 0; i < CONNECT_MAX_RETRIES && !ioto->connected; i++) {
        if ((rc = attachSocket(i)) == 0) {
            // Successful connection
            break;
        }
        if (rc == R_ERR_CANT_COMPLETE) {
            // Connection worked, but mqtt communications failed. So don't retry.
            break;
        }
        delay = ioBackoff(delay, &mqttBackoff);
    }
    rLeave(&connecting);

    if (ioto->connected) {
        schedule = jsonGet(ioto->config, 0, "mqtt.schedule", 0);
        window = schedule ? cronUntilEnd(schedule, rGetTime()) : 0;
        if (window > (MAXINT64 - MAXINT)) {
            if (mqttWindow) {
                rStopEvent(mqttWindow);
            }
            mqttWindow = rStartEvent((REventProc) ioDisconnect, 0, window);
            rInfo("mqtt", "MQTT connection window closes in %d secs", (int) (window / TPS));
        }
    } else {
        if (rCheckInternet()) {
            rError("mqtt", "Failed to establish cloud messaging connection");
            // Test vs the boot session maximum reprovision limit
            maxReprovision = jsonGetInt(ioto->config, 0, "limits.reprovision", 5);
            if (reprovisions++ < maxReprovision) {
                ioDeprovision();
                // Wait for cloud:provisioned event
            }
        } else {
            // Connection failed. Schedule a retry
            rError("mqtt", "Device cloud connection failed");
            startMqtt(rGetTime());
        }
        return R_ERR_CANT_CONNECT;
    }
    return 0;
}

static void disconnectMqtt(void)
{
    ioto->cloudReady = 0;

    if (ioto->mqttSocket) {
        rInfo("mqtt", "Cloud connection closed");
        rFreeSocket(ioto->mqttSocket);
        ioto->mqttSocket = 0;
    }
    if (ioto->connected) {
        ioto->connected = 0;
        rSignal("mqtt:disconnected");
        startMqtt(rGetTime());
    }
}

/*
    Forcibly connect to the cloud despite the schedule window
 */
PUBLIC int ioConnect(void)
{
    if (!ioto->connected && ioto->endpoint) {
        return connectMqtt();
    }
    return 0;
}

/*
    Called to force a disconnect
 */
PUBLIC void ioDisconnect()
{
    rDisconnectSocket(ioto->mqttSocket);
}

/*
    Attach a socket to the MQTT object. Called only from connectMqtt().
 */
static int attachSocket(int retry)
{
    char    topic[MQTT_MAX_TOPIC_SIZE];
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
    /*
        The connect may work even if the certificate is inactive. The mqttConnect will then fail
     */
    if (rConnectSocket(sock, endpoint, port, 0) < 0) {
        if (retry == 0) {
            rError("mqtt", "Cannot connect to socket at %s:%d %s", endpoint, port, sock->error ? sock->error : "");
        }
        rFreeSocket(sock);
        return R_ERR_CANT_CONNECT;
    }
    if (mqttConnect(ioto->mqtt, sock, 0, MQTT_WAIT_ACK) < 0) {
        rDebug("mqtt", "Cannot connect with MQTT");
        rFreeSocket(sock);
        return R_ERR_CANT_COMPLETE;
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
        Setup the device cloud throttle indicator. This is important to optimize device fleets.
     */
    mqttSubscribe(ioto->mqtt, throttle, 1, MQTT_WAIT_NONE,
                  SFMT(topic, "ioto/device/%s/mqtt/throttle", ioto->id));

    rInfo("mqtt", "Connected to mqtt://%s:%d", endpoint, port);
    /*
        The cloud is now connected, but not yet ready if using sync service.
     */
    rSignal("mqtt:connected");
#if !SERVICES_SYNC
    // If sync service enabled, then cloud:ready is signaled by sync.c after a syncdown completion.
    rSignal("cloud:ready");
#endif
    return 0;
}

static void throttle(const MqttRecv *rp)
{
    Json *json;
    Time timestamp;

    json = jsonParse(rp->data, 0);
    if (!json) {
        rError("mqtt", "Received bad throttle data: %s", rp->data);
        return;
    }
    timestamp = jsonGetNum(json, 0, "timestamp", 0);
    if (!timestamp || timestamp < (rGetTime() - 30 * TPS)) {
        rTrace("mqtt", "Reject stale throttle data: %lld secs ago", (rGetTime() - timestamp) / TPS);
        jsonFree(json);
        return;
    }
    if (jsonGetBool(json, 0, "close", 0)) {
        rInfo("mqtt", "Cloud connection blocked due to persistent excessive I/O. Delay reprovision for 1 hour.");
        rDisconnectSocket(ioto->mqttSocket);
        ioto->blockedUntil = rGetTime() + IO_REPROVISION * TPS;
    } else {
        mqttThrottle(ioto->mqtt);
    }
    jsonFree(json);
    rSignal("mqtt:throttle");
}

/*
    Respond to MQTT events
 */
static void onEvent(Mqtt *mqtt, int event)
{
    if (rGetState() != R_READY) {
        return;
    }
    switch (event) {
    case MQTT_EVENT_ATTACH:
        /*
            On-demand connection required. Ignore the schedule window.
         */
        connectMqtt();
        break;

    case MQTT_EVENT_DISCONNECT:
        disconnectMqtt();
        break;

    case MQTT_EVENT_TIMEOUT:
        //  Respond to timeout and force a disconnection
        rDisconnectSocket(ioto->mqttSocket);
    }
}

/*
    Alloc a request/response. This manages the MQTT subscriptions for specific topics.
    SECURITY Acceptable: - Request IDs will wrap around after 2^31.
 */
static RR *allocRR(Mqtt *mq, cchar *topic)
{
    char subscription[MQTT_MAX_TOPIC_SIZE];
    RR   *rr, *rp;
    int  index;

    if (!mq || !topic) {
        return NULL;
    }
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
        //  Subscribe to all sequence numbers on this topic, this will use the master subscription.
        SFMT(subscription, "%s/+", topic);
        if (mqttSubscribe(mq, rrResponse, 1, MQTT_WAIT_NONE, subscription) < 0) {
            rError("mqtt", "Cannot subscribe to %s", subscription);
            freeRR(rr);
            return NULL;
        }
        rr->topic = sclone(topic);
    }
    rPushItem(ioto->rr, rr);
    return rr;
}

static void freeRR(RR *rr)
{
    if (rr) {
        // Optimization: no benefit from local unsubscription when using master subscriptions.
        rFree(rr->topic);
        rFree(rr);
    }
}

#if KEEP
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
#endif

/*
    Process a response. Resume the fiber and pass the response data. The caller of mqttRequest must free.
 */
static void rrResponse(const MqttRecv *rp)
{
    RR  *rr;
    int index, seq;

    seq = (int) stoi(rBasename(rp->topic));
    for (ITERATE_ITEMS(ioto->rr, rr, index)) {
        if (rr->seq == seq) {
            if (rr->timeout) {
                rStopEvent(rr->timeout);
            }
            rRemoveItem(ioto->rr, rr);
            rResumeFiber(rr->fiber, (void*) sclone(rp->data));
            freeRR(rr);
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
    rInfo("mqtt", "MQTT request timed out");
    rRemoveItem(ioto->rr, rr);
    rResumeFiber(rr->fiber, 0);
}

/*
    Initiate a request
 */
PUBLIC char *mqttRequest(Mqtt *mq, cchar *body, Ticks timeout, cchar *topicFmt, ...)
{
    va_list ap;
    RR      *rr;
    char    publish[MQTT_MAX_TOPIC_SIZE], subscription[MQTT_MAX_TOPIC_SIZE], topic[MQTT_MAX_TOPIC_SIZE];

    va_start(ap, topicFmt);
    sfmtbufv(topic, sizeof(topic), topicFmt, ap);
    va_end(ap);

    //  This will use the master subscription
    SFMT(subscription, "ioto/device/%s/%s", ioto->id, topic);
    if ((rr = allocRR(mq, subscription)) == 0) {
        return NULL;
    }
    timeout = timeout > 0 ? timeout : RR_DEFAULT_TIMEOUT;
    if (!rGetTimeouts()) {
        timeout = MAXINT;
    }
    rr->timeout = rStartEvent((REventProc) rrTimeout, rr, timeout);

    SFMT(publish, "ioto/service/%s/%s/%d", ioto->id, topic, rr->seq);
    if (mqttPublish(ioto->mqtt, body, 0, 1, MQTT_WAIT_NONE, publish) < 0) {
        return NULL;
    }
    //  Returns null on a timeout. Caller must free result.
    return rYieldFiber(0);
}

#if KEEP
/*
    Release a request/response subscription
 */
PUBLIC void mqttRequestFree(Mqtt *mq, cchar *topicFmt, ...)
{
    va_list ap;
    char    subscription[MQTT_MAX_TOPIC_SIZE], topic[MQTT_MAX_TOPIC_SIZE];

    va_start(ap, topicFmt);
    sfmtbufv(topic, sizeof(topic), topicFmt, ap);
    va_end(ap);

    SFMT(subscription, "ioto/device/%s/%s", ioto->id, topic);
    freeRR(mq, subscription);
}
#endif

/*
    Get an accumulated metric value for a period
    Dimensions is a JSON object.
 */
PUBLIC double ioGetMetric(cchar *metric, cchar *dimensions, cchar *statistic, int period)
{
    char   *result;
    char   *msg;
    double num;

    if (dimensions == NULL || *dimensions == '\0') {
        dimensions = "{\"Device\":\"${deviceId}\"}";
    }
    msg = sfmt("{\"metric\":\"%s\",\"dimensions\":%s,\"period\":%d,\"statistic\":\"%s\"}",
               metric, dimensions, period, statistic);
    result = mqttRequest(ioto->mqtt, msg, 0, "metric/get");
    num = stod(result);
    rFree(result);
    rFree(msg);
    return num;
}

/*
    Define a metric in the Embedthis/Device namespace.
    Dimensions is a JSON array of objects. Each object contains the properties of that dimension.
    The {} object, means no dimensions.
 */
PUBLIC int ioSetMetric(cchar *metric, double value, cchar *dimensions, int elapsed)
{
    char *msg;
    int rc;

    if (dimensions == NULL || *dimensions == '\0') {
        dimensions = "[{\"Device\":\"${deviceId}\"}]";
    }
    msg = sfmt("{\"metric\":\"%s\",\"value\":%g,\"dimensions\":%s,\"buffer\":{\"elapsed\":%d}}",
               metric, value, dimensions, elapsed);
    rc = mqttPublish(ioto->mqtt, msg, 0, 1, MQTT_WAIT_NONE,
                "$aws/rules/IotoDevice/ioto/service/%s/metric/set", ioto->id);
    rFree(msg);
    return rc;
}

/*
    Set a value in the Store key/value database.
    Uses db sync if available, otherwise uses MQTT.
 */
PUBLIC void ioSet(cchar *key, cchar *value)
{
#if SERVICES_SYNC
    dbUpdate(ioto->db, "Store",
             DB_JSON("{key: '%s', value: '%s', type: 'string'}", key, value),
             DB_PARAMS(.upsert = 1));
#else
    //  Optimize by using AWS basic ingest topic
    char *msg = sfmt("{\"key\":\"%s\",\"value\":\"%s\",\"type\":\"string\"}", key, value);
    mqttPublish(ioto->mqtt, msg, -1, 1, MQTT_WAIT_NONE,
                "$aws/rules/IotoDevice/ioto/service/%s/store/set", ioto->id);
    rFree(msg);
#endif
}

/*
    Set a number in the Store key/value database.
    Uses db sync if available, otherwise uses MQTT.
 */
PUBLIC int ioSetNum(cchar *key, double value)
{
    int rc;

    rc = 0;
#if SERVICES_SYNC
    if (dbUpdate(ioto->db, "Store",
             DB_JSON("{key: '%s', value: '%g', type: 'number'}", key, value),
             DB_PARAMS(.upsert = 1)) == NULL) {
        return R_ERR_CANT_WRITE;
    }
#else
    //  Optimize by using AWS basic ingest topic
    char *msg = sfmt("{\"key\":\"%s\",\"value\":%lf,\"type\":\"number\"}", key, value);
    rc = mqttPublish(ioto->mqtt, msg, -1, 1, MQTT_WAIT_NONE,
                "$aws/rules/IotoDevice/ioto/service/%s/store/set", ioto->id);
    rFree(msg);
#endif
    return rc;
}

/*
    Set a number in the Store key/value database.
    Uses db sync if available, otherwise uses MQTT.
 */
PUBLIC void ioSetBool(cchar *key, bool value)
{
 #if SERVICES_SYNC
    dbUpdate(ioto->db, "Store",
             DB_JSON("{key: '%s', value: '%g', type: 'boolean'}", key, value),
             DB_PARAMS(.upsert = 1));
 #else
    //  Optimize by using AWS basic ingest topic
    char *msg = sfmt("{\"key\":\"%s\",\"value\":%lf,\"type\":\"boolean\"}", key, value);
    mqttPublish(ioto->mqtt, msg, -1, 1, MQTT_WAIT_NONE,
                "$aws/rules/IotoDevice/ioto/service/%s/store/set", ioto->id);
    rFree(msg);
 #endif
}

//  Caller must free
PUBLIC char *ioGet(cchar *key)
{
#if SERVICES_SYNC
    return sclone(dbGetField(ioto->db, "Store", "value", DB_PROPS("key", key), DB_PARAMS()));
#else
    char *msg = sfmt("{\"key\":\"%s\"}", key);
    //  Must not use basic-ingest for mqttRequest
    char *result = mqttRequest(ioto->mqtt, msg, 0, "store/get");
    rFree(msg);
    return result;
#endif
}

PUBLIC bool ioGetBool(cchar *key)
{
    char *msg, *result;
    bool value;

    msg = sfmt("{\"key\":\"%s\"}", key);
    //  Must not use basic-ingest for mqttRequest
    result = mqttRequest(ioto->mqtt, msg, 0, "store/get");
    value = 0;
    if (smatch(result, "true")) {
        value = 1;
    } else if (smatch(result, "false")) {
        value = 0;
    }
    rFree(msg);
    rFree(result);
    return value;
}

PUBLIC double ioGetNum(cchar *key)
{
    char   *msg, *result;
    double num;

    msg = sfmt("{\"key\":\"%s\"}", key);
    //  Must not use basic-ingest for mqttRequest
    result = mqttRequest(ioto->mqtt, msg, 0, "store/get");
    if (result == NULL) {
        return 0;
    }
    num = stod(result);
    rFree(msg);
    rFree(result);
    return num;
}

PUBLIC bool ioConnected(void)
{
    return ioto->connected;
}

/*
    Run a function when the cloud connection is established and ready for use.
 */
PUBLIC void ioOnConnect(RWatchProc fn, void *arg, bool sync)
{
    if (!ioto->cloudReady) {
        rWatch("cloud:ready", fn, arg);
        return;
    }
    if (sync) {
        fn(NULL, arg);
    } else {
        rSpawnFiber("onconnect", (RFiberProc) fn, arg);
    }
}

PUBLIC void ioOnConnectOff(RWatchProc fn, void *arg)
{
    rWatchOff("cloud:ready", fn, arg);
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
