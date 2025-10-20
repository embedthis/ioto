/*
    shadow.c - Shadow state management

    shadow.json contains control state and is saved to AWS IoT Shadows.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

#if SERVICES_SHADOW
/************************************ Forwards *********************************/

static void lazySave(Json *json, int delay);
static Json *loadShadow(void);
static void onShadowReceive(MqttRecv *rp);
static int publishShadow(Json *json);
static int saveShadow(Json *json);
static void subscribeShadow(void);

/************************************* Code ***********************************/

PUBLIC int ioInitShadow(void)
{
    if ((ioto->shadow = loadShadow()) == 0) {
        return R_ERR_CANT_READ;
    }
    ioto->shadowName = jsonGetClone(ioto->config, 0, "cloud.shadow", "default");
    ioto->shadowTopic = sfmt("$aws/things/%s/shadow/name/%s", ioto->id, ioto->shadowName);
    ioOnConnect((RWatchProc) subscribeShadow, NULL, 1);
    return 0;
}

PUBLIC void ioTermShadow(void)
{
    Json *json;

    json = ioto->shadow;

    if (json) {
        if (ioto->shadowEvent) {
            rStopEvent(ioto->shadowEvent);
            saveShadow(json);
        }
        jsonFree(json);
    }
    rFree(ioto->shadowName);
    rFree(ioto->shadowTopic);
    ioto->shadowTopic = 0;
    ioto->shadowName = 0;
    ioto->shadow = 0;
}

static void subscribeShadow(void)
{
    if (!smatch(ioto->cloudType, "dedicated")) {
        rError("shadow", "Cloud type \"%s\" does not support AWS IoT shadows", ioto->cloudType);
    } else {
        //  OPT - could common up to just %s/
        mqttSubscribe(ioto->mqtt, onShadowReceive, 1, MQTT_WAIT_NONE, "%s/get/accepted", ioto->shadowTopic);
        mqttSubscribe(ioto->mqtt, onShadowReceive, 1, MQTT_WAIT_NONE, "%s/get/rejected", ioto->shadowTopic);
        mqttSubscribe(ioto->mqtt, onShadowReceive, 1, MQTT_WAIT_NONE, "%s/update/accepted", ioto->shadowTopic);
        mqttSubscribe(ioto->mqtt, onShadowReceive, 1, MQTT_WAIT_NONE, "%s/update/rejected", ioto->shadowTopic);
        mqttPublish(ioto->mqtt, "", 0, 1, MQTT_WAIT_ACK, "%s/get", ioto->shadowTopic);
        rInfo("shadow", "Connected to: AWS IOT core");
    }
}

PUBLIC void ioSaveShadow()
{
    lazySave(ioto->shadow, 0);
}

static void lazySave(Json *json, int delay)
{
    if (!ioto->shadowEvent) {
        ioto->shadowEvent = rStartEvent((REventProc) saveShadow, json, delay);
    }
}

static int saveShadow(Json *json)
{
    char *path;

    if (ioto->nosave) return 0;
    ioto->shadowEvent = 0;

    path = rGetFilePath(IO_SHADOW_FILE);
    if (jsonSave(json, 0, 0, path, ioGetFileMode(), JSON_JSON5 | JSON_MULTILINE) < 0) {
        rError("shadow", "Cannot save to %s, errno %d", json->path, errno);
        rFree(path);
        return R_ERR_CANT_WRITE;
    }
    rFree(path);
    return publishShadow(json);
}

PUBLIC char *ioGetShadow(cchar *key, cchar *defaultValue)
{
    return jsonGetClone(ioto->shadow, 0, key, defaultValue);
}

PUBLIC void ioSetShadow(cchar *key, cchar *value, bool save)
{
    jsonSet(ioto->shadow, 0, key, value, 0);
    if (save) {
        lazySave(ioto->shadow, IO_SAVE_DELAY);
    }
}

PUBLIC int ioGetFileMode(void)
{
    return smatch(ioto->profile, "dev") ?  0660 : 0600;
}

static Json *loadShadow()
{
    Json *json;
    char *errorMsg, *path;

    path = rGetFilePath(IO_SHADOW_FILE);
    if (rAccessFile(path, R_OK) == 0) {
        if ((json = jsonParseFile(path, &errorMsg, 0)) == 0) {
            rError("shadow", "%s", errorMsg);
            rFree(errorMsg);
            rFree(path);
            return 0;
        }
    } else {
        json = jsonAlloc(0);
    }
#if KEEP
    rPrintf("%s\n%s\n", path, jsonString(json, 0));
#endif
    rFree(path);
    return json;
}

static void onShadowReceive(MqttRecv *rp)
{
    Json  *json;
    int   nid;
    char  *data, *msg, *path;
    cchar *topic;

    topic = rp->topic;

    msg = snclone(rp->data, rp->dataSize);
    rTrace("shadow", "Received shadow: %s", msg);

    if (sends(topic, "/get/accepted")) {

        //  Extract state.reported
        json = jsonParse(msg, 0);
        nid = jsonGetId(json, 0, "state.reported");
        data = jsonToString(json, nid, 0, JSON_PRETTY);

        jsonFree(ioto->shadow);
        ioto->shadow = jsonParse(data, 0);

        //  Just to make local debugging easier.
        path = rGetFilePath(IO_SHADOW_FILE);
        rWriteFile(path, data, slen(data), ioGetFileMode());
        rFree(path);
        rFree(data);
        jsonFree(json);

    } else if (sends(topic, "/get/rejected")) {
        rError("shadow", "Get shadow rejected: %s", msg);

    } else if (sends(topic, "/update/accepted")) {
        ;

    } else if (sends(topic, "/update/rejected")) {
        rError("shadow", "Update shadow rejected: %s", msg);
    }
    rFree(msg);
}

/*
    Publish to AWS IOT core shadows
 */
static int publishShadow(Json *json)
{
    char *buf, *data;

    if (ioto->mqtt == 0) return R_ERR_BAD_STATE;

    if ((data = jsonToString(json, 0, 0, JSON_QUOTES)) == 0) {
        return R_ERR_BAD_STATE;
    }
    if (slen(data) > IO_MESSAGE_SIZE) {
        rError("shadow", "State is too big to save to AWS IOT");
        rFree(data);
        return R_ERR_WONT_FIT;
    }
    buf = sfmt("{\"state\":{\"reported\":%s}}", data);
    mqttPublish(ioto->mqtt, buf, slen(buf), 0, MQTT_WAIT_NONE, "%s/update", ioto->shadowTopic, ioto->shadowName);
    rFree(buf);
    rFree(data);
    return 0;
}

#else
void dummyShadow(void)
{
}
#endif /* SERVICES_CLOUD */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
