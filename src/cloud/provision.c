/*
    provision.c - Provision the device with MQTT certificates and API endpoints

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

#if SERVICES_PROVISION
/*********************************** Locals ***********************************/

#define PROVISION_MAX_DELAY (24 * 60 * 60 * TPS)

static REvent provisionEvent;
static bool   provisioning = 0;

/*********************************** Forwards *********************************/

static bool parseProvisioningResponse(Json *json);
static bool provisionDevice(void);
static void postProvisionSync(void);
static void releaseProvisioning(const MqttRecv *rp);
static int  startProvision(void);
static void subscribeProvisioningEvents(void);

#if SERVICES_KEYS
static void extractKeys(void);
#endif

/************************************* Code ***********************************/
/*
    Initialize the provisioner service.
    Always watch for the deprovisioned signal and reprovision.
 */
PUBLIC int ioInitProvisioner(void)
{
    rWatch("mqtt:connected", (RWatchProc) subscribeProvisioningEvents, 0);
    rWatch("cloud:deprovisioned", (RWatchProc) startProvision, 0);
    if (!ioto->endpoint) {
        startProvision();
    }
    return 0;
}

PUBLIC void ioTermProvisioner(void)
{
    rWatchOff("mqtt:connected", (RWatchProc) subscribeProvisioningEvents, 0);
    rWatchOff("cloud:deprovisioned", (RWatchProc) startProvision, 0);
}

/*
    Start the provisioner service if not already provisioned.
    Can also be called by the user to immmediately provision incase backed off.
 */
PUBLIC void ioStartProvisioner(void)
{
    if (!ioto->endpoint) {
        startProvision();
    }
}

/*
    Provision the device from the device cloud. This blocks until claimed and provisioned.
    If called when already provisioned, it will return immediately.
    This code is idempotent. May block for a long time.
 */
static int startProvision(void)
{
    Ticks delay;

    // Wake any existing provisioner
    ioResumeBackoff(&provisionEvent);

    /*
        Wait for device to be claimed (will set api)
     */
    rEnter(&provisioning, 0);
    if (!ioto->endpoint) {
        for (delay = TPS; !ioto->api && delay; delay = ioBackoff(delay, &provisionEvent)) {
            if (ioRegister() == R_ERR_BAD_ARGS) {
                return R_ERR_BAD_ARGS;
            }
            if (ioto->api) break;
        }
        for (delay = TPS; !ioto->endpoint; delay = ioBackoff(delay, &provisionEvent)) {
            if (provisionDevice()) {
                break;
            }
        }
        if (ioto->endpoint) {
            rSignal("cloud:provisioned");
        } else {
            rInfo("ioto", "Provisioning device, waiting for device to be claimed ...");
        }
    }
    rLeave(&provisioning);
    return 0;
}

/*
    Send a provisioning request to the device cloud
 */
static bool provisionDevice(void)
{
    Json *json;
    char data[80], url[512];

    /*
        Talk to the device cloud to get certificates
        SECURITY Acceptable:: ioto->api is of limited length and is not a security risk.
     */
    SFMT(url, "%s/tok/device/provision", ioto->api);
    SFMT(data, "{\"id\":\"%s\"}", ioto->id);

    json = urlPostJson(url, data, 0, "Authorization: bearer %s\r\nContent-Type: application/json\r\n", ioto->apiToken);

    if (json == 0 || json->count == 0) {
        rError("ioto", "Error provisioning device");
        jsonFree(json);
        return 0;
    }
    if (!parseProvisioningResponse(json)) {
        return 0;
    }
    jsonFree(json);
    return 1;
}

/*
    Parse provisioning response payload from the device cloud.
    This saves the response in provision.json5 and sets ioto->api if provisioned.
 */
static bool parseProvisioningResponse(Json *json)
{
    cchar *certificate, *error, *key;
    char  *certMem, *keyMem, *path;
    int   delay;

    key = certificate = path = 0;

    if ((error = jsonGet(json, 0, "error", 0)) != 0) {
        delay = jsonGetInt(json, 0, "delay", 0);
        if (delay > 0) {
            ioto->blockedUntil = rGetTime() + delay * TPS;
            rError("ioto", "Device is temporarily blocked for %d seconds due to persistent excessive I/O", delay);
            return 0;
        }
    }
    rInfo("ioto", "Device claimed");

    /*
        Extract provisioning certificates for MQTT communications with AWS IoT
     */
    certificate = jsonGet(json, 0, "certificate", 0);
    key = jsonGet(json, 0, "key", 0);
    if (!certificate || !key) {
        rError("ioto", "Provisioning is missing certificate");
        return 0;
    }
    if (ioto->nosave) {
        certMem = sfmt("@%s", certificate);
        keyMem = sfmt("@%s", key);
        jsonSet(json, 0, "certificate", certMem, JSON_STRING);
        jsonSet(json, 0, "key", keyMem, JSON_STRING);
        rFree(certMem);
        rFree(keyMem);

    } else {
        path = rGetFilePath(IO_CERTIFICATE);
        if (rWriteFile(path, certificate, slen(certificate), 0600) < 0) {
            rError("ioto", "Cannot save certificate to %s", path);
        } else {
            jsonSet(json, 0, "certificate", path, JSON_STRING);
        }
        rFree(path);

        path = rGetFilePath(IO_KEY);
        if (rWriteFile(path, key, slen(key), 0600) < 0) {
            rError("ioto", "Cannot save key to %s", path);
        } else {
            jsonSet(json, 0, "key", path, JSON_STRING);
        }
        rFree(path);
    }
    jsonRemove(json, 0, "cert");
    jsonBlend(ioto->config, 0, "provision", json, 0, 0, 0);

    if (rEmitLog("debug", "provision")) {
        rDebug("provision", "%s", jsonString(json, JSON_HUMAN));
    }
    if (!ioto->nosave) {
        path = rGetFilePath(IO_PROVISION_FILE);
        if (jsonSave(ioto->config, 0, "provision", path, 0600, JSON_JSON5 | JSON_MULTILINE) < 0) {
            rError("ioto", "Cannot save provisioning state to %s", path);
            rFree(path);
            return 0;
        }
        rFree(path);
    }
    rFree(ioto->account);
    ioto->account = jsonGetClone(ioto->config, 0, "provision.accountId", 0);
    dbAddContext(ioto->db, "accountId", ioto->account);

    rFree(ioto->cloudType);
    ioto->cloudType = jsonGetClone(ioto->config, 0, "provision.cloudType", 0);
    rFree(ioto->endpoint);
    ioto->endpoint = jsonGetClone(ioto->config, 0, "provision.endpoint", 0);

    rInfo("ioto", "Device provisioned for %s cloud \"%s\" in %s",
          jsonGet(ioto->config, 0, "provision.cloudType", 0),
          jsonGet(ioto->config, 0, "provision.cloudName", 0),
          jsonGet(ioto->config, 0, "provision.cloudRegion", 0)
          );

#if SERVICES_SYNC
    rWatch("mqtt:connected", (RWatchProc) postProvisionSync, 0);
#endif
    //  Run by event to decrease stack length
    rStartEvent((REventProc) rSignal, "device:provisioned", 0);

#if SERVICES_KEYS
    if (ioto->keyService && smatch(ioto->cloudType, "dedicated")) {
        ioGetKeys();
    }
#endif
    return 1;
}

/*
    One-time db sync after provisioning
 */
static void postProvisionSync(void)
{
    ioSyncUp(0, 1);
    rWatchOff("mqtt:connected", (RWatchProc) postProvisionSync, 0);
}

/*
    Called on signal mqtt:connected to subscribe for provisioning events from the cloud
 */
static void subscribeProvisioningEvents(void)
{
    mqttSubscribe(ioto->mqtt, releaseProvisioning, 1, MQTT_WAIT_NONE, "ioto/device/%s/provision/+", ioto->id);
}

/*
    Receive provisioning command (release)
 */
static void releaseProvisioning(const MqttRecv *rp)
{
    Time  timestamp;
    cchar *cmd;

    cmd = rBasename(rp->topic);
    if (smatch(cmd, "release")) {
        timestamp = stoi(rp->data);
        if (timestamp == 0) {
            timestamp = rGetTime();
        }
        /*
            Ignore stale release commands that IoT Core may be resending
            If really deprovisioned, then the connection will fail and mqtt will reprovision after 3 failed retries.
         */
        if (rGetTime() < (timestamp + 10 * TPS)) {
            //  Unit tests may get a stale restart command
            rInfo("ioto", "Received provisioning command %s", rp->topic);
            dbSetField(ioto->db, "Device", "connection", "offline", DB_PROPS("id", ioto->id), DB_PARAMS());
            if (ioto->connected) {
                ioDisconnect();
            }
            ioDeprovision();
        }
    } else {
        rError("ioto", "Unknown provision command %s", cmd);
    }
}

/*
    Deprovision the device.
    This is atomic and will not block. Also idempotent.
 */
PUBLIC void ioDeprovision(void)
{
    char *path;

    rFree(ioto->api);
    ioto->api = 0;
    rFree(ioto->apiToken);
    ioto->apiToken = 0;
    rFree(ioto->account);
    ioto->account = 0;
    rFree(ioto->endpoint);
    ioto->endpoint = 0;
    rFree(ioto->cloudType);
    ioto->cloudType = 0;
    ioto->registered = 0;

    jsonSet(ioto->config, 0, "provision.certificate", 0, 0);
    jsonSet(ioto->config, 0, "provision.key", 0, 0);
    jsonSet(ioto->config, 0, "provision.endpoint", 0, 0);
    jsonSet(ioto->config, 0, "provision.accountId", 0, 0);
    jsonSet(ioto->config, 0, "provision.cloudType", 0, 0);

    //  Remove certificates
    path = rGetFilePath(IO_CERTIFICATE);
    unlink(path);
    rFree(path);

    path = rGetFilePath(IO_KEY);
    unlink(path);
    rFree(path);

    //  Remove provisioning state
    jsonRemove(ioto->config, 0, "provision");

    path = rGetFilePath(IO_PROVISION_FILE);
    unlink(path);
    rFree(path);
    rInfo("ioto", "Device deprovisioned");

    rSignal("cloud:deprovisioned");
}

#if SERVICES_KEYS
/*
    Renew device IAM credentials
 */
PUBLIC void ioGetKeys(void)
{
    Json  *json, *config;
    char  url[512];
    Ticks delay;

    SFMT(url, "%s/tok/device/getCreds", ioto->api);

    json = urlPostJson(url, 0, -1, "Authorization: bearer %s\r\nContent-Type: application/json\r\n", ioto->apiToken);
    if (json == 0) {
        rError("ioto", "Cannot get credentials");
        return;
    }
    /*
        Blend into in-memory config so we can keep persistent links to the key values.
     */
    config = ioto->config;
    jsonBlend(config, 0, "provision.keys", json, 0, 0, 0);

    extractKeys();
    jsonFree(json);

    delay = min((ioto->awsExpires - rGetTime()) - (20 * 60 * TPS), 5 * 60 * TPS);
    rStartEvent((REventProc) ioGetKeys, 0, delay);
}

static void extractKeys(void)
{
    cchar *prior;
    int   pid;

    pid = jsonGetId(ioto->config, 0, "provision.keys");

    prior = ioto->awsAccess;
    ioto->awsAccess = jsonGetClone(ioto->config, pid, "accessKeyId", 0);
    ioto->awsSecret = jsonGetClone(ioto->config, pid, "secretAccessKey", 0);
    ioto->awsToken = jsonGetClone(ioto->config, pid, "sessionToken", 0);
    ioto->awsRegion = jsonGetClone(ioto->config, pid, "region", 0);
    ioto->awsExpires = rParseIsoDate(jsonGet(ioto->config, pid, "expires", 0));

    /*
        Update logging on first time key fetch
     */
    if (!prior) {
        ioUpdateLog(0);
    }
    rSignal("device:keys");
}
#endif /* SERVICES_KEYS */

#else
void dummyProvision(void)
{
}
#endif /* SERVICES_PROVISION */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
