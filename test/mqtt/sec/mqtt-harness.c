/*
    mqtt-harness.c - MQTT parser fuzz tests. See README.md for usage. Include mqttLib.c to access static parsers. No socket I/O occurs.

    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
 */

/********************************** Includes **********************************/

#include "harness.h"

#ifndef SEC_LIBFUZZER
#include "fuzzcore/fuzz.c"
#endif

/*
    Include the source to access static parsers.
 */
#define ME_COM_MQTT 1
#include "mqttLib.c"

/*********************************** Locals ***********************************/

/*
    Reuse one client, resetting parser state and queued messages between inputs.
 */
static Mqtt *gMq;

static void noopEvent(Mqtt *mq, int event)
{
    (void) mq;
    (void) event;
}

static void initRuntime(void)
{
    static int initialised = 0;

    if (!initialised) {
        initialised = 1;
        rInit(0, 0);
        gMq = mqttAlloc("fuzz-client", noopEvent);
        /*
            queueMsg needs a socket; its NULL wait makes rSetWaitMask a no-op.
         */
        gMq->sock = rAllocSocket();
    }
}

/*
    Free queued acknowledgements because the harness never sends them.
 */
static void drainQueue(Mqtt *mq)
{
    MqttMsg *msg, *next;

    for (msg = mq->head.next; msg != &mq->head; msg = next) {
        next = msg->next;
        msg->hold = 0;
        msg->wait = 0;
        freeMsg(msg);
    }
    mq->head.next = mq->head.prev = &mq->head;
}

static void resetMqtt(Mqtt *mq)
{
    drainQueue(mq);
    rFlushBuf(mq->buf);
    mq->error = 0;
    rFree(mq->errorMsg);
    mq->errorMsg = 0;
    mq->connected = 0;
    mq->fiberCount = 0;
}

/*
    Bound diagnostic output because fuzz input is not NUL-terminated.
 */
static int clampLen(size_t len)
{
    return (int) (len < 80 ? len : 80);
}

/********************************** Oracles ***********************************/

/*
    Check buffered packet consumption and topic/payload bounds.
 */
static bool packetOracle(cchar *input, size_t len)
{
    Mqtt     *mq;
    MqttRecv rp;
    ssize    remaining, consumed;
    int      iterations;
    bool     ok;

    initRuntime();
    mq = gMq;
    resetMqtt(mq);
    ok = true;

    rPutBlockToBuf(mq->buf, input, len);
    rAddNullToBuf(mq->buf);

    iterations = 0;
    while (!mq->error && iterations++ < 1000) {
        remaining = rGetBufLength(mq->buf);
        if (remaining <= 0) {
            break;
        }
        consumed = unpackResp(mq, &rp);
        if (consumed < 0) {
            break;
        }
        if (consumed == 0) {
            break;
        }
        if (consumed > remaining) {
            fprintf(stderr,
                    "sec: mqtt packet: unpackResp consumed %zd bytes but only %zd were buffered "
                    "(input \"%.*s\")\n", consumed, remaining, clampLen(len), input);
            ok = false;
            break;
        }
        if (rp.hdr.type == MQTT_PACKET_PUBLISH) {
            cuchar *packetEnd = rp.start + rp.hdr.length;
            if (rp.topicSize > 0 &&
                ((cuchar*) rp.topic < rp.start || (cuchar*) rp.topic + rp.topicSize > packetEnd)) {
                fprintf(stderr,
                        "sec: mqtt packet: PUBLISH topic [%p,+%zu) escapes the packet body "
                        "(input \"%.*s\")\n", (void*) rp.topic, rp.topicSize, clampLen(len), input);
                ok = false;
            }
            if (rp.dataSize > 0 &&
                ((cuchar*) rp.data < rp.start || (cuchar*) rp.data + rp.dataSize > packetEnd)) {
                fprintf(stderr,
                        "sec: mqtt packet: PUBLISH payload [%p,+%zd) escapes the packet body "
                        "(input \"%.*s\")\n", (void*) rp.data, rp.dataSize, clampLen(len), input);
                ok = false;
            }
        }
        processRecvMsg(mq, &rp);
        rAdjustBufStart(mq->buf, (ssize) consumed);
    }
    return ok;
}

/*
    Compare remaining-length decoding with an independent MQTT reference decoder.
 */
