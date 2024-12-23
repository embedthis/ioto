/*
 * MQTT Client Library Source
 */

#include "mqtt.h"

#if ME_COM_MQTT



/********* Start of file src/mqttLib.c ************/

/*
    mqtt.c - MQTT library

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.

    Spec: https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html

    This modules support on-demand connection. To facilitate, this module maintains a "timeout" which
    is how long the connection may be used. That is independant of the keepAlive which platforms typically
    limit. AWS limits this to 1200 seconds. To keep a connection alive longer than that, we employ a Ping request.
 */

/********************************** Includes **********************************/



#if ME_COM_MQTT
/********************************** Defines ***********************************/

#define MqttThrottleMin   500
#define MqttThrottleMax   30 * TPS

#define MQTT_HTONS(s) htons(s)
#define MQTT_NTOHS(s) ntohs(s)
#define MQTT_WAIT_TIMEOUT (15 * TPS)

#if ME_R_DEBUG_LOGGING
static cchar *packetTypes[] = {
    "Unknown packet type",
    "Connect",
    "Connect Ack",
    "Publish",
    "Publish Ack",
    "Publish Rec",
    "Publish Rel",
    "Publish Comp",
    "Subscribe",
    "Subscribe Ack",
    "Unsubscribe",
    "Unsubscribe Ack",
    "Ping",
    "Ping Ack",
    "Disconnect",
};
#endif

/*********************************** Locals ***********************************/

#define MQTT_BITFIELD_RULE_VIOLOATION(bitfield, rule_value, rule_mask) ((bitfield ^ rule_value)&rule_mask)

struct {
    cuchar typeIsValid[16];
    cuchar requiredFlags[16];
    cuchar requiredFlagsMask[16];
} MqttHdr_rules = {
    {
        // boolean value, true if type is valid
        0x00, // MQTT_PACKET_RESERVED
        0x01, // MQTT_PACKET_CONNECT
        0x01, // MQTT_PACKET_CONN_ACK
        0x01, // MQTT_PACKET_PUBLISH
        0x01, // MQTT_PACKET_PUB_ACK
        0x01, // MQTT_PACKET_PUB_REC
        0x01, // MQTT_PACKET_PUB_REL
        0x01, // MQTT_PACKET_PUB_COMP
        0x01, // MQTT_PACKET_SUB
        0x01, // MQTT_PACKET_SUB_ACK
        0x01, // MQTT_PACKET_UNSUB
        0x01, // MQTT_PACKET_UNSUB_ACK
        0x01, // MQTT_PACKET_PING
        0x01, // MQTT_PACKET_PING_ACK
        0x01, // MQTT_PACKET_DISCONNECT
        0x00  // MQTT_PACKET_RESERVED
    },
    {
        // flags that must be set for the associated control type
        0x00, // MQTT_PACKET_RESERVED
        0x00, // MQTT_PACKET_CONNECT
        0x00, // MQTT_PACKET_CONN_ACK
        0x00, // MQTT_PACKET_PUBLISH
        0x00, // MQTT_PACKET_PUB_ACK
        0x00, // MQTT_PACKET_PUB_REC
        0x02, // MQTT_PACKET_PUB_REL
        0x00, // MQTT_PACKET_PUB_COMP
        0x02, // MQTT_PACKET_SUB
        0x00, // MQTT_PACKET_SUB_ACK
        0x02, // MQTT_PACKET_UNSUB
        0x00, // MQTT_PACKET_UNSUB_ACK
        0x00, // MQTT_PACKET_PING
        0x00, // MQTT_PACKET_PING_ACK
        0x00, // MQTT_PACKET_DISCONNECT
        0x00  // MQTT_PACKET_RESERVED
    },
    {
        // mask of flags that must be specific values for the associated control type
        0x00, // MQTT_PACKET_RESERVED
        0x0F, // MQTT_PACKET_CONNECT
        0x0F, // MQTT_PACKET_CONN_ACK
        0x00, // MQTT_PACKET_PUBLISH
        0x0F, // MQTT_PACKET_PUB_ACK
        0x0F, // MQTT_PACKET_PUB_REC
        0x0F, // MQTT_PACKET_PUB_REL
        0x0F, // MQTT_PACKET_PUB_COMP
        0x0F, // MQTT_PACKET_SUB
        0x0F, // MQTT_PACKET_SUB_ACK
        0x0F, // MQTT_PACKET_UNSUB
        0x0F, // MQTT_PACKET_UNSUB_ACK
        0x0F, // MQTT_PACKET_PING
        0x0F, // MQTT_PACKET_PING_ACK
        0x0F, // MQTT_PACKET_DISCONNECT
        0x00  // MQTT_PACKET_RESERVED
    }
};

/*********************************** Forwards *********************************/

static MqttMsg *allocMsg(Mqtt *mq, int type, int id, ssize size);
static MqttTopic *allocTopic(Mqtt *mq, MqttCallback callback, cchar *topic, MqttWaitFlags wait);
static void dequeueMsg(Mqtt *mq, MqttMsg *msg);
static MqttMsg *findMsg(Mqtt *mq, MqttPacketType type, int id);
static MqttMsg *findMsgByType(Mqtt *mq, MqttPacketType type);
static void freeMsg(MqttMsg *msg);
static void freeTopics(Mqtt *mq, cchar *topic);
static void freeTopic(MqttTopic *tp);
static int getId(Mqtt *mq);
static int getStringLen(cchar *s);
static MqttTopic *getTopic(Mqtt *mq, MqttRecv *topic);
static void idleCheck(Mqtt *mq);
static void incomingMsg(MqttRecv *rp);
static bool matchTopic(MqttTopic *tp, cchar **segments);
static void notify(Mqtt *mq, int event);
static int packHdr(Mqtt *mq, uchar *bp, MqttHdr *hdr);
static int packString(uchar *bp, cchar *str);
static int packUnit16(uchar *bp, uint16 integer);
static MqttMsg *packPub(Mqtt *mq, MqttPacketType type, int id);
static void processMqtt(Mqtt *mq);
static int processRecvMsg(Mqtt *mq, MqttRecv *rp);
static int processSentMsg(Mqtt *mq, MqttMsg *msg);
static int pubAck(Mqtt *mq, int id);
static int pubComp(Mqtt *mq, int id);
static int pubRec(Mqtt *mq, int id);
static int pubRel(Mqtt *mq, int id);
static int publish(Mqtt *mq, cvoid *buf, ssize bufsize, int qos, MqttWaitFlags wait, int retain, cchar *topic);
static void queueMsg(Mqtt *mq, MqttMsg *msg, uchar *end);
static int recvMsgs(Mqtt *mq);
static void resetConnection(Mqtt *mq);
static int sendMsgs(Mqtt *mq);
static int setError(Mqtt *mq, int error, cchar *fmt, ...);
static void setState(Mqtt *mq, MqttMsg *msg, int state);
static cchar **splitTopic(cchar *topic, ssize topicLen, char **segbuf);
static void stopProcessing(Mqtt *mq);
static int subscribe(Mqtt *mq, MqttCallback callback, int maxQos, MqttWaitFlags wait, cchar *topic);
static int unpackConn(Mqtt *mq, MqttRecv *rp, cuchar *bp);
static int unpackPublish(Mqtt *mq, MqttRecv *rp, cuchar *bp);
static int unpackPub(Mqtt *mq, MqttRecv *rp, cuchar *bp);
static int unpackResp(Mqtt *mq, MqttRecv *rp);
static int unpackRespHdr(Mqtt *mq, MqttRecv *rp);
static int unpackSuback(Mqtt *mq, MqttRecv *rp, cuchar *bp);
static int unpackUnsubAck(Mqtt *mq, MqttRecv *rp, cuchar *bp);
static uint16 unpackUint16(cuchar *bp);
static int waitUntil(Mqtt *mq, MqttMsg *msg, MqttWaitFlags state);

