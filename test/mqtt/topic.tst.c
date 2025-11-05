/*
    topic.c.tst - MQTT topic matching and wildcard tests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "mqtt.h"

/*********************************** Locals ***********************************/

static int  messageCount = 0;
static char *lastTopic = NULL;
static char *lastMessage = NULL;

static RSocket *createConnectedSocket(void)
{
    RSocket *sock = rAllocSocket();

    if (rConnectSocket(sock, "localhost", 1883, 0) < 0) {
        rFreeSocket(sock);
        return NULL;
    }
    return sock;
}

static void messageCallback(const MqttRecv *resp)
{
    messageCount++;
    rFree(lastTopic);
    rFree(lastMessage);

    if (resp->topic && resp->topicSize > 0) {
        lastTopic = rAlloc(resp->topicSize + 1);
        memcpy(lastTopic, resp->topic, resp->topicSize);
        lastTopic[resp->topicSize] = '\0';
    }

    if (resp->data && resp->dataSize > 0) {
        lastMessage = rAlloc(resp->dataSize + 1);
        memcpy(lastMessage, resp->data, resp->dataSize);
        lastMessage[resp->dataSize] = '\0';
    }
}

/************************************ Code ************************************/

static void testTopicExactMatch(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    sock = createConnectedSocket();
    ttrue(sock);

    mq = mqttAlloc("exact-match-client", NULL);
    ttrue(mq);

    if (mqttConnect(mq, sock, 0, MQTT_WAIT_ACK) == 0) {
        rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test/topic");
        teq(rc, 0);
    }

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testTopicSingleLevelWildcard(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    sock = createConnectedSocket();
    ttrue(sock);

    mq = mqttAlloc("single-wildcard-client", NULL);
    ttrue(mq);

    if (mqttConnect(mq, sock, 0, MQTT_WAIT_ACK) == 0) {
        rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test/+");
        teq(rc, 0);

        rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "+/topic");
        teq(rc, 0);

        rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test/+/status");
        teq(rc, 0);
    }

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testTopicMultiLevelWildcard(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    sock = createConnectedSocket();
    ttrue(sock);

    mq = mqttAlloc("multi-wildcard-client", NULL);
    ttrue(mq);

    if (mqttConnect(mq, sock, 0, MQTT_WAIT_ACK) == 0) {
        rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test/#");
        teq(rc, 0);

        rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "#");
        teq(rc, 0);
    }

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testTopicComplexWildcards(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    mq = mqttAlloc("complex-wildcard-client", NULL);
    ttrue(mq);

    sock = createConnectedSocket();
    ttrue(sock);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "device/+/sensor/+/data");
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "home/+/temperature/#");
    teq(rc, 0);

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testTopicEmptyLevels(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    mq = mqttAlloc("empty-levels-client", NULL);
    ttrue(mq);

    sock = createConnectedSocket();
    ttrue(sock);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test//topic");
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "/test/topic");
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test/topic/");
    teq(rc, 0);

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testTopicDollarTopics(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    mq = mqttAlloc("dollar-topics-client", NULL);
    ttrue(mq);

    sock = createConnectedSocket();
    ttrue(sock);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "$SYS/broker/version");
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "$share/group/topic");
    teq(rc, 0);

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testTopicCaseSensitive(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    mq = mqttAlloc("case-sensitive-client", NULL);
    ttrue(mq);

    sock = createConnectedSocket();
    ttrue(sock);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "Test/Topic");
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test/topic");
    teq(rc, 0);

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testTopicUtf8(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    mq = mqttAlloc("utf8-client", NULL);
    ttrue(mq);

    sock = createConnectedSocket();
    ttrue(sock);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "测试/主题");
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "тест/тема");
    teq(rc, 0);

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testTopicSpecialCharacters(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    mq = mqttAlloc("special-chars-client", NULL);
    ttrue(mq);

    sock = createConnectedSocket();
    ttrue(sock);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test-topic_1");
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test.topic.2");
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test@topic");
    teq(rc, 0);

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testTopicMaxLength(void)
{
    RSocket *sock;
    Mqtt    *mq;
    char    maxTopic[MQTT_MAX_TOPIC_SIZE];
    char    tooLongTopic[MQTT_MAX_TOPIC_SIZE + 10];
    int     rc;

    mq = mqttAlloc("max-length-client", NULL);
    ttrue(mq);

    sock = createConnectedSocket();
    ttrue(sock);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    memset(maxTopic, 'a', MQTT_MAX_TOPIC_SIZE - 1);
    maxTopic[MQTT_MAX_TOPIC_SIZE - 1] = '\0';

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, maxTopic);
    teq(rc, 0);

    memset(tooLongTopic, 'b', sizeof(tooLongTopic) - 1);
    tooLongTopic[sizeof(tooLongTopic) - 1] = '\0';

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, tooLongTopic);
    teq(rc, R_ERR_BAD_ARGS);

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testTopicValidation(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    mq = mqttAlloc("validation-client", NULL);
    ttrue(mq);

    sock = createConnectedSocket();
    ttrue(sock);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "topic/with/+/invalid#+");
    teq(rc, R_ERR_BAD_ARGS);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "topic/with/#/invalid");
    teq(rc, R_ERR_BAD_ARGS);

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testTopicHierarchy(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    mq = mqttAlloc("hierarchy-client", NULL);
    ttrue(mq);

    sock = createConnectedSocket();
    ttrue(sock);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "a");
    teq(rc, 0);
    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "a/b");
    teq(rc, 0);
    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "a/b/c");
    teq(rc, 0);
    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "a/b/c/d");
    teq(rc, 0);

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testTopicMasterSubscription(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    mq = mqttAlloc("master-sub-client", NULL);
    ttrue(mq);

    sock = createConnectedSocket();
    ttrue(sock);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    mq = mqttAlloc("master-sub-client2", NULL);
    ttrue(mq);

    if (mqttConnect(mq, sock, 0, MQTT_WAIT_ACK) == 0) {
        rc = mqttSubscribeMaster(mq, 1, MQTT_WAIT_NONE, "device/+");
        teq(rc, 0);

        rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "device/123/status");
        teq(rc, 0);
        rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "device/456/data");
        teq(rc, 0);
    }

    mqttFree(mq);
    rFreeSocket(sock);
}

static void fiberMain(void *data)
{
    testTopicExactMatch();
    testTopicSingleLevelWildcard();
    testTopicMultiLevelWildcard();
    testTopicComplexWildcards();
    testTopicEmptyLevels();
    testTopicDollarTopics();
    testTopicCaseSensitive();
    testTopicUtf8();
    testTopicSpecialCharacters();
    testTopicMaxLength();
    testTopicValidation();
    testTopicHierarchy();
    testTopicMasterSubscription();
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