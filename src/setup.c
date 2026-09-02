/*
    setup.c - Setup for Ioto. Load configuration files.

    This code is intended to run from the main fiber and should not yield, block or create fibers.

    Users can use access most common fields via the "ioto->" object.
    Can also use jsonGet and jsonGet(ioto->config) to read config values.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

/************************************ Forwards *********************************/

static int blendConditional(Json *json, cchar *property);
static int loadJson(Json *json, cchar *property, cchar *filename, bool optional);
static Json *makeTemplate(void);
static void enableServices(void);
static void reset(void);

/************************************* Code ***********************************/
/*
    Load config.json and provision.json into config
 */
PUBLIC int ioInitConfig(void)
{
    Json   *json;
    size_t stackInitial, stackMax, stackGrow, stackReset;
    int    maxFibers, poolMin, poolMax;

    assert(rIsMain());

    if (!ioto) {
        ioAlloc();
    }
    if (ioLoadConfig() < 0) {
        return R_ERR_CANT_READ;
    }
    if (ioto->cmdReset) {
        reset();
    }
    /*
        Callback for users to modify config at runtime
     */
    json = ioto->config;
    ioConfig(json);

    // Configure the fiber pool. A value of zero keeps the default.
    maxFibers = svaluei(jsonGet(json, 0, "limits.fibers", "4"));
    poolMin = svaluei(jsonGet(json, 0, "limits.fiberPoolMin", "0"));
    poolMax = svaluei(jsonGet(json, 0, "limits.fiberPoolMax", "4"));
    rSetFiberLimits(maxFibers, poolMin, poolMax);

    //  Configure fiber stack limits if specified. A value of zero keeps the default.
    stackInitial = (size_t) svalue(jsonGet(json, 0, "limits.fiberStack", "0"));
    if (stackInitial == 0) {
        // Backwards compatibility with old "limits.stack" property.
        stackInitial = (size_t) svalue(jsonGet(json, 0, "limits.stack", "0"));
    }
    stackMax = (size_t) svalue(jsonGet(json, 0, "limits.fiberStackMax", "0"));
    stackGrow = (size_t) svalue(jsonGet(json, 0, "limits.fiberStackGrow", "0"));
    stackReset = (size_t) svalue(jsonGet(json, 0, "limits.fiberStackReset", "0"));
    rSetFiberStackLimits(stackInitial, stackMax, stackGrow, stackReset);


    if (ioto->cmdId) {
        jsonSet(json, 0, "device.id", ioto->cmdId, JSON_STRING);
    }
    if (ioto->cmdProduct) {
        jsonSet(json, 0, "device.product", ioto->cmdProduct, JSON_STRING);
    }
    if (ioto->cmdProfile) {
        jsonSet(json, 0, "profile", ioto->cmdProfile, JSON_STRING);
    }
    ioto->logDir = jsonGetClone(json, 0, "directories.log", ".");
    ioto->profile = jsonGetClone(json, 0, "profile", "dev");
    ioto->app = jsonGetClone(json, 0, "app", "blank");
    ioto->product = jsonGetClone(json, 0, "device.product", 0);
    ioto->version = jsonGetClone(json, 0, "version", "1.0.0");
    ioto->properties = makeTemplate();

#if SERVICES_CLOUD
    if (ioInitCloudConfig() < 0) {
        return R_ERR_CANT_INITIALIZE;
    }
#endif

#if ME_COM_SSL
    /*
        Root CA to use for URL requests to external services
     */
    char *authority = (char*) jsonGet(json, 0, "tls.authority", 0);
    if (authority) {
        authority = rGetFilePath(authority);
        if (rAccessFile(authority, R_OK) == 0) {
            rSetSocketDefaultCerts(authority, NULL, NULL, NULL);
        } else {
            rError("ioto", "Cannot access TLS root certificates \"%s\"", authority);
            rFree(authority);
            return R_ERR_CANT_INITIALIZE;
        }
        rFree(authority);
    }
#endif
    ioUpdateLog(0);
    rInfo("ioto", "Starting Ioto %s, with \"%s\" app %s, using \"%s\" profile",
          ME_VERSION, ioto->app, ioto->version, ioto->profile);
    enableServices();
    return 0;
}

PUBLIC void ioTermConfig(void)
{
    jsonFree(ioto->config);
    jsonFree(ioto->properties);

    rFree(ioto->app);
    rFree(ioto->cmdConfigDir);
    rFree(ioto->cmdStateDir);
    rFree(ioto->cmdSync);
    rFree(ioto->logDir);
    rFree(ioto->profile);
    rFree(ioto->product);
    rFree(ioto->version);

    ioto->app = 0;
    ioto->config = 0;
    ioto->logDir = 0;
    ioto->profile = 0;
    ioto->product = 0;
    ioto->version = 0;
    ioto->properties = 0;

#if SERVICES_CLOUD
    ioTermCloudConfig();
#endif
}

