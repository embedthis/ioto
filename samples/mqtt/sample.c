/*
    aws-s3/sample.c - Send a file to S3
 */

/********************************** Includes **********************************/

#include "ioto.h"

/*********************************** Forwards *********************************/

static void incoming(MqttRecv *rp);

/************************************* Code ***********************************/

PUBLIC int ioStart(void)
{
    /*
        Subscribe for incoming notifications.
        This call will block and wait for acknowledgement.
        Note: call blocks, but ioto is not blocked.
     */
    mqttSubscribe(ioto->mqtt, (MqttCallback) incoming, 2, MQTT_WAIT_ACK, "/myDevice/change");

    /*
        Publish an "initialized" message with quality of service (1) which means send at most once.
        Don't wait for sending or acknowledgement.
     */
    mqttPublish(ioto->mqtt, "Initialized", 0, 1, MQTT_WAIT_NONE, "/myDevice/init");

    /*
        Unsubscribe from receiving further messages
     */
    mqttUnsubscribe(ioto->mqtt, "/myDevice/unsub", MQTT_WAIT_NONE);

    //  Exit sample
    rStop();
    return 0;
}

PUBLIC void ioStop() {}

static void incoming(MqttRecv *rp)
{
    char  *msg;

    msg = snclone(rp->data, rp->dataSize);
    rTrace("shadow", "Received from topic %s: %s", rp->topic, msg);
    rFree(msg);
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