/************************************* Code ***********************************/

PUBLIC Mqtt *mqttAlloc(cchar *clientId, MqttEventProc proc)
{
    Mqtt *mq;

    assert(clientId && *clientId);

    mq = rAllocType(Mqtt);

    if ((mq->buf = rAllocBuf(MQTT_BUF_SIZE)) == 0) {
        return 0;
    }
    if ((mq->topics = rAllocList(0, 0)) == 0) {
        return 0;
    }
    mq->id = sclone(clientId);
    mq->proc = proc;
    mq->head.next = mq->head.prev = &mq->head;
    mq->error = R_ERR_NOT_CONNECTED;
    mq->msgTimeout = MQTT_MSG_TIMEOUT;
    mq->maxMessage = MQTT_MAX_MESSAGE_SIZE;
    mq->mask = R_READABLE;
    mq->nextId = 0;
    mq->lastActivity = rGetTicks();
    mq->masterTopics = rAllocList(0, R_DYNAMIC_VALUE);

    mq->keepAlive = MQTT_KEEP_ALIVE;
    mq->timeout = MQTT_TIMEOUT;
    return mq;
}

PUBLIC void mqttFree(Mqtt *mq)
{
    if (!mq) return;
    stopProcessing(mq);

    mq->freed = 1;
    resetConnection(mq);
    rFreeBuf(mq->buf);
    rFreeList(mq->topics);
    rFreeList(mq->masterTopics);
    rFree(mq->errorMsg);
    rFree(mq->willMsg);
    rFree(mq->willTopic);
    rFree(mq->id);
    rFree(mq);
}

static void resetConnection(Mqtt *mq)
{
    if (mq->keepAliveEvent) {
        rStopEvent(mq->keepAliveEvent);
    }
    freeTopics(mq, NULL);
    rFlushBuf(mq->buf);
    rFree(mq->errorMsg);
    mq->errorMsg = 0;
    mq->error = R_ERR_NOT_CONNECTED;
}

static void stopProcessing(Mqtt *mq)
{
    MqttMsg *msg, *next;

    if (mq->freed) return;

    /*
        Disconnected - resume all waiting fibers
     */
    for (msg = mq->head.next; msg != &mq->head; msg = next) {
        next = msg->next;
        if (msg->fiber) {
            //  WARNING: this will not switch immediately to the other fiber
            rResumeFiber(msg->fiber, (void*) (ssize) R_ERR_NOT_CONNECTED);
        }
        msg->wait = 0;
        dequeueMsg(mq, msg);
    }
    rDebug("mqtt", "Disconnecting mqtt connection");

    //  Remove recv buffer and subscription topics
    resetConnection(mq);

    mq->sock = 0;
    mq->processing = 0;
    mq->error = 0;

    if (rGetState() <= R_STOPPING) {
        //  Must do last
        notify(mq, MQTT_EVENT_DISCONNECT);
    }
}

static void processMqtt(Mqtt *mq)
{
    ssize mask;

    assert(mq);

    mask = (int) (ssize) (rGetFiber()->result);
    if (mask != 0) {
        if (mask & R_READABLE) {
            recvMsgs(mq);
        }
        if (mask & R_WRITABLE && mqttMsgsToSend(mq)) {
            sendMsgs(mq);
        }
    }
    if (mq->error) {
        stopProcessing(mq);
    } else {
        mask = R_READABLE | (mqttMsgsToSend(mq) ? R_WRITABLE : 0);
        rSetWaitMask(mq->sock->wait, mask, rGetTicks() + MQTT_WAIT_TIMEOUT);
    }
}

PUBLIC void mqttSetCredentials(Mqtt *mq, cchar *username, cchar *password)
{
    rFree(mq->password);
    rFree(mq->username);
    mq->password = sclone(password);
    mq->username = sclone(username);
}

PUBLIC int mqttConnect(Mqtt *mq, RSocket *sock, int flags, MqttWaitFlags wait)
{
    MqttMsg *msg;
    MqttHdr hdr;
    Ticks   delay;
    cchar   *id;
    uchar   *bp;
    int     rc, length;

    assert(mq);

    flags = flags & ~(MQTT_CONNECT_RESERVED);
    mq->sock = sock;

    id = mq->id ? mq->id : "";
    if (mq->error == R_ERR_NOT_CONNECTED) {
        mq->error = 0;
    }
    hdr.type = MQTT_PACKET_CONNECT;
    hdr.flags = 0x00;

    /*
        Size of variable portion: 10 + string + topic + will message + user name + password
     */
    length = 10;

    if (id[0] == '\0' && !(flags & MQTT_CONNECT_CLEAN_SESSION)) {
        // Clean session if empty id
        return setError(mq, R_ERR_BAD_SESSION, "Missing client ID");
    }
    length += getStringLen(id);

    if (mq->willTopic && mq->willMsg) {
        flags |= MQTT_CONNECT_WILL_FLAG;
        length += getStringLen(mq->willTopic);

        if (mq->willMsg == NULL) {
            return setError(mq, R_ERR_BAD_ARGS, "Missing will message");
        }
        length += 2 + (int) mq->willMsgSize;

        // assert that the will QOS is valid (i.e. not 3)
        if ((flags & 0x18) == 0x18) {
            // bitwise equality with QoS 3 (invalid)
            return setError(mq, R_ERR_BAD_ARGS, "Bad QOS in flags 0x%x", flags);
        }
    } else {
        // there is no will so set all will flags to zero
        flags &= ~MQTT_CONNECT_WILL_FLAG;
        flags &= ~0x18;
        flags &= ~MQTT_CONNECT_WILL_RETAIN;
    }

    if (mq->username && *mq->username) {
        flags |= MQTT_CONNECT_USER_NAME;
        length += getStringLen(mq->username);
    } else {
        flags &= ~MQTT_CONNECT_USER_NAME;
    }

    if (mq->password && *mq->password) {
        flags |= MQTT_CONNECT_PASSWORD;
        length += getStringLen(mq->password);
    } else {
        flags &= ~MQTT_CONNECT_PASSWORD;
    }
    hdr.length = length;

    if ((msg = allocMsg(mq, MQTT_PACKET_CONNECT, 0, length)) == 0) {
        return R_ERR_MEMORY;
    }
    bp = msg->start;

    rc = packHdr(mq, bp, &hdr);
    if (rc < 0) {
        freeMsg(msg);
        return rc;
    }
    bp += rc;

    *bp++ = 0x00;
    *bp++ = 0x04;
    *bp++ = (uchar) 'M';
    *bp++ = (uchar) 'Q';
    *bp++ = (uchar) 'T';
    *bp++ = (uchar) 'T';
    *bp++ = MQTT_PROTOCOL_LEVEL;
    *bp++ = flags;

    /*
        We implement on-demand connections and flexible keep alive
        Set the server side up to the max keep alive
     */
    bp += packUnit16(bp, (int) (mq->keepAlive / TPS));
    bp += packString(bp, id);

    if (flags & MQTT_CONNECT_WILL_FLAG) {
        bp += packString(bp, mq->willTopic);
        bp += packUnit16(bp, (uint16) mq->willMsgSize);
        memcpy(bp, mq->willMsg, mq->willMsgSize);
        bp += mq->willMsgSize;
    }
    if (flags & MQTT_CONNECT_USER_NAME) {
        bp += packString(bp, mq->username);
    }
    if (flags & MQTT_CONNECT_PASSWORD) {
        bp += packString(bp, mq->password);
    }
    if (!mq->processing) {
        rSetWaitHandler(mq->sock->wait, (RWaitProc) processMqtt, mq, R_IO, rGetTicks() + MQTT_WAIT_TIMEOUT);
    }
    queueMsg(mq, msg, bp);

    //  Wait for conn ack
    waitUntil(mq, msg, wait);

    if (!mq->connected) {
        return R_ERR_CANT_CONNECT;
    }
    if (mq->keepAliveEvent) {
        rStopEvent(mq->keepAliveEvent);
    }
    delay = min(mq->keepAlive, mq->timeout);
    mq->keepAliveEvent = rStartEvent((REventProc) idleCheck, mq, delay);
    notify(mq, MQTT_EVENT_CONNECTED);
    return 0;
}

