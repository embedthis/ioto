/*
    lifecycle.tst.c - Argument validation, error state and object lifecycle. No broker needed.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "mqtt-test.h"

/************************************ Code ************************************/

static void onMessage(const MqttRecv *rp)
{
}

/*
    Replacing credentials with values that are too long leaves the previous values in place
 */
static void testCredentialsReplace(void)
{
    Mqtt *mq;
    char longUser[MQTT_MAX_USERNAME_SIZE + 10];

    mq = mqttAlloc("life-creds", NULL);
    tcall(mqttSetCredentials(mq, "u1", "p1"), 0);
    memset(longUser, 'U', sizeof(longUser));
    longUser[sizeof(longUser) - 1] = '\0';
    tcall(mqttSetCredentials(mq, longUser, "p2"), R_ERR_BAD_ARGS);
    tmatch(mq->username, "u1");
    tmatch(mq->password, "p1");
    tcall(mqttSetCredentials(mq, "u2", longUser), R_ERR_BAD_ARGS);
    tmatch(mq->username, "u1");
    tmatch(mq->password, "p1");
    tcall(mqttSetCredentials(mq, "u3", "p3"), 0);
    tmatch(mq->username, "u3");
    tmatch(mq->password, "p3");
    mqttFree(mq);
}

/*
    The will message is carried in the CONNECT packet with a two byte length
 */
static void testWillLimits(void)
{
    Mqtt *mq;
    char *msg;

    mq = mqttAlloc("life-will", NULL);
    msg = rAlloc(0x10000);
    memset(msg, 'w', 0x10000);
    tcall(mqttSetWill(mq, "will", msg, 0x10000), R_ERR_BAD_ARGS);
    tnull(mq->willTopic);
    tcall(mqttSetWill(mq, "will", msg, 0xFFFF), 0);
    teq((int) mq->willMsgSize, 0xFFFF);
    tcall(mqttSetWill(mq, "will", "short", 5), 0);
    teq((int) mq->willMsgSize, 5);
    tcall(mqttSetWill(mq, NULL, "x", 1), R_ERR_BAD_ARGS);
    tcall(mqttSetWill(mq, "will", NULL, 1), R_ERR_BAD_ARGS);
    rFree(msg);
    mqttFree(mq);
}

/*
    Once an error is recorded, operations fail with that error
 */
static void testErrorState(void)
{
    Mqtt *mq;

    mq = mqttAlloc("life-error", NULL);
    mq->error = R_ERR_NETWORK;
    tcall(mqttPublish(mq, "x", 1, 0, MQTT_WAIT_NONE, "t"), R_ERR_NETWORK);
    tcall(mqttPublishRetained(mq, "x", 1, 0, MQTT_WAIT_NONE, "t"), R_ERR_NETWORK);
    tcall(mqttSubscribe(mq, onMessage, 1, MQTT_WAIT_NONE, "t"), R_ERR_NETWORK);
    tcall(mqttPing(mq), R_ERR_NETWORK);
    tmatch(mqttGetError(mq), rGetError(R_ERR_NETWORK));
    mq->error = 0;
    mqttFree(mq);
}

/*
    Topic filters are validated before anything is sent, and a bad filter is not a connection error
 */
static void testTopicValidation(void)
{
    static cchar *bad[] = { "a#", "a/#/b", "#a", "a+", "+a", "a/+b", "a/b+", "a\x01" "b", "a\x7f", "\t", NULL };
    Mqtt         *mq;
    int          i;

    mq = mqttAlloc("life-topic", NULL);
    for (i = 0; bad[i]; i++) {
        tcall(mqttSubscribe(mq, onMessage, 1, MQTT_WAIT_NONE, "%s", bad[i]), R_ERR_BAD_ARGS);
        tcall(mqttSubscribeMaster(mq, 1, MQTT_WAIT_NONE, "%s", bad[i]), R_ERR_BAD_ARGS);
        teq(mq->error, 0);
    }
    teq(rGetListLength(mq->topics), 0);
    teq(rGetListLength(mq->masterTopics), 0);
    mqttFree(mq);
}

static void testNullTolerance(void)
{
    Mqtt *mq;

    tcall(mqttSetWill(NULL, "t", "m", 1), R_ERR_BAD_ARGS);
    tcall(mqttUnsubscribe(NULL, "t", MQTT_WAIT_NONE), R_ERR_BAD_ARGS);
    tcall(mqttUnsubscribeMaster(NULL, "t", MQTT_WAIT_NONE), R_ERR_BAD_ARGS);
    tcall(mqttSubscribeMaster(NULL, 1, MQTT_WAIT_NONE, "t"), R_ERR_BAD_ARGS);
    teq(mqttGetQueueCount(NULL), 0);
    tfalse(mqttCheckQueue(NULL));
    tcall(mqttPing(NULL), R_ERR_BAD_ARGS);
    tcall(mqttDisconnect(NULL), R_ERR_BAD_ARGS);
    tcall(mqttConnect(NULL, NULL, 0, MQTT_WAIT_NONE), R_ERR_BAD_ARGS);
    tcall(mqttSetCredentials(NULL, "u", "p"), R_ERR_BAD_ARGS);

    mq = mqttAlloc("life-null", NULL);
    tcall(mqttUnsubscribe(mq, NULL, MQTT_WAIT_NONE), R_ERR_BAD_ARGS);
    tcall(mqttUnsubscribeMaster(mq, NULL, MQTT_WAIT_NONE), R_ERR_BAD_ARGS);
    teq(mqttGetQueueCount(mq), 0);
    ttrue(mqttCheckQueue(mq));
    mqttSetMessageSize(mq, 0);
    teq((int) mq->maxMessage, MQTT_MAX_MESSAGE_SIZE);
    mqttFree(mq);
}

static void fiberMain(void *data)
{
    testCredentialsReplace();
    testWillLimits();
    testErrorState();
    testTopicValidation();
    testNullTolerance();
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
