/*
    mqtt-test.h - Shared helpers for the MQTT unit tests

    The tests connect to a broker on localhost. The port comes from MQTT_PORT and defaults to 1883,
    matching test/setup.sh which starts the broker for the suite.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_MQTT_TEST
#define _h_MQTT_TEST 1

/********************************** Includes **********************************/

#include    "testme.h"
#include    "mqtt.h"

/*********************************** Defines **********************************/

/*
    Assert the result of a call. The TestMe comparison macros evaluate their arguments more than
    once, so a call with side effects must be made exactly once and its result compared.
 */
#define tcall(expr, expected) \
        if (1) { \
            int _rc = (expr); \
            teq(_rc, expected); \
        } else

/*
    Poll until the condition holds or the timeout in milliseconds expires, yielding between polls
    so the event loop, the client and the broker make progress.
 */
#define WAIT_UNTIL(cond, timeout) \
        if (1) { \
            Ticks _deadline = rGetTicks() + (timeout); \
            while (!(cond) && rGetTicks() < _deadline) { \
                rSleep(5); \
            } \
        } else

/************************************ Code ************************************/

static inline int mqttTestPort(void)
{
    cchar *port;

    port = getenv("MQTT_PORT");
    return port ? atoi(port) : 1883;
}

/*
    Connect a socket to the test broker. Returns NULL if the broker is not running.
 */
static inline RSocket *connectBroker(void)
{
    RSocket *sock;

    sock = rAllocSocket();
    if (rConnectSocket(sock, "localhost", mqttTestPort(), 0) < 0) {
        rFreeSocket(sock);
        return NULL;
    }
    return sock;
}

#endif /* _h_MQTT_TEST */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