static void idleCheck(Mqtt *mq)
{
    Ticks delay, elapsed;

    if (!mq->sock) return;

    elapsed = rGetTicks() - mq->lastActivity + 1;
    if (elapsed >= mq->timeout) {
        rInfo("mqtt", "Idle connection has timed out");
        notify(mq, MQTT_EVENT_TIMEOUT);

    } else {
        if (elapsed >= mq->keepAlive) {
            rTrace("mqtt", "Keeping connection alive with ping");
            mqttPing(mq);
        }
        delay = max(min(mq->keepAlive, mq->timeout) - elapsed, TPS);
        rStartEvent((REventProc) idleCheck, mq, delay);
    }
}

/*
    Notify the caller of an event
 */
static void notify(Mqtt *mq, int event)
{
    if (mq->proc) {
        (mq->proc)(mq, event);
    }
}

/*
    Attach to a socket. If already attached, all good. If not, notify the caller to attach.
 */
static bool onDemandAttach(Mqtt *mq)
{
    if (mq->sock) {
        return 1;
    }
    /*
        The caller should call mqttConnect which defines mq->sock
     */
    rDebug("mqtt", "Attaching socket");
    notify(mq, MQTT_EVENT_ATTACH);

    if (!mq->sock) {
        return 0;
    }
    return 1;
}

static int waitUntil(Mqtt *mq, MqttMsg *msg, MqttWaitFlags wait)
{
    ssize result;

    assert(!rIsMain());
    assert(mq);
    assert(msg);

    if (!mq || !mq->sock) {
        return R_ERR_NOT_CONNECTED;
    }
    if (!(wait & (MQTT_WAIT_SENT | MQTT_WAIT_ACK))) {
        return 0;
    }
    msg->wait = wait;
    msg->fiber = rGetFiber();
    msg->hold = 1;

    result = (ssize) rYieldFiber(0);
    msg->hold = 0;
    freeMsg(msg);

    if (result < 0) {
        return R_ERR_NOT_CONNECTED;
    }
    return 0;
}

//  Could wait flags also have flag for retained?
PUBLIC int mqttPublish(Mqtt *mq, cvoid *buf, ssize bufsize, int qos, MqttWaitFlags wait, cchar *topic, ...)
{
    va_list ap;
    char    topicBuf[MQTT_TOPIC_SIZE];

    if (mq == 0) {
        rError("mqtt", "Publish on bad Mqtt object");
        return R_ERR_BAD_ARGS;
    }
    assert(mq);
    assert(buf);
    assert(topic);

    va_start(ap, topic);
    sfmtbufv(topicBuf, sizeof(topicBuf), topic, ap);
    va_end(ap);

    return publish(mq, buf, bufsize, qos, wait, 0, topicBuf);
}

PUBLIC int mqttPublishRetained(Mqtt *mq, cvoid *buf, ssize bufsize, int qos, MqttWaitFlags wait, cchar *topic, ...)
{
    va_list ap;
    char    topicBuf[MQTT_TOPIC_SIZE];

    assert(mq);
    assert(buf);
    assert(bufsize > 0);
    assert(topic);

    va_start(ap, topic);
    sfmtbufv(topicBuf, sizeof(topicBuf), topic, ap);
    va_end(ap);

    return publish(mq, buf, bufsize, qos, wait, MQTT_RETAIN, topicBuf);
}

static int publish(Mqtt *mq, cvoid *buf, ssize bufsize, int qos, MqttWaitFlags wait, int retain, cchar *topic)
{
    MqttMsg *msg;
    MqttHdr hdr;
    Ticks   decay, elapsed, now;
    uchar   *bp;
    int     flags, id, rc, length;

    assert(mq);
    assert(buf);
    assert(topic);

    if (!onDemandAttach(mq)) {
        return R_ERR_CANT_WRITE;
    }
    id = getId(mq);

    if (topic == NULL) {
        return R_ERR_BAD_NULL;
    }
    flags = (retain ? MQTT_RETAIN : 0) | (qos << 1 & 0x6);
    if (qos < 0 || qos >= 3) {
        return R_ERR_BAD_ARGS;
    }
    if (bufsize < 0) {
        bufsize = slen(buf);
    }
    if (bufsize > mq->maxMessage) {
        return R_ERR_WONT_FIT;
    }
    hdr.flags = flags;
    hdr.type = MQTT_PACKET_PUBLISH;

    length = getStringLen(topic);
    if (qos > 0) {
        //  Add room for the packet ID for retransmits
        length += 2;
    }
    length += (int) bufsize;
    hdr.length = length;

    if ((msg = allocMsg(mq, hdr.type, id, length)) == 0) {
        return R_ERR_MEMORY;
    }
    msg->qos = qos;
    bp = msg->start;

    rc = packHdr(mq, bp, &hdr);
    if (rc < 0) {
        freeMsg(msg);
        return (int) rc;
    }
    bp += rc;
    bp += packString(bp, topic);
    if (qos > 0) {
        bp += packUnit16(bp, id);
    }
    memcpy(bp, buf, bufsize);
    bp += bufsize;

    if (mq->throttle > 0) {
        now = rGetTicks();
        /*
            Decay by 3% of throttle delay each second, plus 5ms per second
         */
        elapsed = (now - mq->throttleLastPub + TPS - 1);
        decay = (mq->throttle * 3 / 100 * elapsed / 1000) + elapsed * 5 / TPS;

        if (mq->throttle) {
            rInfo("mqtt", "Delay sending message for %lld ms", mq->throttle);
            rSleep(mq->throttle);
        } else {
            rInfo("mqtt", "Throttling restrictions lifted");
        }
        mq->throttle -= decay;
        if (mq->throttle < 0) {
            mq->throttle = 0;
        }
        mq->throttleLastPub = now;
    }
    queueMsg(mq, msg, bp);
    rDebug("mqtt", "Publish message to \"%s\"", topic);
    return waitUntil(mq, msg, wait);
}

/*
    Throttle excessive sending load.
    NOTICE: the terms of service require that this code not be removed or disabled.
 */
PUBLIC void mqttThrottle(Mqtt *mq)
{
    Ticks now;

    /*
        Throttle rising exponentially
     */
    now = rGetTicks();
    mq->throttle = max(mq->throttle * 2, mq->throttle + MqttThrottleMin);
    mq->throttleMark = now;
    mq->throttleLastPub = now;
    if (mq->throttle > MqttThrottleMax) {
        mq->throttle = MqttThrottleMax;
    }
    rError("mqtt", "Device sending too much data, sending throttled for %lld ms", mq->throttle);
}

/*
    Perform a master subscription. Subsequent subscriptions using this topic as a prefix will
    not incurr an MQTT protocol subscription. Ideal to minimize the number of network subscriptions.
 */