static ssize referenceVarint(cchar *input, size_t len, uint32 *value, bool *needsFifthByte)
{
    size_t pos;
    int    shift;
    uchar  c;
    uint32 v;

    *value = 0;
    *needsFifthByte = false;
    v = 0;
    shift = 0;
    for (pos = 1; pos < len && pos <= 4; pos++) {
        c = (uchar) input[pos];
        v += (uint32) (c & 0x7F) << shift;
        shift += 7;
        if (!(c & 0x80)) {
            *value = v;
            return (ssize) (pos + 1);
        }
    }
    if (pos == 5 && pos < len && ((uchar) input[pos - 1] & 0x80)) {
        *needsFifthByte = true;
    }
    return 0;
}

static bool varintOracle(cchar *input, size_t len)
{
    Mqtt     *mq;
    MqttRecv rp;
    ssize    rc, refConsumed;
    uint32   refValue;
    uchar    type, flags;
    bool     needsFifthByte, typeOk, ok;

    if (len == 0) {
        return true;
    }
    initRuntime();
    mq = gMq;
    resetMqtt(mq);
    ok = true;

    rPutBlockToBuf(mq->buf, input, len);
    rAddNullToBuf(mq->buf);

    memset(&rp, 0, sizeof(rp));
    rc = unpackRespHdr(mq, &rp);

    refConsumed = referenceVarint(input, len, &refValue, &needsFifthByte);

    if (needsFifthByte && rc > 0) {
        fprintf(stderr,
                "sec: mqtt varint: a varint needing a 5th continuation byte was ACCEPTED (rc %zd, "
                "length %u) (input \"%.*s\")\n", rc, rp.hdr.length, clampLen(len), input);
        ok = false;
    }
    /*
        Exclude type/flag rejections from checks for valid varint acceptance.
     */
    type = (uchar) input[0] >> 4;
    flags = (uchar) input[0] & 0x0F;
    typeOk = MqttHdr_rules.typeIsValid[type] &&
             !MQTT_BITFIELD_RULE_VIOLOATION(flags, MqttHdr_rules.requiredFlags[type], MqttHdr_rules.requiredFlagsMask[type]);

    if (typeOk && refConsumed > 0 && (size_t) refConsumed + refValue <= len) {
        if (rc <= 0) {
            fprintf(stderr,
                    "sec: mqtt varint: reference decoder accepts a complete varint (header %zd "
                    "bytes, length %u) but unpackRespHdr rejected it, rc %zd (input \"%.*s\")\n",
                    refConsumed, refValue, rc, clampLen(len), input);
            ok = false;
        } else if (rp.hdr.length != refValue) {
            fprintf(stderr,
                    "sec: mqtt varint: decoded length %u disagrees with the reference decoder's %u "
                    "(input \"%.*s\")\n", rp.hdr.length, refValue, clampLen(len), input);
            ok = false;
        }
    }
    if (rc > 0 && ((size_t) rc + rp.hdr.length) > len) {
        fprintf(stderr,
                "sec: mqtt varint: unpackRespHdr accepted a header claiming %zd + %u bytes but only "
                "%zu were supplied (input \"%.*s\")\n", rc, rp.hdr.length, len, clampLen(len), input);
        ok = false;
    }
    return ok;
}

#if defined(MQTT_ORACLE_VARINT)
    #define ORACLE      varintOracle
    #define ORACLE_NAME "varint"
#else
    #define ORACLE      packetOracle
    #define ORACLE_NAME "packet"
#endif

/****************************** libFuzzer entry *******************************/

#ifdef SEC_LIBFUZZER
SEC_LIBFUZZER_ENTRY(ORACLE)

#else

/*********************************** main *************************************/

/*
    Use sizeof on literals to count embedded NUL bytes.
 */
typedef struct {
    cchar *data;
    size_t len;
} Seed;

#define SEED(x) { x, sizeof(x) - 1 }

