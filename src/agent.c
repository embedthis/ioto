/*
    ioto.c - Primary Ioto control

    This code runs on a fiber and can block, yield and create fibers.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

/*********************************** Globals **********************************/

PUBLIC Ioto *ioto = NULL;

/*********************************** Defines **********************************/

#define TRACE_FILTER         "stderr:raw,error,info,!debug:all,!mbedtls"
#define TRACE_VERBOSE_FILTER "stdout:raw,error,info,trace,!debug:all,!mbedtls"
#define TRACE_DEBUG_FILTER   "stdout:all:all,!mbedtls"
#define TRACE_FORMAT         "%A: %M"

/*********************************** Forwards *********************************/

static int initServices(void);

/************************************* Code ***********************************/
/*
    Allocate the global Ioto singleton
 */
PUBLIC Ioto *ioAlloc(void)
{
    ioto = rAllocType(Ioto);
    return ioto;
}

PUBLIC void ioFree(void)
{
    if (ioto) {
        //  ioto members freed in ioTerm and ioTermConfig
        rFree(ioto);
    }
    ioto = NULL;
}

/*
    Initialize after ioInitSetup
 */
PUBLIC void ioInit(void)
{
    assert(!rIsMain());

    if (!ioto) {
        ioAlloc();
    }
    if (initServices() < 0) {
        rStop();
        return;
    }
    if (rGetState() != R_INITIALIZED) {
        return;
    }
    ioto->ready = 1;
    rSetState(R_READY);
    rInfo("ioto", "Ioto ready");
    rSignal("app:ready");
    if (ioStart() < 0) {
        rError("ioto", "Cannot start Ioto, ioStart() failed");
        rStop();
    }
}

/*
    Terminate Ioto.
    If doing a reset, run the script "scripts/reset" before leaving.
 */
PUBLIC void ioTerm(void)
{
#if ME_UNIX_LIKE
    char *output, *script;
    int  status;

    if (rGetState() == R_RESTART) {
        //  TermServices will release ioto->config. Get persistent reference to script.
        script = jsonGetClone(ioto->config, 0, "scripts.reset", 0);
    } else {
        script = 0;
    }
#endif
    ioto->ready = 0;
    ioStop();
#if SERVICES_WEB
    ioTermWeb();
#endif
#if SERVICES_CLOUD
    ioTermCloud();
#endif
#if SERVICES_DATABASE
    ioTermDb();
#endif
    ioTermConfig();

#if ME_UNIX_LIKE
    if (script && *script) {
        /*
            Security: The reset script is configured via a config file. Ensure the config files
            have permissions that prevent modification by unauthorized users.
            SECURITY Acceptable: - the script is provided by the developer configuration scripts.reset and is secure.
         */
        status = rRun(script, &output);
        if (status != 0) {
            rError("ioto", "Reset script failure: %d, %s", status, output);
        }
        rFree(output);
    }
    rFree(script);
#endif
}

/*
    Start the Ioto runtime
 */
PUBLIC int ioStartRuntime(int verbose)
{
    cchar *filter;

    if (rInit((RFiberProc) NULL, NULL) < 0) {
        return R_ERR_CANT_INITIALIZE;
    }
    if (verbose == 1) {
        filter = TRACE_VERBOSE_FILTER;
    } else if (verbose >= 2) {
        filter = TRACE_DEBUG_FILTER;
    } else {
        filter = NULL;
    }
    if (filter && rSetLog(filter, NULL, 1) < 0) {
        rTerm();
        return R_ERR_CANT_INITIALIZE;
    }
    if (ioAlloc() == NULL) {
        return R_ERR_MEMORY;
    }
    return 0;
}

PUBLIC void ioStopRuntime(void)
{
    rTerm();
}

/*
    Run Ioto. This will block and service events forever (or till instructed to stop)
    Should be called from main()
    The fn argument is not used, but helps build systems to ensure it is included in the build.
 */
PUBLIC int ioRun(void *fn)
{
    rSleep(0);

    while (rGetState() < R_STOPPING) {
        if (ioInitConfig() < 0) {
            rFatal("ioto", "Cannot initialize Ioto\n");
        }
        if (rSpawnFiber("ioInit", (RFiberProc) ioInit, NULL) < 0) {
            rFatal("ioto", "Cannot initialize runtime");
        }
        if (rGetState() < R_STOPPING) {
            //  Service events until instructed to exit
            rServiceEvents();
        }
        ioTerm();
        if (rGetState() == R_RESTART) {
            rTerm();
            rInit(NULL, NULL);
        }
    }
    ioFree();
    rInfo("ioto", "Ioto exiting");
    return 0;
}

