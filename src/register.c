/*
    register.c - One-time device registration during manufacturer or first connect.

    NOTE: The Ioto license requires that if this code is removed or disabled, you must manually enter and
    maintain device volumes using Embedthis Builder (https://admin.embedthis.com) or you must have a current
    contract agreement with Embedthis to use an alternate method.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

#if SERVICES_REGISTER
/*********************************** Forwards *********************************/

static int parseRegisterResponse(Json *json);

/************************************* Code ***********************************/
/*
    Send a registration request to the builder
 */
PUBLIC int ioRegister(void)
{
    Json       *params, *response;
    char       *data, *path, url[160];
    bool       test;
    int        rc;
    static int once = 0;

#if SERVICES_CLOUD
    if (ioto->api && ioto->apiToken) {
        rInfo("ioto", "Device registered and claimed by %s cloud \"%s\" in %s",
              jsonGet(ioto->config, 0, "provision.cloudType", 0),
              jsonGet(ioto->config, 0, "provision.cloudName", 0),
              jsonGet(ioto->config, 0, "provision.cloudRegion", 0)
              );
        return 0;
    }
#else
    if (ioto->registered) {
        rInfo("ioto", "Device already registered");
        return 0;
    }
#endif
    if (!ioto->product || smatch(ioto->product, "PUT-YOUR-PRODUCT-ID-HERE")) {
        rError("ioto", "Cannot register device, missing \"product\" in config/device.json5");
        return R_ERR_BAD_ARGS;
    }
    if (!ioto->id || *ioto->id == '\0') {
        rError("ioto", "Cannot register device, missing device \"id\" in config/device.json5");
        return R_ERR_BAD_ARGS;

    } else if (smatch(ioto->id, "auto")) {
        ioto->id = cryptID(10);
        rInfo("ioto", "Generated device claim ID %s", ioto->id);
        jsonSet(ioto->config, 0, "device.id", ioto->id, JSON_STRING);

        if (!ioto->nosave && !ioto->noSaveDevice) {
            path = rGetFilePath(IO_DEVICE_FILE);
            if (jsonSave(ioto->config, 0, "device", path, 0600, JSON_HUMAN) < 0) {
                rError("ioto", "Cannot save device registration to %s", path);
                return R_ERR_CANT_WRITE;
            }
        }
    }
    params = jsonAlloc();
    jsonBlend(params, 0, 0, ioto->config, 0, "device", 0);

#if SERVICES_CLOUD
    /*
        If the device.json5 "account" and "cloud" properties are set to the user's device manager
        account and cloud (Account Settings) then auto-claim.
     */
    if (ioto->account) {
        jsonSet(params, 0, "account", ioto->account, JSON_STRING);
    }
    if (ioto->cloud) {
        jsonSet(params, 0, "cloud", ioto->cloud, JSON_STRING);
    }
#endif
    jsonSetDate(params, 0, "created", 0);
    test = jsonGetBool(params, 0, "test", 0);

    data = jsonToString(params, 0, 0, JSON_JSON);
    jsonFree(params);

    if (once++ == 0) {
        rInfo("ioto", "Registering %sdevice with %s", test ? "test " : "", ioto->builder);
    }

    // SECURITY Acceptable: - the ioto-api is provided by the developer configuration and is secure
    sfmtbuf(url, sizeof(url), "%s/device/register", ioto->builder);
    response = urlPostJson(url, data, 0, "Authorization: bearer %s\r\nContent-Type: application/json\r\n",
                           ioto->product);
    rFree(data);

    rc = parseRegisterResponse(response);
    jsonFree(response);
    return rc;
}

/*
    Parse register response
 */
static int parseRegisterResponse(Json *json)
{
    char       *path;

    /*
        Security: The registration response is trusted and is used to configure the device.
        The device security is thus dependent on the security of the registration server.
     */
    if (json == 0 || json->count < 2) {
        rError("ioto", "Cannot register device");
        return R_ERR_CANT_COMPLETE;
    }
    if (rEmitLog("debug", "ioto")) {
        rDebug("ioto", "Device register response: %s", jsonString(json, JSON_HUMAN));
    }
    /*
        Response will have 2 elements when registered but not claimed
     */
    if (json->count < 2) {
        return 0;
#if SERVICES_CLOUD
    } else if (json->count == 2 && ioto->provisionService) {
        static int once = 0;
        //  Registered but not yet claimed and not auto claiming
        if (once++ == 0 && !ioto->account && !ioto->cloud) {
            rInfo("ioto", "Device not yet claimed. Claim %s with the product device app.", ioto->id);
        }
#endif
    }

    /*
        Update registration info in the provision.json5 and in-memory config.
     */
    jsonRemove(ioto->config, 0, "provision");
    jsonBlend(ioto->config, 0, "provision", json, 0, 0, 0);

    if (rEmitLog("debug", "ioto")) {
        rDebug("ioto", "Provisioning: %s", jsonString(json, JSON_HUMAN));
    }

    if (!ioto->nosave && !ioto->noSaveDevice) {
        path = rGetFilePath(IO_PROVISION_FILE);
        if (jsonSave(ioto->config, 0, "provision", path, 0600, JSON_JSON5 | JSON_MULTILINE) < 0) {
            rError("ioto", "Cannot save device provisioning to %s", path);
            rFree(path);
            return R_ERR_CANT_WRITE;
        }
        rFree(path);
    }

#if SERVICES_CLOUD
    rFree(ioto->api);
    ioto->api = jsonGetClone(ioto->config, 0, "provision.api", 0);
    rFree(ioto->apiToken);
    ioto->apiToken = jsonGetClone(ioto->config, 0, "provision.token", 0);
#endif

    ioto->registered = jsonGetBool(ioto->config, 0, "provision.registered", 0);
    rSignal("device:registered");
    return 0;
}

#endif /* SERVICES_REGISTER */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