PUBLIC int mqttSubscribeMaster(Mqtt *mq, int maxQos, MqttWaitFlags wait, cchar *fmt, ...)
{
    va_list ap;
    char    *topic;
    int     rc;

    va_start(ap, fmt);
    topic = sfmtv(fmt, ap);
    va_end(ap);

    rInfo("mqtt", "Define master MQTT subscription \"%s\"", topic);
    
    if ((rc = mqttSubscribe(mq, NULL, maxQos, wait, "%s", topic)) == 0) {
        if (sends(topic, "/+") || sends(topic, "/#")) {
            topic[slen(topic) - 2] = '\0';
        }
        //  Give topic to list to own and free
        rAddItem(mq->masterTopics, topic);
    } else {
        rFree(topic);
    }
    return rc;
}

PUBLIC int mqttSubscribe(Mqtt *mq, MqttCallback callback, int maxQos, MqttWaitFlags wait, cchar *fmt, ...)
{
    MqttTopic *tp;
    va_list   ap;
    cchar     *masterTopic;
    char      topic[MQTT_TOPIC_SIZE];
    int       mid;

    if (mq == 0) {
        rError("mqtt", "Subscribe on bad Mqtt object");
        return R_ERR_BAD_ARGS;
    }
    va_start(ap, fmt);
    sfmtbufv(topic, sizeof(topic), fmt, ap);
    va_end(ap);

    if (callback) {
        if ((tp = allocTopic(mq, callback, topic, wait)) == 0) {
            return R_ERR_MEMORY;
        }
        rAddItem(mq->topics, tp);
        for (ITERATE_ITEMS(mq->masterTopics, masterTopic, mid)) {
            if (sstarts(topic, masterTopic)) {
                rDebug("mqtt", "Local subscription to \"%s\" via master \"%s\"", topic, masterTopic);
                return 0;
            }
        }
    }
    return subscribe(mq, callback, maxQos, wait, topic);
}

static int subscribe(Mqtt *mq, MqttCallback callback, int maxQos, MqttWaitFlags wait, cchar *topic)
{
    MqttMsg *msg;
    MqttHdr hdr;
    uchar   *bp;
    int     id;
    int     rc;

    if (!onDemandAttach(mq)) {
        return R_ERR_CANT_WRITE;
    }
    hdr.type = MQTT_PACKET_SUB;
    hdr.flags = 2;

    /*
        String length + string + QOS
     */
    hdr.length = 2 + getStringLen(topic) + 1;

    id = getId(mq);
    if ((msg = allocMsg(mq, hdr.type, id, hdr.length)) == 0) {
        return R_ERR_MEMORY;
    }
    bp = msg->start;

    rc = packHdr(mq, bp, &hdr);
    if (rc < 0) {
        freeMsg(msg);
        return rc;
    }
    bp += rc;
    bp += packUnit16(bp, id);
    bp += packString(bp, topic);
    *bp++ = maxQos;
    queueMsg(mq, msg, bp);

    rDebug("mqtt", "Subscribe to \"%s\"", topic);
    if (waitUntil(mq, msg, wait) < 0) {
        return R_ERR_BAD_STATE;
    }
    return 0;
}

PUBLIC int mqttUnsubscribe(Mqtt *mq, cchar *topic, MqttWaitFlags wait)
{
    MqttHdr hdr;
    MqttMsg *msg;
    uchar   *bp;
    int     id;
    int     rc;

    if (!onDemandAttach(mq)) {
        return R_ERR_CANT_WRITE;
    }
    id = getId(mq);

    hdr.type = MQTT_PACKET_UNSUB;
    hdr.flags = 2;
    hdr.length = 2 + getStringLen(topic);

    if ((msg = allocMsg(mq, hdr.type, id, hdr.length)) == 0) {
        return R_ERR_MEMORY;
    }
    bp = msg->start;

    rc = packHdr(mq, bp, &hdr);
    if (rc < 0) {
        freeMsg(msg);
        return rc;
    }
    bp += rc;
    bp += packUnit16(bp, id);
    bp += packString(bp, topic);

    queueMsg(mq, msg, bp);
    freeTopics(mq, topic);

    rDebug("mqtt", "Unsubscribe %s", topic);

    if (waitUntil(mq, msg, wait) < 0) {
        return R_ERR_CANT_CONNECT;
    }
    return 0;
}

PUBLIC int mqttPing(Mqtt *mq)
{
    MqttMsg *msg;
    MqttHdr hdr;
    uchar   *bp;
    int     rc;

    assert(mq);

    if (!onDemandAttach(mq)) {
        return R_ERR_CANT_WRITE;
    }
    hdr.type = MQTT_PACKET_PING;
    hdr.flags = 0;
    hdr.length = 0;

    if ((msg = allocMsg(mq, hdr.type, 0, hdr.length)) == 0) {
        return R_ERR_MEMORY;
    }
    bp = msg->start;

    rc = packHdr(mq, bp, &hdr);
    if (rc < 0) {
        freeMsg(msg);
        return rc;
    }
    bp += rc;
    queueMsg(mq, msg, bp);
    rDebug("mqtt", "Ping");
    return 0;
}

PUBLIC int mqttDisconnect(Mqtt *mq)
{
    MqttHdr hdr;
    MqttMsg *msg;
    uchar   *bp;
    int     rc;

    assert(mq);

    if (!mq->sock) {
        //  No demand attach - no point
        return R_ERR_CANT_WRITE;
    }
    hdr.type = MQTT_PACKET_DISCONNECT;
    hdr.flags = 0;
    hdr.length = 2;

    if ((msg = allocMsg(mq, hdr.type, 0, hdr.length)) == 0) {
        return R_ERR_MEMORY;
    }
    bp = msg->start;

    rc = packHdr(mq, bp, &hdr);
    if (rc < 0) {
        freeMsg(msg);
        return rc;
    }
    queueMsg(mq, msg, bp);
    rTrace("mqtt", "Disconnect");
    return 0;
}

int pubAck(Mqtt *mq, int id)
{
    MqttMsg *msg;

    assert(mq);
    assert(id);

    msg = packPub(mq, MQTT_PACKET_PUB_ACK, id);
    queueMsg(mq, msg, NULL);
    return 0;
}

int pubRec(Mqtt *mq, int id)
{
    MqttMsg *msg;

    assert(mq);
    assert(id);

    msg = packPub(mq, MQTT_PACKET_PUB_REC, id);
    queueMsg(mq, msg, NULL);
    return 0;
}

static int pubRel(Mqtt *mq, int id)
{
    MqttMsg *msg;

    assert(mq);
    assert(id);

    msg = packPub(mq, MQTT_PACKET_PUB_REL, id);
    queueMsg(mq, msg, NULL);
    return 0;
}

static int pubComp(Mqtt *mq, int id)
{
    MqttMsg *msg;

    assert(mq);
    assert(id);

    msg = packPub(mq, MQTT_PACKET_PUB_COMP, id);
    queueMsg(mq, msg, NULL);
    return 0;
}

