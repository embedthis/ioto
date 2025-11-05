/*
    basic.c.tst - Basic MQTT Unit tests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "mqtt.h"

/************************************ Code ************************************/

static void eventProc(Mqtt *mq, int event)
{
}

static void testBasicAlloc(void)
{
    Mqtt *mq;

    mq = mqttAlloc(NULL, NULL);
    tfalse(mq);

    mq = mqttAlloc("test", eventProc);
    ttrue(mq);
    mqttFree(mq);
}

static void fiberMain(void *data)
{
    testBasicAlloc();
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