/*
    Start services. Order of initialization matters.
    We initialize MQTT early so that on-demand connections and provisioning may take place.
    Return false if initialization failed. Note: some services may trigger a R_RESTART.
 */
static int initServices(void)
{
#ifdef SERVICES_SERIALIZE
    if (ioto->serializeService) {
        ioSerialize();
    }
#endif
#if SERVICES_REGISTER
    /*
        One-time device registration during manufacturer or first connect.
        NOTE: The Ioto license requires that if this code is removed or disabled, you must manually enter and
        maintain device volumes using Embedthis Builder (https://admin.embedthis.com) or you must have a current
        contract agreement with Embedthis to use an alternate method.
     */
    if (ioto->registerService) {
        if (!ioto->registered && ioRegister() < 0) {
            return R_ERR_BAD_ARGS;
        }
    }
#endif

#if SERVICES_DATABASE
    if (ioto->dbService && ioInitDb() < 0) {
        return R_ERR_CANT_READ;
    }
#endif
#if SERVICES_WEB
    if (ioto->webService && ioInitWeb() < 0) {
        return R_ERR_CANT_INITIALIZE;
    }
#endif
#if SERVICES_CLOUD
    if (ioto->cloudService && ioInitCloud() < 0) {
        return R_ERR_CANT_INITIALIZE;
    }
#endif
#if SERVICES_AI
    ioto->aiService = 1;
    if (ioto->aiService && ioInitAI() < 0) {
        return R_ERR_CANT_INITIALIZE;
    }
#endif
#if SERVICES_UPDATE
    if (ioto->updateService) {
        //  Delay to allow provisioning to complete
        rStartEvent((RFiberProc) ioUpdate, 0, 15 * TPS);
    }
#endif
#if ME_DEBUG
    //  Used to test memory leaks after running for a period of time
    if (getenv("VALGRIND")) {
        rStartEvent((REventProc) rStop, 0, 60 * TPS);
    }
#endif
    return 0;
}

/*
    Update the log output configuration. This may be called at startup and after cloud provisioning to
    redirect the device log to the cloud.
 */
PUBLIC int ioUpdateLog(bool force)
{
    Json  *json;
    cchar *dir, *format, *path, *source, *types;
    char  *fullPath;

    json = ioto->config;
    types = source = 0;

    format = jsonGet(json, 0, "log.format", "%T %S: %M");
    path = jsonGet(json, 0, "log.path", "stdout");
    source = jsonGet(json, 0, "log.sources", "all,!mbedtls");
    types = jsonGet(json, 0, "log.types", "error,info");
    dir = jsonGet(json, 0, "directories.log", ".");

    rSetLogFormat(format, force);
    rSetLogFilter(types, source, force);

    if (smatch(path, "default")) {
        path = IO_LOG_FILE;

#if SERVICES_CLOUD
    } else if (smatch(path, "cloud")) {
        if (ioto->awsAccess) {
            //  This will register a new log handler
            ioEnableCloudLog();
        }
        return 0;
#endif
    }
    /*
        SECURITY Acceptable: - the log path is provided by the developer log.path and is deemed secure
     */
    fullPath = rJoinFile(dir, path);
    if (rSetLogPath(fullPath, force) < 0) {
        rError("ioto", "Cannot open log %s", fullPath);
        rFree(fullPath);
        return R_ERR_CANT_OPEN;
    }
    rFree(fullPath);
    return 0;
}

/*
    Convenience routine over jsonGet.
 */
PUBLIC cchar *ioGetConfig(cchar *key, cchar *defaultValue)
{
    return jsonGet(ioto->config, 0, key, defaultValue);
}

PUBLIC int ioGetConfigInt(cchar *key, int defaultValue)
{
    return jsonGetInt(ioto->config, 0, key, defaultValue);
}

/*
    Expand ${references} in the "str" using properties variables in ioto->properties
 */
 PUBLIC char *ioExpand(cchar *str)
 {
     return jsonTemplate(ioto->properties, str, 1);
 }
 
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