/*
    Load the configuration from the config JSON files.
    This loads each JSON file and blends the results into the ioto->config JSON tree.
 */
PUBLIC int ioLoadConfig(void)
{
    Json  *json;
    cchar *dir;
    char  *path;

    json = ioto->config = jsonAlloc();

    /*
        Command line --config, --state and --ioto can set the config/state and ioto.json paths.
        SECURITY Acceptable:: ioto->cmdStateDir is set internally and is not a security risk.
     */
    rAddDirectory("state", ioto->cmdStateDir ? ioto->cmdStateDir : IO_STATE_DIR);

    if (ioto->cmdConfigDir) {
        rAddDirectory("config", ioto->cmdConfigDir);
    } else if (ioto->cmdIotoFile) {
        path = rDirname(sclone(ioto->cmdIotoFile));
        rAddDirectory("config", path);
        rFree(path);
    } else if (rAccessFile("state/config/ioto.json5", R_OK) == 0) {
        rAddDirectory("config", "state/config");
    } else if (rAccessFile("ioto.json5", R_OK) == 0) {
        rAddDirectory("config", ".");
    } else if (rAccessFile("config/ioto.json5", R_OK) == 0) {
        rAddDirectory("config", "config");
    } else {
        rAddDirectory("config", "@state/config");
    }

    if (loadJson(json, NULL, ioto->cmdIotoFile ? ioto->cmdIotoFile : IO_CONFIG_FILE, 0) < 0) {
        return R_ERR_CANT_READ;
    }
    if (json->count == 0) {
        rInfo("ioto", "Cannot find valid \"%s\" config file", IO_CONFIG_FILE);
    }
    if (loadJson(json, NULL, IO_LOCAL_FILE, 1) < 0) {
        return R_ERR_CANT_READ;
    }
#if SERVICES_WEB
    if (loadJson(json, "web", IO_WEB_FILE, 1) < 0) {
        return R_ERR_CANT_READ;
    }
#endif
    if (loadJson(json, "device", IO_DEVICE_FILE, 1) < 0) {
        return R_ERR_CANT_READ;
    }
#if SERVICES_PROVISION
    if (!ioto->cmdReset) {
        if (loadJson(json, "provision", IO_PROVISION_FILE, 1) < 0) {
            return R_ERR_CANT_READ;
        }
    }
#endif
    //  Last chance local overrides
    if (loadJson(json, NULL, IO_LOCAL_FILE, 1) < 0) {
        return R_ERR_CANT_READ;
    }
    if (ioto->cmdStateDir) {
        //  Override state directory with command line
        jsonSet(json, 0, "directories.state", ioto->cmdStateDir, JSON_STRING);
    }
#if !ESP32 && !FREERTOS
    cchar *stateDir;
    if ((stateDir = jsonGet(json, 0, "directories.state", 0)) != 0) {
        rAddDirectory("state", stateDir);
    }
#endif
    if ((dir = jsonGet(json, 0, "directories.db", 0)) != 0) {
        rAddDirectory("db", dir);
    } else {
        rAddDirectory("db", "@state/db");
    }
    if ((dir = jsonGet(json, 0, "directories.certs", 0)) != 0) {
        rAddDirectory("certs", dir);
    } else {
        rAddDirectory("certs", "@state/certs");
    }
    if ((dir = jsonGet(json, 0, "directories.site", 0)) != 0) {
        rAddDirectory("site", dir);
    } else {
        rAddDirectory("site", "@state/site");
    }
    if (rEmitLog("debug", "ioto")) {
        rDebug("ioto", "%s", jsonString(json, JSON_HUMAN));
    }
    return 0;
}

/*
    Determine which services to enable
 */
static void enableServices(void)
{
    Json *config;
    int  sid;

    config = ioto->config;

    sid = jsonGetId(config, 0, "services");
    if (sid < 0) {
        ioto->webService = 1;
    } else {
        //  Defaults to true if no config.json
        ioto->aiService = jsonGetBool(config, sid, "ai", 0);
        ioto->dbService = jsonGetBool(config, sid, "database", 1);
        ioto->testService = jsonGetBool(config, sid, "test", 0);
        ioto->updateService = jsonGetBool(config, sid, "update", 0);
        ioto->webService = jsonGetBool(config, sid, "web", 1);

#if SERVICES_CLOUD
        ioEnableCloudServices(config, sid);
#endif
    }
    rInfo("ioto", "Enabling services: %s%s%s%s%s%s",
          ioto->aiService ? "ai " : "",
          ioto->dbService ? "db " : "",
          ioto->mqttService ? "mqtt " : "",
          ioto->testService ? "test " : "",
          ioto->updateService ? "update " : "",
          ioto->webService ? "web" : "");

}

/*
    Load a json "filename" from the "dir" and blend into the existing JSON tree at the given "property".
 */