static int sendMsgs(Mqtt *mq)
{
    MqttMsg *msg, *next;
    Ticks   now;
    int     written;
    int     wait, rc, send, qos2 = 0;

    assert(mq);

    if (mq->error) {
        //  A connection fatal error has occurred.
        return mq->error;
    }
    now = rGetTicks();

    for (msg = mq->head.next; msg != &mq->head; msg = next) {
        send = 0;
        next = msg->next;

        if (mq->connected || msg->type == MQTT_PACKET_CONNECT) {
            if (msg->state == MQTT_UNSENT) {
                send = 1;
            } else if (msg->state == MQTT_AWAITING_ACK && now > (msg->sent + mq->msgTimeout)) {
                //  Retransmit
                msg->start = msg->buf;
                send = 1;
            }
        }
        /*
            Only send QoS 2 message if there are no inflight QoS 2 PUBLISH messages
         */
        if (msg->type == MQTT_PACKET_PUBLISH && (msg->state == MQTT_UNSENT || msg->state == MQTT_AWAITING_ACK)) {
            if (msg->qos == 2) {
                if (qos2) {
                    send = 0;
                }
                qos2 = 1;
            }
        }
        if (!send) {
            continue;
        }
        assert(msg->start < msg->end);
        mq->lastActivity = rGetTicks();

        written = (int) rWriteSocketSync(mq->sock, msg->start, msg->end - msg->start);

        if (written < 0) {
            rError("mqtt", "Error writing to mqtt: %d", written);
            return setError(mq, R_ERR_NETWORK, "Cannot write to socket: errno %d", rGetOsError());

        } else if (written > 0) {
            rDebug("mqtt", "Wrote %d bytes to mqtt", written);
            msg->start += written;
        }
        if (msg->start < msg->end) {
            //  Partial send
            mq->mask |= R_WRITABLE;
            break;
        }
        /*
            Whole message has been sent
         */
        msg->sent = now;
        wait = msg->wait;

        if ((rc = processSentMsg(mq, msg)) != 0) {
            return rc;
        }
        if (wait == MQTT_WAIT_SENT) {
            rResumeFiber(msg->fiber, 0);
        }
    }
    return 0;
}

static int processSentMsg(Mqtt *mq, MqttMsg *msg)
{
#if ME_R_DEBUG_LOGGING
    rDebug("mqtt", "Sent message \"%s\"", packetTypes[msg->type]);
#endif

    assert(mq);
    assert(msg);

    switch (msg->type) {
    case MQTT_PACKET_PUB_ACK:
    case MQTT_PACKET_PUB_COMP:
    case MQTT_PACKET_DISCONNECT:
        setState(mq, msg, MQTT_COMPLETE);
        break;

    case MQTT_PACKET_PUBLISH:
        if (msg->qos == 0) {
            setState(mq, msg, MQTT_COMPLETE);
        } else if (msg->qos == 1) {
            setState(mq, msg, MQTT_AWAITING_ACK);
            //  set DUP flag for subsequent sends [Spec MQTT-3.3.1-1]
            msg->start[0] |= MQTT_DUP;
        } else {
            setState(mq, msg, MQTT_AWAITING_ACK);
        }
        break;

    case MQTT_PACKET_CONNECT:
    case MQTT_PACKET_PUB_REC:
    case MQTT_PACKET_PUB_REL:
    case MQTT_PACKET_SUB:
    case MQTT_PACKET_UNSUB:
    case MQTT_PACKET_PING:
        setState(mq, msg, MQTT_AWAITING_ACK);
        break;

    default:
        return setError(mq, R_ERR_BAD_REQUEST, "Bad request type %d", msg->type);
    }
    return 0;
}

static int recvMsgs(Mqtt *mq)
{
    MqttRecv recv;
    RBuf     *buf;
    ssize    bytes, space;
    int      consumed;

    assert(mq);

    buf = mq->buf;

    while (!mq->error) {
        rResetBufIfEmpty(buf);
        space = rGetBufSpace(buf);
        if (space < MQTT_BUF_SIZE) {
            if (rGrowBuf(buf, MQTT_BUF_SIZE) < 0) {
                return setError(mq, R_ERR_MEMORY, "Cannot grow receive buffer");
            }
        }
        bytes = rReadSocketSync(mq->sock, rGetBufStart(buf), rGetBufSpace(buf));
        if (bytes < 0) {
            return setError(mq, R_ERR_NETWORK, "Cannot read from socket, errno %d", rGetOsError());
        }
        if (bytes == 0) {
            break;
        }
        rDebug("mqtt", "Read %zd bytes from mqtt", bytes);
        rAdjustBufEnd(buf, bytes);

        mq->lastActivity = rGetTicks();

        consumed = unpackResp(mq, &recv);
        if (consumed < 0) {
            return setError(mq, consumed, "Cannot unpack response");
        }
        if (consumed == 0) {
            //  Wait for the rest of the data
            return 0;
        }
        rAdjustBufStart(buf, consumed);
        //  Add a null helps debugging. Not required as dataSize determines data length.
        rAddNullToBuf(buf);

        processRecvMsg(mq, &recv);
    }
    return mq->error;
}

