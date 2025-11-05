/*
    packet.c.tst - MQTT packet parsing and validation tests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "mqtt.h"

/************************************ Code ************************************/

static void testMqttPacketTypes(void)
{
    teq(MQTT_PACKET_CONNECT, 1);
    teq(MQTT_PACKET_CONN_ACK, 2);
    teq(MQTT_PACKET_PUBLISH, 3);
    teq(MQTT_PACKET_PUB_ACK, 4);
    teq(MQTT_PACKET_PUB_REC, 5);
    teq(MQTT_PACKET_PUB_REL, 6);
    teq(MQTT_PACKET_PUB_COMP, 7);
    teq(MQTT_PACKET_SUB, 8);
    teq(MQTT_PACKET_SUB_ACK, 9);
    teq(MQTT_PACKET_UNSUB, 10);
    teq(MQTT_PACKET_UNSUB_ACK, 11);
    teq(MQTT_PACKET_PING, 12);
    teq(MQTT_PACKET_PING_ACK, 13);
    teq(MQTT_PACKET_DISCONNECT, 14);
}

static void testMqttQosFlags(void)
{
    teq(MQTT_QOS_FLAGS_0, 0);
    teq(MQTT_QOS_FLAGS_1, 2);
    teq(MQTT_QOS_FLAGS_2, 4);
    teq((MQTT_QOS_FLAGS_MASK & MQTT_QOS_FLAGS_1), MQTT_QOS_FLAGS_1);
    teq((MQTT_QOS_FLAGS_MASK & MQTT_QOS_FLAGS_2), MQTT_QOS_FLAGS_2);
}

static void testMqttPubFlags(void)
{
    teq(MQTT_RETAIN, 1);
    teq(MQTT_DUP, 8);
    teq((MQTT_DUP | MQTT_RETAIN), 9);
}

static void testMqttConnectFlags(void)
{
    teq(MQTT_CONNECT_CLEAN_SESSION, 2);
    teq(MQTT_CONNECT_WILL_FLAG, 4);
    teq(MQTT_CONNECT_WILL_RETAIN, 32);
    teq(MQTT_CONNECT_PASSWORD, 64);
    teq(MQTT_CONNECT_USER_NAME, 128);
}

static void testMqttConnackCodes(void)
{
    teq(MQTT_CONNACK_ACCEPTED, 0);
    teq(MQTT_CONNACK_REFUSED_PROTOCOL_VERSION, 1);
    teq(MQTT_CONNACK_REFUSED_IDENTIFIER_REJECTED, 2);
    teq(MQTT_CONNACK_REFUSED_SERVER_UNAVAILABLE, 3);
    teq(MQTT_CONNACK_REFUSED_BAD_USER_NAME_OR_PASSWORD, 4);
    teq(MQTT_CONNACK_REFUSED_NOT_AUTHORIZED, 5);
}

static void testMqttSubackCodes(void)
{
    teq(MQTT_SUBACK_SUCCESS_MAX_QOS_0, 0);
    teq(MQTT_SUBACK_SUCCESS_MAX_QOS_1, 1);
    teq(MQTT_SUBACK_SUCCESS_MAX_QOS_2, 2);
    teq(MQTT_SUBACK_FAILURE, 128);
}

static void testMqttWaitFlags(void)
{
    teq(MQTT_WAIT_NONE, 0);
    teq(MQTT_WAIT_SENT, 1);
    teq(MQTT_WAIT_ACK, 2);
    teq(MQTT_WAIT_FAST, 4);
    teq((MQTT_WAIT_SENT | MQTT_WAIT_ACK), 3);
}

static void testMqttProtocolLevel(void)
{
    teq(MQTT_PROTOCOL_LEVEL, 0x04);
}

static void testMqttConstants(void)
{
    teq(MQTT_INLINE_BUF_SIZE, 128);
    teq(MQTT_BUF_SIZE, 4096);
    teq(MQTT_MAX_TOPIC_SIZE, 128);
    teq(MQTT_MAX_CLIENT_ID_SIZE, 23);
    teq(MQTT_MAX_USERNAME_SIZE, 128);
    teq(MQTT_MAX_PASSWORD_SIZE, 128);
    teq(MQTT_MAX_MESSAGE_SIZE, 256 * 1024 * 1024);
}

static void testMqttMsgStates(void)
{
    teq(MQTT_UNSENT, 1);
    teq(MQTT_AWAITING_ACK, 2);
    teq(MQTT_COMPLETE, 3);
}

static void testMqttEventTypes(void)
{
    teq(MQTT_EVENT_ATTACH, 1);
    teq(MQTT_EVENT_CONNECTED, 2);
    teq(MQTT_EVENT_DISCONNECT, 3);
    teq(MQTT_EVENT_TIMEOUT, 4);
}

static void testMqttHdrStruct(void)
{
    MqttHdr hdr;

    hdr.type = MQTT_PACKET_CONNECT;
    hdr.flags = 0;
    hdr.length = 10;

    teq(hdr.type, MQTT_PACKET_CONNECT);
    teq(hdr.flags, 0);
    teq(hdr.length, 10);
}

static void testMqttRecvStruct(void)
{
    MqttRecv recv;

    memset(&recv, 0, sizeof(recv));
    recv.hdr.type = MQTT_PACKET_PUBLISH;
    recv.id = 123;
    recv.qos = 1;
    recv.retain = 1;
    recv.dup = 0;

    teq(recv.hdr.type, MQTT_PACKET_PUBLISH);
    teq(recv.id, 123);
    teq(recv.qos, 1);
    teq(recv.retain, 1);
    teq(recv.dup, 0);
}

static void testMqttTopicStruct(void)
{
    MqttTopic topic;

    memset(&topic, 0, sizeof(topic));
    topic.topic = "test/topic";
    topic.callback = NULL;
    topic.wait = MQTT_WAIT_NONE;

    tmatch(topic.topic, "test/topic");
    tfalse(topic.callback);
    teq(topic.wait, MQTT_WAIT_NONE);
}

static void fiberMain(void *data)
{
    testMqttPacketTypes();
    testMqttQosFlags();
    testMqttPubFlags();
    testMqttConnectFlags();
    testMqttConnackCodes();
    testMqttSubackCodes();
    testMqttWaitFlags();
    testMqttProtocolLevel();
    testMqttConstants();
    testMqttMsgStates();
    testMqttEventTypes();
    testMqttHdrStruct();
    testMqttRecvStruct();
    testMqttTopicStruct();
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