#if defined(MQTT_ORACLE_VARINT)
    static const Seed seeds[] = {
        SEED("\x20\x02"),                                  //  CONNACK, length 2 (header only, needs 2 more)
        SEED("\x20\x00"),                                  //  CONNACK, length 0
        SEED("\xd0\x00"),                                  //  PINGRESP, length 0
        SEED("\x30\x7f"),                                  //  PUBLISH, length 127 (1-byte varint boundary)
        SEED("\x30\x80\x01"),                              //  PUBLISH, length 128 (2-byte varint boundary)
        SEED("\x30\xff\x7f"),                              //  PUBLISH, length 16383 (2-byte max)
        SEED("\x30\x80\x80\x01"),                          //  PUBLISH, length 16384 (3-byte boundary)
        SEED("\x30\xff\xff\xff\x7f"),                      //  PUBLISH, length near 4-byte max
        SEED("\x30\xff\xff\xff\xff\x7f"),                  //  5 continuation bytes - must be rejected
        SEED("\x30\x80\x80\x80\x80\x01"),                  //  5-byte varint, all continuation bits set
        SEED("\x30\x80"),                                  //  truncated - only 1 varint byte, still continuing
        SEED("\x00\x00"),                                  //  reserved type 0 - invalid type, valid varint
        SEED("\xf0\x00"),                                  //  reserved type 15 - invalid type, valid varint
        SEED(""),
        SEED("\x20"),
    };
#else
    static const Seed seeds[] = {
        SEED("\x20\x02\x00\x00"),                                     //  CONNACK accepted, no session
        SEED("\x20\x02\x01\x00"),                                     //  CONNACK accepted, session present
        SEED("\x20\x02\x00\x05"),                                     //  CONNACK refused, not authorized
        SEED("\x30\x09\x00\x03""abc""hello"),                         //  PUBLISH qos0, topic "abc", data "hello"
        SEED("\x32\x0b\x00\x03""abc""\x00\x01""hello"),               //  PUBLISH qos1, packet id 1
        SEED("\x34\x0b\x00\x03""abc""\x00\x02""hello"),               //  PUBLISH qos2, packet id 2
        SEED("\x40\x02\x00\x01"),                                     //  PUBACK id 1
        SEED("\x50\x02\x00\x02"),                                     //  PUBREC id 2
        SEED("\x62\x02\x00\x02"),                                     //  PUBREL id 2 (flags 0x02 required)
        SEED("\x70\x02\x00\x02"),                                     //  PUBCOMP id 2
        SEED("\x90\x03\x00\x01\x01"),                                 //  SUBACK id 1, code max-qos-1
        SEED("\x90\x03\x00\x01\x80"),                                 //  SUBACK id 1, code failure
        SEED("\xb0\x02\x00\x01"),                                     //  UNSUBACK id 1
        SEED("\xd0\x00"),                                             //  PINGRESP
        SEED("\xe0\x00"),                                             //  DISCONNECT
        SEED("\x20\x02\x00\x00" "\xd0\x00"),                          //  two packets back to back
        SEED("\x30\xff\x7f" "abc"),                                   //  PUBLISH claims 16383 bytes, only 3 given
        SEED("\x30\x02\xff\xff"),                                     //  PUBLISH, topicSize > hdr.length - 2
        SEED("\x00\x00"),                                             //  reserved type 0
        SEED("\x64\x02\x00\x02"),                                     //  PUB_REL without required 0x02 flag
        SEED(""),
    };
#endif

int main(int argc, char **argv)
{
    FuzzConfig config;
    FuzzRunner *runner;
    cchar      *replay;
    int        crashes, leaked;
    size_t     s;

    (void) argc;
    (void) argv;

    initRuntime();

    memset(&config, 0, sizeof(config));
    config.stop = false;
    config.verbose = false;
    // fuzzInit creates only one directory level.
    config.crashDir = "crashes";

    runner = fuzzInit(&config);
    fuzzSetOracle(runner, ORACLE);

    for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
        fuzzAddCorpus(runner, seeds[s].data, seeds[s].len);
    }
    // fuzzLoadDir does not recurse.
    fuzzLoadDir(runner, "corpus/" ORACLE_NAME);

    replay = getenv("FUZZ_REPLAY");
    if (replay) {
        printf("Replay mode: %s\n", replay);
        fuzzClearCorpus(runner);
        fuzzLoadArtifact(runner, replay);
    }
    printf("Oracle: %s\n", ORACLE_NAME);

    crashes = fuzzRun(runner);
    fuzzReport(runner);
    fuzzFree(runner);

    leaked = fuzzLeakCheck();

    return (crashes > 0 || leaked > 0) ? 1 : 0;
}

#endif /* SEC_LIBFUZZER */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
 */