static int processRecvMsg(Mqtt *mq, MqttRecv *rp)
{
    MqttMsg   *msg;
    MqttRecv  *arg;
    MqttTopic *tp;
    RFiber    *fiber;
    int       rc, wait;

    assert(mq);
    assert(rp);

#if ME_R_DEBUG_LOGGING
    if (0 < rp->hdr.type && rp->hdr.type < sizeof(packetTypes) / sizeof(char**)) {
        rDebug("mqtt", "Receive message \"%s\"", packetTypes[rp->hdr.type]);
    }
#endif
    rc = 0;

    switch (rp->hdr.type) {
    case MQTT_PACKET_CONN_ACK:
        msg = findMsgByType(mq, MQTT_PACKET_CONNECT);
        if (msg == NULL) {
            rc = setError(mq, R_ERR_BAD_ACK, "Cannot find connect message to acknowledge");
            break;
        }
        if (rp->code == MQTT_CONNACK_ACCEPTED) {
            mq->connected = 1;
        } else {
            mq->connected = 0;
            if (rp->code == MQTT_CONNACK_REFUSED_IDENTIFIER_REJECTED) {
                rc = setError(mq, R_ERR_CANT_COMPLETE, "Connection refused due to client ID");
            } else {
                rc = setError(mq, R_ERR_CANT_CONNECT, "Connection refused");
            }
        }
        wait = msg->wait;
        fiber = msg->fiber;

        setState(mq, msg, MQTT_COMPLETE);

        if (wait == MQTT_WAIT_ACK) {
            rResumeFiber(fiber, (void*) 0);
        }
        break;

    case MQTT_PACKET_PUBLISH:
        // Prepare response: only for QOS 1 or 2 (PUB_ACK vs PUBREC)
        if (rp->qos == 1) {
            rc = pubAck(mq, rp->id);
            if (rc != 0) {
                setError(mq, rc, "Cannot send ack for message");
                break;
            }

        } else if (rp->qos == 2) {
            // check if this is a duplicate
            if (findMsg(mq, MQTT_PACKET_PUB_REC, rp->id) != NULL) {
                break;
            }
            rc = pubRec(mq, rp->id);
            if (rc != 0) {
                setError(mq, rc, "Cannot send rec for message");
                break;
            }
        }
        if ((tp = getTopic(mq, rp)) == 0) {
            rInfo("mqtt", "Ignoring message, not subscribed to %s", rp->topic);
            break;
        }
        if (tp->callback) {
            /*
                Asynchronously notify the subscriber
                The received message struct is on the stack, so must copy message and data.
             */
            rp->mq = mq;
            rp->matched = tp;
            if (tp->wait & MQTT_WAIT_FAST) {
                (rp->matched->callback)(rp);
            } else {
                arg = rAllocType(MqttRecv);
                memcpy(arg, rp, sizeof(MqttRecv));
                arg->topic = rAlloc(rp->topicSize + 1);
                sncopy(arg->topic, rp->topicSize + 1, (char*) rp->topic, rp->topicSize);
                if (rp->dataSize > 0) {
                    if ((arg->data = rAlloc(rp->dataSize + 1)) == 0) {
                        break;
                    }
                    memcpy(arg->data, rp->data, rp->dataSize);
                    arg->data[arg->dataSize] = 0;
                } else {
                    arg->data = 0;
                }
                fiber = rAllocFiber("incoming-mqtt", (RFiberProc) incomingMsg, arg);
                rStartFiber(fiber, 0);
            }
        }
        break;

    case MQTT_PACKET_PUB_ACK:
        msg = findMsg(mq, MQTT_PACKET_PUBLISH, rp->id);
        if (msg == NULL) {
            rc = setError(mq, R_ERR_BAD_ACK, "Ack received for unknown pubAck");
            break;
        } else {
            wait = msg->wait;
            fiber = msg->fiber;
            setState(mq, msg, MQTT_COMPLETE);
            if (wait & MQTT_WAIT_ACK) {
                rResumeFiber(fiber, 0);
            }
        }
        break;

    case MQTT_PACKET_PUB_REC:
        // check if this is a duplicate
        if (findMsg(mq, MQTT_PACKET_PUB_REL, rp->id) != NULL) {
            break;
        }
        msg = findMsg(mq, MQTT_PACKET_PUBLISH, rp->id);
        if (msg == NULL) {
            rc = setError(mq, R_ERR_BAD_ACK, "Unknown ack for pubRec message");
            break;
        }
        setState(mq, msg, MQTT_COMPLETE);
        rc = pubRel(mq, rp->id);
        if (rc != 0) {
            setError(mq, rc, "Cannot send rel for message");
            break;
        }
        break;

    case MQTT_PACKET_PUB_REL:
        msg = findMsg(mq, MQTT_PACKET_PUB_REC, rp->id);
        if (msg == NULL) {
            rc = setError(mq, R_ERR_BAD_ACK, "Unknown ack for pubRel message");
            break;
        }
        setState(mq, msg, MQTT_COMPLETE);
        rc = pubComp(mq, rp->id);
        if (rc != 0) {
            setError(mq, rc, "Cannot send pubRel message");
            break;
        }
        break;

    case MQTT_PACKET_PUB_COMP:
        msg = findMsg(mq, MQTT_PACKET_PUB_REL, rp->id);
        if (msg == NULL) {
            rc = setError(mq, R_ERR_BAD_ACK, "Unknown ack for pubComp message");
            break;
        }
        setState(mq, msg, MQTT_COMPLETE);
        break;

    case MQTT_PACKET_SUB_ACK:
        msg = findMsg(mq, MQTT_PACKET_SUB, rp->id);
        if (msg == NULL) {
            setError(mq, R_ERR_BAD_ACK, "Unknown ack for subAck message");
            break;
        }
        wait = msg->wait;
        fiber = msg->fiber;

        setState(mq, msg, MQTT_COMPLETE);
        // check that subscription was successful (not currently only one subscribe at a time)
        if (rp->codes[0] == MQTT_SUBACK_FAILURE) {
            rc = setError(mq, R_ERR_CANT_COMPLETE, "Subscribe failed");
        }
        if (wait & MQTT_WAIT_ACK) {
            rResumeFiber(fiber, 0);
        }
        break;

    case MQTT_PACKET_UNSUB_ACK:
        msg = findMsg(mq, MQTT_PACKET_UNSUB, rp->id);
        if (msg == NULL) {
            rc = setError(mq, R_ERR_BAD_ACK, "Unknown ack for unsubAck message");
        } else {
            wait = msg->wait;
            fiber = msg->fiber;
            setState(mq, msg, MQTT_COMPLETE);
            if (wait & MQTT_WAIT_ACK) {
                rResumeFiber(fiber, 0);
            }
        }
        break;

    case MQTT_PACKET_PING_ACK:
        msg = findMsgByType(mq, MQTT_PACKET_PING);
        if (msg == NULL) {
            rc = setError(mq, R_ERR_BAD_ACK, "Unknown ack for pingResp message");
        } else {
            wait = msg->wait;
            fiber = msg->fiber;
            setState(mq, msg, MQTT_COMPLETE);
            if (wait & MQTT_WAIT_ACK) {
                rResumeFiber(fiber, 0);
            }
        }
        break;

    default:
        rc = setError(mq, R_ERR_BAD_RESPONSE, "Bad response message");
        break;
    }
    return rc;
}

/*
    This is run via its own fiber
 */
static void incomingMsg(MqttRecv *rp)
{
    assert(rp && rp->matched && rp->matched->callback);

    (rp->matched->callback)(rp);

    rFree(rp->data);
    rFree(rp->topic);
    rFree(rp);
}

static MqttTopic *allocTopic(Mqtt *mq, MqttCallback callback, cchar *topic, MqttWaitFlags wait)
{
    MqttTopic *tp;

    assert(mq);
    assert(topic);
    assert(callback);

    tp = rAllocType(MqttTopic);
    tp->topic = sclone(topic);
    tp->segments = splitTopic(topic, slen(topic), &tp->segbuf);
    tp->callback = callback;
    tp->wait = wait;
    return tp;
}

static void freeTopics(Mqtt *mq, cchar *topic)
{
    MqttTopic *tp;
    int       next;

    assert(mq);

    for (ITERATE_ITEMS(mq->topics, tp, next)) {
        if (!topic || smatch(topic, tp->topic)) {
            freeTopic(tp);
        }
    }
    rClearList(mq->topics);
}

static void freeTopic(MqttTopic *tp)
{
    assert(tp);

    rFree(tp->topic);
    rFree(tp->segments);
    rFree(tp->segbuf);
    rFree(tp);
}

#if KEEP
static void showTopics(Mqtt *mq)
{
    MqttTopic *tp;
    int       next;

    for (ITERATE_ITEMS(mq->topics, tp, next)) {
        printf("TOPIC %s\n", tp->topic);
        printf("  SEG %s\n", tp->segments[0]);
    }
}
#endif

static MqttTopic *getTopic(Mqtt *mq, MqttRecv *rp)
{
    MqttTopic *tp;
    cchar     **segments;
    char      *segbuf;
    int       next;

    assert(mq);
    assert(rp);

    if ((segments = splitTopic(rp->topic, rp->topicSize, &segbuf)) == 0) {
        return 0;
    }
    for (ITERATE_ITEMS(mq->topics, tp, next)) {
        if (matchTopic(tp, segments)) {
            rFree(segbuf);
            rFree(segments);
            return tp;
        }
    }
    rFree(segbuf);
    rFree(segments);
    return 0;
}

static cchar **splitTopic(cchar *topic, ssize topicLen, char **segbuf)
{
    cchar *cp;
    char  *next, **segments;
    int   i, count;

    assert(topic);
    assert(segbuf);

    assert(topic);
    for (count = 1, cp = topic; cp < &topic[topicLen]; cp++) {
        if (*cp == '/') count++;
    }
    segments = rAlloc((count + 1) * sizeof(char*));
    *segbuf = sclone(topic);

    for (i = 0, next = *segbuf; next != 0; i++) {
        segments[i] = stok(next, "/", &next);
    }
    segments[i] = 0;
    return (cchar**) segments;
}

static bool matchTopic(MqttTopic *tp, cchar **segments)
{
    cchar **ip, **mp;

    assert(tp);
    assert(segments);

    for (mp = tp->segments, ip = segments; *mp && *ip; mp++, ip++) {
        if (*mp[0] == '#') {
            mp++;
            break;
        } else if (*mp[0] == '+') {
            continue;
        } else if (!smatch(*mp, *ip)) {
            return 0;
        }
    }
    if (*mp) {
        return 0;
    }
    return 1;
}