static int loadJson(Json *json, cchar *property, cchar *filename, bool optional)
{
    Json *extra;
    char *path;

    path = rGetFilePath(filename);
    if (rAccessFile(path, F_OK) < 0) {
        if (!optional) {
            rError("ioto", "Cannot find required file %s", path);
            rFree(path);
            return R_ERR_CANT_FIND;
        }
        rFree(path);
        return 0;
    }
    if ((extra = jsonParseFile(path, NULL, 0)) == 0) {
        rError("ioto", "Cannot parse %s", path);
        rFree(path);
        return R_ERR_CANT_READ;
    }
    rDebug("ioto", "Loading %s", path);

    if (jsonBlend(json, 0, property, extra, 0, 0, 0) < 0) {
        rError("ioto", "Cannot blend %s", path);
        jsonFree(extra);
        rFree(path);
        return R_ERR_CANT_READ;
    }
    jsonFree(extra);
    if (blendConditional(json, property) < 0) {
        rFree(path);
        return R_ERR_CANT_READ;
    }
    rFree(path);
    return 0;
}

static int blendConditional(Json *json, cchar *property)
{
    Json     *conditional;
    JsonNode *collection;
    cchar    *value;
    char     *text;
    int      rootId, id;

    rootId = jsonGetId(json, 0, property);
    if (rootId < 0) {
        return 0;
    }
    /*
        Extract the conditional set as we can't iterate while mutating the JSON
     */
    if ((text = jsonToString(json, rootId, "conditional", 0)) == NULL) {
        return 0;
    }
    conditional = jsonParseKeep(text, 0);
    if (!conditional) {
        return 0;
    }
    for (ITERATE_JSON(conditional, NULL, collection, nid)) {
        value = 0;
        if (smatch(collection->name, "profile")) {
            if ((value = ioto->cmdProfile) == 0) {
                value = jsonGet(ioto->config, 0, "profile", "dev");
            }
        }
        if (!value) {
            //  Get the profile name
            value = jsonGet(json, 0, collection->name, 0);
        }
        if (value) {
            //  Profile exists, so find the target id
            id = jsonGetId(conditional, jsonGetNodeId(conditional, collection), value);
            if (id >= 0) {
                if (jsonBlend(json, 0, property, conditional, id, 0, JSON_COMBINE) < 0) {
                    rError("ioto", "Cannot blend %s", collection->name);
                    return R_ERR_CANT_COMPLETE;
                }
            }
        }
    }
    jsonRemove(json, rootId, "conditional");
    jsonFree(conditional);
    return 0;
}

/*
    Make a JSON collection of properties to be used with iotoExpand
 */
static Json *makeTemplate(void)
{
    Json *json;
    char hostname[ME_MAX_FNAME];

    json = jsonAlloc();
    if (gethostname(hostname, sizeof(hostname)) < 0) {
        scopy(hostname, sizeof(hostname), "localhost");
    }
    hostname[sizeof(hostname) - 1] = '\0';
    jsonSet(json, 0, "hostname", hostname, 0);
#if SERVICES_CLOUD
    jsonSet(json, 0, "id", ioto->id, 0);
    jsonSet(json, 0, "instance", ioto->instance ? ioto->instance : hostname, 0);
#endif
    return json;
}

/*
    Set a template variable in the ioto->template collection
 */
PUBLIC void ioSetTemplateVar(cchar *key, cchar *value)
{
    jsonSet(ioto->properties, 0, key, value, 0);
}

static void removeFile(cchar *file)
{
    char *path;

    path = rGetFilePath(file);
    unlink(path);
    rFree(path);
}

/*
    Hardware reset (--reset)
 */
static void reset(void)
{
    char *dest, *path;

    rInfo("main", "Reset to factory defaults");

    removeFile(IO_PROVISION_FILE);
    removeFile(IO_SHADOW_FILE);
    removeFile(IO_CERTIFICATE);
    removeFile(IO_KEY);
    removeFile("@db/device.db.jnl");
    removeFile("@db/device.db.sync");
    /*
        SECURITY Acceptable:: TOCTOU race risk is accepted. Expect file system to be secured.
     */
    path = rGetFilePath("@db/device.db.reset");
    if (rAccessFile(path, R_OK) == 0) {
        dest = rGetFilePath("@db/device.db");
        rCopyFile(path, dest, 0664);
        rFree(dest);
    } else {
        removeFile("@db/device.db");
    }
    rFree(path);
}

/*
    Help some older linkers with references to libraries
    MakeMe struggles with order of libraries and Windows depends on a set order.
 */
#if ME_COM_WEBSOCK
PUBLIC void *WebSocketsRef = webSocketAlloc;
#endif
#if ME_COM_WEB
PUBLIC void *WebRef = webAlloc;
#endif
#if ME_COM_URL
PUBLIC void *UrlRef = urlAlloc;
#endif
#if ME_COM_JSON
PUBLIC void *JsonRef = jsonAlloc;
#endif

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
