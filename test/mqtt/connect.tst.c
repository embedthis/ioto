/*
    connect.c.tst - MQTT connection tests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "mqtt.h"

/************************************ Code ************************************/

static void testMqttConnectNullSocket(void)
{
    Mqtt *mq;
    int  rc;

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, NULL, 0, MQTT_WAIT_NONE);
    tfalse(rc == 0);

    mqttFree(mq);
}

static void testMqttConnectEmptyIdCleanSession(void)
{
    Mqtt *mq;

    mq = mqttAlloc("", NULL);
    tfalse(mq);

    mq = mqttAlloc("test", NULL);
    ttrue(mq);
    mqttFree(mq);
}

static void testMqttConnectEmptyIdNoCleanSession(void)
{
    Mqtt *mq;

    mq = mqttAlloc("", NULL);
    tfalse(mq);
}

static void testMqttConnectWithCredentials(void)
{
    Mqtt *mq;
    int  rc;

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttSetCredentials(mq, "testuser", "testpass");
    teq(rc, 0);
    tmatch(mq->username, "testuser");
    tmatch(mq->password, "testpass");

    mqttFree(mq);
}

static void testMqttConnectWithWill(void)
{
    Mqtt *mq;
    char *willMsg = "device offline";
    int  rc;

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttSetWill(mq, "device/status", willMsg, slen(willMsg));
    teq(rc, 0);
    tmatch(mq->willTopic, "device/status");
    teq(mq->willMsgSize, slen(willMsg));

    mqttFree(mq);
}

static void testMqttDisconnectNoSocket(void)
{
    Mqtt *mq;
    int  rc;

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttDisconnect(mq);
    teq(rc, R_ERR_CANT_WRITE);

    mqttFree(mq);
}

static void testMqttPingNoSocket(void)
{
    Mqtt *mq;
    int  rc;

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttPing(mq);
    teq(rc, R_ERR_CANT_WRITE);

    mqttFree(mq);
}

static void testMqttGetError(void)
{
    Mqtt  *mq;
    cchar *err;

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    err = mqttGetError(mq);
    ttrue(err);

    mqttFree(mq);
}

static void testMqttMsgsToSend(void)
{
    Mqtt *mq;
    int  count;

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    count = mqttMsgsToSend(mq);
    teq(count, 0);

    mqttFree(mq);
}

static void testMqttGetLastActivity(void)
{
    Mqtt  *mq;
    Ticks activity;

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    activity = mqttGetLastActivity(mq);
    ttrue(activity > 0);

    mqttFree(mq);
}

static void fiberMain(void *data)
{
    testMqttConnectNullSocket();
    testMqttConnectEmptyIdCleanSession();
    testMqttConnectEmptyIdNoCleanSession();
    testMqttConnectWithCredentials();
    testMqttConnectWithWill();
    testMqttDisconnectNoSocket();
    testMqttPingNoSocket();
    testMqttGetError();
    testMqttMsgsToSend();
    testMqttGetLastActivity();
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
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */