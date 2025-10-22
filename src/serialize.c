/*
    serialize.c - Manufacture dynamic serialization

    This module gets a Unique device claim ID (10 character UDI).
    If the "services.serialize" is set to "auto", this module will dynamically create a random device ID.
    If set to "factory", ioSerialize() will call the factory serialization service defined via
    the "api.serialize" URL setting. The resultant deviceId is saved in the config/device.json5 file.

    SECURITY Acceptable: This program is a developer / manufacturing tool and is not used in production devices.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

#ifdef SERVICES_SERIALIZE
/*********************************** Locals ***********************************/

#define SERIALIZE_TIMEOUT 30 * 1000

/*********************************** Forwards *********************************/

static bool getSerial(void);

/************************************* Code ***********************************/
/*
    Factory serialization. WARNING: this blocks ioto.
 */
PUBLIC void ioSerialize(void)
{
    while (!ioto->id) {
        getSerial();
        if (ioto->id) {
            break;
        }
        rSleep(2 * 1000);
    }
#if SERVICES_CLOUD
    if (ioto->id) {
        rInfo("ioto", "Device Claim ID: %s", ioto->id);
    }
#endif
}

/*
    Get a Unique device claim ID (UDI)
    This issues a request to the factory serialization service if the "services.serialize" is set to "factory"
    Otherwise is allocates a 10 character claim ID here.
 */
static bool getSerial(void)
{
    Url   *up;
    Json  *config, *result;
    cchar *mode, *serialize;
    char  *def, *id, *path, *saveId;
    int   did;

    id = saveId = 0;
    config = ioto->config;

    /*
        The allocation omde can be: factory, auto, none. Defaults to "auto"
     */
    mode = jsonGet(config, 0, "services.serialize", 0);
    did = jsonGetId(config, 0, "device");
    if (did < 0) {
        jsonSet(config, 0, "device", "", JSON_OBJECT);
        did = jsonGetId(config, 0, "device");
    }
    if (smatch(mode, "factory")) {
        //  Get serialize API endpoint
        if ((serialize = jsonGet(config, 0, "api.serialize", 0)) == 0) {
            rError("serialize", "Missing api.serialize endpoint in config.json");
            return 0;
        }
        up = urlAlloc(0);
        did = jsonGetId(config, 0, "device");

        if (sstarts(serialize, "http")) {
            //  Ask manufacturing controller for device ID
            def = jsonToString(config, did, 0, JSON_JSON);
            urlSetTimeout(up, SERIALIZE_TIMEOUT);
            result = urlJson(up, "POST", serialize, def, 0, 0);
            if (result == 0) {
                rError("serialize", "Cannot fetch device ID from %s: %s", serialize, urlGetError(up));
            } else {
                if ((id = jsonGetClone(result, 0, "id", 0)) == 0) {
                    rError("serialize", "Cannot find device ID in response");
                } else {
                    saveId = id;
                }
                jsonFree(result);
            }
            rFree(def);

#if ME_UNIX_LIKE
        } else {
            cchar *product;
            char  *p, *command, *output;
            int   status;

            product = jsonGet(config, did, "product", 0);
            for (p = (char*) product; *p; p++) {
                if (!isalnum((int) *p)) {
                    rError("serialize", "Product name has invalid characters for command");
                    urlFree(up);
                    return 0;
                }
            }
            /*
                SECURITY Acceptable: This program is a tool not used in production devices.
                This is an acceptable security risk.
             */
            command = sfmt("serialize \"%s\"", product);
            status = rRun(command, &output);
            if (status != 0) {
                rError("serialize", "Cannot serialize %s", command);
                rFree(output);
            } else {
                id = saveId = output;
            }
            rFree(command);
#endif
        }
        urlFree(up);

    } else if (smatch(mode, "none")) {
        ;

    } else { /* Default "auto" */
        id = saveId = cryptID(10);
    }
    if (id) {
        jsonSet(config, did, "id", id, JSON_STRING);
        rFree(ioto->id);
        ioto->id = jsonGetClone(ioto->config, 0, "device.id", 0);
    }
    if (saveId && !ioto->noSaveDevice && !ioto->nosave) {
        path = rGetFilePath(IO_DEVICE_FILE);
        if (jsonSave(config, did, 0, path, 0600, JSON_JSON5 | JSON_MULTILINE) < 0) {
            rError("serialize", "Cannot save serialization to %s", path);
            rFree(path);
            return 0;
        }
        rFree(path);
        rFree(saveId);
    }
    return id ? 1 : 0;
}

#else
void dummySerialize(void)
{
}
#endif /* SERVICES_SERIALIZE */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
