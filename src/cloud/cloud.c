/*
    cloud.c - Cloud services. Includes cloudwatch logs, log capture, shadow state and database sync.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

#if SERVICES_CLOUD
/************************************* Code ***********************************/

PUBLIC int ioInitCloud(void)
{
#if SERVICES_PROVISION
    if (ioto->provisionService) {
        if (ioInitProvisioner() < 0) {
            return R_ERR_CANT_INITIALIZE;
        }
    }
#endif
#if SERVICES_MQTT
    if (ioto->mqttService && (ioInitMqtt() < 0)) {
        return R_ERR_CANT_INITIALIZE;
    }
#endif
#if SERVICES_SHADOW
    if (ioto->shadowService && (ioInitShadow() < 0)) {
        return R_ERR_CANT_INITIALIZE;
    }
#endif
#if SERVICES_LOGS
    if (ioto->logService && (ioInitLogs() < 0)) {
        return R_ERR_CANT_INITIALIZE;
    }
#endif
    return 0;
}

PUBLIC void ioTermCloud(void)
{
#if SERVICES_LOGS
    if (ioto->logService) {
        ioTermLogs();
    }
#endif
#if SERVICES_SYNC
    if (ioto->syncService) {
        ioTermSync();
    }
#endif
#if SERVICES_SHADOW
    if (ioto->shadowService) {
        ioTermShadow();
    }
#endif
#if SERVICES_MQTT
    ioTermMqtt();
#endif
    ioto->instance = 0;
}
#if SERVICES_CLOUD
/*
    Invoke an Ioto REST API on the device cloud.

    url POST https://xxxxxxxxxx.execute-api.ap-southeast-1.amazonaws.com/tok/action/invoke \
        'Authorization: xxxxxxxxxxxxxxxxxxxxxxxxxx' \
        'Content-Type: application/json' \
        '{name:"AutomationName",context:{propertyName:42}}'
 */
PUBLIC Json *ioAPI(cchar *url, cchar *data)
{
    Json *response;
    char *api;

    // SECURITY Acceptable: - the ioto-api is provided by the cloud service and is secure
    api = sfmt("%s/%s", ioto->api, url);
    response = urlPostJson(api, data, 0,
                           "Authorization: bearer %s\r\nContent-Type: application/json\r\n", ioto->apiToken);
    rFree(api);
    if (!response) {
        rError("ai", "Cannot invoke automation");
        return 0;
    }
    return response;
}

/**
    Invoke an Ioto automation on the device cloud.
    data is a string representation of a json object
    Context {...properties} in strict JSON. Use JSON or JFMT to create the context.
 */
PUBLIC int ioAutomation(cchar *name, cchar *context)
{
    Json  *response, *jsonContext, *data;
    cchar *args;
    int   rc;

    data = jsonAlloc();
    jsonSet(data, 0, "name", name, JSON_STRING);

    if ((jsonContext = jsonParse(context, 0)) == 0) {
        rError("ai", "Invalid JSON context provided to ioAutomation");
        jsonFree(data);
        return R_ERR_BAD_ARGS;
    }
    jsonBlend(data, 0, "context", jsonContext, 0, 0, 0);
    jsonFree(jsonContext);

    args = jsonString(data, 0);
    response = ioAPI("tok/action/invoke", args);
    jsonFree(data);

    rc = R_ERR_CANT_COMPLETE;
    if (response) {
        if (jsonGet(response, 0, "error", 0) == 0) {
            rc = 0;
        }
        jsonFree(response);
    }
    if (rc != 0) {
        rError("ai", "Cannot invoke automation");
    }
    return rc;
}

/*
    Upload a file to the device cloud.
 */
PUBLIC int ioUpload(cchar *path, uchar *buf, size_t len)
{
    Url   *up;
    cchar *response;
    char  *api, *data, *url;
    int   rc;

    up = urlAlloc(0);
    api = sfmt("%s/tok/file/getSignedUrl", ioto->api);
    data = sfmt(SDEF({
        "id" : "%s",
        "command" : "put",
        "filename" : "%s",
        "mimeType" : "image/jpeg",
        "size" : "%ld"
    }), ioto->id, path, len);

    rc = R_ERR_CANT_COMPLETE;
    if (urlFetch(up, "POST", api, data, 0, "Authorization: bearer %s\r\nContent-Type: application/json\r\n",
                 ioto->apiToken) != URL_CODE_OK) {
        rError("nature", "Error: %s", urlGetResponse(up));

    } else if ((response = urlGetResponse(up)) == NULL) {
        rError("nature", "Empty signed URL response");

    } else {
        url = sclone(response);
        if (urlFetch(up, "PUT", strim(url, "\"", R_TRIM_BOTH), (char*) buf, len,
                     "Content-Type: image/jpeg\r\n") != URL_CODE_OK) {
            rError("nature", "Cannot upload to signed URL");
        } else {
            rc = 0;
        }
        rFree(url);
    }
    rFree(data);
    rFree(api);
    urlFree(up);
    return rc;
}

/*
    Exponential backoff. This can be awakened via ioResumeBackoff()
 */
PUBLIC Ticks ioBackoff(Ticks delay, REvent *event)
{
    assert(event);

    if (delay == 0) {
        delay = TPS * 10;
    }
    delay += TPS / 4;
    if (delay > 3660 * TPS) {
        delay = 3660 * TPS;
    }
    if (ioto->blockedUntil > rGetTime()) {
        delay = max(delay, ioto->blockedUntil - rGetTime());
    }
    if (*event) {
        rStopEvent(*event);
    }
    *event = rStartEvent(NULL, 0, delay);
    rYieldFiber(0);
    *event = 0;
    return delay;
}

/*
    Resume a backoff event
 */
PUBLIC void ioResumeBackoff(REvent *event)
{
    if (event && *event) {
        rRunEvent(*event);
    }
}
#endif /* SERVICES_CLOUD */

#else
void dummyCloud(void)
{
}
#endif /* SERVICES_CLOUD */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