static int checkHdr(Mqtt *mq, MqttHdr *hdr)
{
    uchar flags, type, requiredFlags, requiredFlagsMask;

    assert(mq);

    // get value and rules
    type = hdr->type;
    flags = hdr->flags;
    requiredFlags = MqttHdr_rules.requiredFlags[type];
    requiredFlagsMask = MqttHdr_rules.requiredFlagsMask[type];

    // check for valid type
    if (!MqttHdr_rules.typeIsValid[type]) {
        return setError(mq, R_ERR_BAD_DATA, "Invalid type in header");
    }
    // check that flags are appropriate
    if (MQTT_BITFIELD_RULE_VIOLOATION(flags, requiredFlags, requiredFlagsMask)) {
        return setError(mq, R_ERR_BAD_STATE, "Invalid flags");
    }
    return 0;
}

static int unpackRespHdr(Mqtt *mq, MqttRecv *rp)
{
    MqttHdr *hdr;
    RBuf    *buf;
    cuchar  *bp, *end, *start;
    uchar   c;
    int     err;
    int     shift;

    assert(rGetBufLength(mq->buf) > 0);

    buf = mq->buf;
    bp = start = (cuchar*) rGetBufStart(buf);
    end = (cuchar*) rGetBufEnd(buf);

    hdr = &(rp->hdr);
    hdr->length = 0;
    hdr->type = *bp >> 4;
    hdr->flags = *bp++ & 0x0F;

    shift = 0;
    do {
        if (shift >= 28) {
            return setError(mq, R_ERR_BAD_RESPONSE, "Cannot unpack response header");
        }
        if (bp >= end) {
            //  end of input
            return 0;
        }
        c = (uchar) * bp++;
        hdr->length += (c & 0x7F) << shift;
        shift += 7;
    } while (c & 0x80);

    if ((err = checkHdr(mq, hdr)) != 0) {
        return err;
    }
    if (&bp[hdr->length] > end) {
        //  Have not read all remaining input
        return 0;
    }
    return (int) (bp - start);
}

static int packHdr(Mqtt *mq, uchar *start, MqttHdr *hdr)
{
    uchar    *bp;
    int      err;
    uint32 length;

    if (!hdr || !start) {
        return R_ERR_BAD_NULL;
    }
    err = checkHdr(mq, hdr);
    if (err) {
        return err;
    }
    bp = start;
    *bp =  (((uchar) hdr->type) << 4) & 0xF0;
    *bp++ |= ((uchar) hdr->flags) & 0x0F;

    /*
        Length is the size of the remaining portion of the message after encoding the fixed header and packet length
     */
    if (hdr->length >= mq->maxMessage) {
        return setError(mq, R_ERR_WONT_FIT, "Message too big");
    }
    length = hdr->length;
    do {
        *bp++ = length & 0x7F;
        if (length > 127) {
            bp[-1] |= 0x80;
        }
        length = length >> 7;
    } while (length > 0);

    return (int) (bp - start);
}

static MqttMsg *allocMsg(Mqtt *mq, int type, int id, ssize size)
{
    MqttMsg *msg;

    if ((msg = rAllocType(MqttMsg)) == NULL) {
        return NULL;
    }
    //  Fixed header is at least 7 bytes
    size += 7;
    if (size <= MQTT_INLINE_BUF_SIZE) {
        msg->buf = msg->inlineBuf;
    } else {
        msg->buf = rAlloc(size);
    }
    msg->end = msg->start = msg->buf;
    msg->endbuf = &msg->buf[size];
    msg->type = type;
    msg->id = id;
    msg->state = MQTT_UNSENT;
    return msg;
}

static void freeMsg(MqttMsg *msg)
{
    if (msg && !msg->hold) {
        if (msg->buf != msg->inlineBuf) {
            rFree(msg->buf);
        }
        rFree(msg);
    }
}

int unpackConn(Mqtt *mq, MqttRecv *rp, cuchar *start)
{
    cuchar *bp;

    bp = start;

    if (rp->hdr.length != 2) {
        return setError(mq, R_ERR_BAD_VALUE, "Bad header length");
    }
    if (*bp & 0xFE) {
        // only bit 1 can be set
        return setError(mq, R_ERR_BAD_VALUE, "Bad conn ack value");
    } else {
        rp->hasSession = *bp++;
    }
    if (*bp > 5) {
        // only bit 1 can be set
        return setError(mq, R_ERR_BAD_VALUE, "Bad conn ack value");
    } else {
        rp->code = (MqttConnCode) * bp++;
    }
    return (int) (bp - start);
}

/*
    Receive and unpack a publish message
 */
int unpackPublish(Mqtt *mq, MqttRecv *rp, cuchar *start)
{
    MqttHdr *hdr;
    cuchar  *bp;
    int     topicSize;

    bp = start;
    hdr = &(rp->hdr);

    rp->dup = (hdr->flags & MQTT_DUP) >> 3;
    rp->qos = (hdr->flags & MQTT_QOS_FLAGS_MASK) >> 1;
    rp->retain = hdr->flags & MQTT_RETAIN;

    if (rp->hdr.length < 4) {
        return setError(mq, R_ERR_BAD_RESPONSE, "Bad received message length");
    }
    topicSize = unpackUint16(bp);
    bp += 2;
    rp->topic = (char*) bp;
    rp->topicSize = topicSize;
    bp += topicSize;

    if (rp->qos > 0) {
        rp->id = unpackUint16(bp);
        bp += 2;
    }
    rp->data = (char*) bp;
    if (rp->qos == 0) {
        rp->dataSize = hdr->length - topicSize - 2;
    } else {
        rp->dataSize = hdr->length - topicSize - 4;
    }
    bp += rp->dataSize;
    return (int) (bp - start);
}

MqttMsg *packPub(Mqtt *mq, MqttPacketType type, int id)
{
    MqttHdr hdr;
    MqttMsg *msg;
    uchar   *bp;
    int     length, rc;

    hdr.type = type;
    hdr.flags = (type == MQTT_PACKET_PUB_REL) ? 0x02 : 0;

    /*
        Variable portion: ID
     */
    length = 2;
    hdr.length = length;

    if ((msg = allocMsg(mq, type, id, length)) == 0) {
        return 0;
    }
    bp = msg->start;

    rc = packHdr(mq, bp, &hdr);
    if (rc < 0) {
        freeMsg(msg);
        return 0;
    }
    bp += rc;
    bp += packUnit16(bp, id);
    msg->end = bp;

    return msg;
}

int unpackPub(Mqtt *mq, MqttRecv *rp, cuchar *start)
{
    uint16 id;
    cuchar   *bp;

    bp = start;
    if (rp->hdr.length != 2) {
        return R_ERR_BAD_RESPONSE;
    }
    id = unpackUint16(bp);
    bp += 2;
    rp->id = id;
    return (int) (bp - start);
}

int unpackSuback(Mqtt *mq, MqttRecv *rp, cuchar *start)
{
    int    length;
    cuchar *bp;

    bp = start;
    length = rp->hdr.length;
    if (length < 3) {
        return R_ERR_BAD_RESPONSE;
    }
    rp->id = unpackUint16(bp);
    bp += 2;
    length -= 2;

    rp->numCodes = length;
    rp->codes = bp;
    bp += length;
    return (int) (bp - start);
}

int unpackUnsubAck(Mqtt *mq, MqttRecv *rp, cuchar *start)
{
    cuchar *bp;

    bp = start;
    if (rp->hdr.length != 2) {
        return R_ERR_BAD_RESPONSE;
    }
    rp->id = unpackUint16(bp);
    bp += 2;
    return (int) (bp - start);
}

int unpackResp(Mqtt *mq, MqttRecv *rp)
{
    cuchar *bp, *start;
    int    rc;

    memset(rp, 0, sizeof(MqttRecv));
    start = (cuchar*) rGetBufStart(mq->buf);

    if ((rc = unpackRespHdr(mq, rp)) <= 0) {
        return rc;
    }
    bp = &start[rc];

    switch (rp->hdr.type) {
    case MQTT_PACKET_CONN_ACK:
        rc = unpackConn(mq, rp, bp);
        break;
    case MQTT_PACKET_PUBLISH:
        rc = unpackPublish(mq, rp, bp);
        break;
    case MQTT_PACKET_PUB_ACK:
        rc = unpackPub(mq, rp, bp);
        break;
    case MQTT_PACKET_PUB_REC:
        rc = unpackPub(mq, rp, bp);
        break;
    case MQTT_PACKET_PUB_REL:
        rc = unpackPub(mq, rp, bp);
        break;
    case MQTT_PACKET_PUB_COMP:
        rc = unpackPub(mq, rp, bp);
        break;
    case MQTT_PACKET_SUB_ACK:
        rc = unpackSuback(mq, rp, bp);
        break;
    case MQTT_PACKET_UNSUB_ACK:
        rc = unpackUnsubAck(mq, rp, bp);
        break;
    case MQTT_PACKET_PING_ACK:
        return rc;
    default:
        return setError(mq, R_ERR_BAD_RESPONSE, "Bad response");
    }
    if (rc < 0) {
        return rc;
    }
    bp += rc;
    return (int) (bp - start);
}

int packUnit16(uchar *bp, uint16 integer)
{
    uint16 integer_htons = MQTT_HTONS(integer);

    memcpy(bp, &integer_htons, 2);
    return 2;
}

uint16 unpackUint16(cuchar *bp)
{
    uint16 integer_htons;

    memcpy(&integer_htons, bp, 2);
    return MQTT_NTOHS(integer_htons);
}

int packString(uchar *bp, cchar *str)
{
    int length;
    int i;

    length = (int) strlen(str);
    bp += packUnit16(bp, length);
    for (i = 0; i < length; ++i) {
        *(bp++) = str[i];
    }
    return length + 2;
}

/*
    Get the next message ID
    LFSR taps taken from: https://en.wikipedia.org/wiki/Linear-feedback_shift_register
 */
static int getId(Mqtt *mq)
{
    MqttMsg *msg;
    uint    lsb;
    int     exist = 0;

    if (mq->nextId == 0) {
        mq->nextId = 163;
    }
    do {
        lsb = mq->nextId & 1;
        (mq->nextId) >>= 1;
        if (lsb) {
            mq->nextId ^= 0xB400u;
        }
        // check that the ID is unique
        exist = 0;
        for (msg = mq->head.next; msg != &mq->head; msg = msg->next) {
            if (msg->id == mq->nextId) {
                exist = 1;
                break;
            }
        }

    } while (exist);
    return mq->nextId;
}

static void queueMsg(Mqtt *mq, MqttMsg *msg, uchar *end)
{
    msg->next = &mq->head;
    msg->prev = mq->head.prev;
    mq->head.prev->next = msg;
    mq->head.prev = msg;

    //  Convenience to set the end of message data
    if (end) {
        msg->end = end;
    }
    rSetWaitMask(mq->sock->wait, R_IO, rGetTicks() + MQTT_WAIT_TIMEOUT);
}

static void dequeueMsg(Mqtt *mq, MqttMsg *msg)
{
    msg->prev->next = msg->next;
    msg->next->prev = msg->prev;
    if (!(msg->wait & (MQTT_WAIT_SENT | MQTT_WAIT_ACK))) {
        freeMsg(msg);
    }
}

PUBLIC int mqttMsgsToSend(Mqtt *mq)
{
    MqttMsg *msg;
    Ticks   now;
    int     count;

    now = rGetTicks();
    count = 0;
    for (msg = mq->head.next; msg != &mq->head; msg = msg->next) {
        if (msg->state == MQTT_UNSENT ||
            (msg->state == MQTT_AWAITING_ACK && (now > (msg->sent + mq->msgTimeout)))) {
            count++;
        }
    }
    return count;
}

PUBLIC int mqttGetQueueCount(Mqtt *mq)
{
    MqttMsg *msg;
    int     count;

    for (count = 0, msg = mq->head.next; msg != &mq->head; msg = msg->next) {
        count++;
    }
    return count;
}

static MqttMsg *findMsg(Mqtt *mq, MqttPacketType type, int id)
{
    MqttMsg *msg;

    assert(mq);

    for (msg = mq->head.next; msg != &mq->head; msg = msg->next) {
        if (msg->type == type) {
            if (id == msg->id) {
                return msg;
            }
        }
    }
    return 0;
}

static MqttMsg *findMsgByType(Mqtt *mq, MqttPacketType type)
{
    MqttMsg *msg;

    assert(mq);
    for (msg = mq->head.next; msg != &mq->head; msg = msg->next) {
        if (msg->type == type && msg->state != MQTT_COMPLETE) {
            return msg;
        }
    }
    return 0;
}

PUBLIC void mqttSetWill(Mqtt *mq, cchar *topic, cvoid *msg, ssize size)
{
    assert(topic && msg && size > 0);

    rFree(mq->willTopic);
    rFree(mq->willMsg);
    mq->willTopic = sclone(topic);
    mq->willMsg = sclone(msg);
    mq->willMsgSize = size;
}

static void setState(Mqtt *mq, MqttMsg *msg, int state)
{
    assert(mq);
    assert(msg);

    msg->state = state;
    if (msg->state == MQTT_COMPLETE) {
        dequeueMsg(mq, msg);
    }
}

/*
    Set an error condition. Only used for errors that are fatal to the connection.
    User should reconnect when a network close event is detected as a result.
 */
static int setError(Mqtt *mq, int error, cchar *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    mq->error = error;
    mq->errorMsg = sfmtv(fmt, ap);
    if (error != R_ERR_NETWORK) {
        rError("mqtt", "Mqtt error %d: %s. Closing socket.", mq->error, mq->errorMsg);
    }
    rCloseSocket(mq->sock);
    va_end(ap);
    return error;
}

PUBLIC bool mqttIsConnected(Mqtt *mq)
{
    if (!mq || !mq->connected) {
        return 0;
    }
    if (rIsSocketClosed(mq->sock)) {
        return 0;
    }
    return 1;
}

PUBLIC cchar *mqttGetError(Mqtt *mq)
{
    assert(mq);
    return rGetError(mq->error);
}

static int getStringLen(cchar *s)
{
    assert(s);

    return (int) (2 + strlen(s));
}

/*
    AWS supports a smaller 128K max message size
 */
PUBLIC void mqttSetMessageSize(Mqtt *mq, int size)
{
    assert(mq);
    assert(size > 0);

    mq->maxMessage = size;
}

PUBLIC void mqttSetKeepAlive(Mqtt *mq, Ticks keepAlive)
{
    if (keepAlive <= 0) {
        keepAlive = MQTT_KEEP_ALIVE;
    }
    if (keepAlive >= MAXINT64) {
        keepAlive /= 2;
    }
    mq->keepAlive = keepAlive;
}

PUBLIC void mqttSetTimeout(Mqtt *mq, Ticks timeout)
{
    if (timeout < 0) {
        timeout = MQTT_TIMEOUT;
    } else if (timeout == 0) {
        timeout = MAXINT64;
    }
    if (timeout >= MAXINT64) {
        //  To prevent integer overflows when doing simple date math
        timeout /= 10;
    }
    mq->timeout = timeout;
}

PUBLIC Time mqttGetLastActivity(Mqtt *mq)
{
    return mq->lastActivity;
}

#else
PUBLIC void dummyMqtt(void)
{
}
#endif /* ME_COM_MQTT */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

#else
void dummyMqtt(){}
#endif /* ME_COM_MQTT */
