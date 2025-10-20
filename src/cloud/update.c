/*
    update.c - Check for software updates

    Update requires a device cloud and device registration but not provisioning.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

#if SERVICES_UPDATE

/*********************************** Forwards *********************************/

static void applyUpdate(char *path);
static bool checkSum(cchar *path, cchar *checksum);
static int download(cchar *url, cchar *path);

/************************************* Code ***********************************/
/*
    Check for updates with the device cloud.
    This maintains management for this device and checks for updates.
 */
PUBLIC bool ioUpdate(void)
{
    Json  *json;
    Url   *up;
    Time  delay, jitter, lastUpdate, period;
    cchar *apply, *checksum, *image, *response, *schedule, *version;
    char  *body, *date, *headers, *path, url[512];
    bool  enable;

    /*
        Protection incase update fails and device loops continually updating
     */
    if ((enable = jsonGetBool(ioto->config, 0, "update.enable", 0)) == 0) {
        return 0;
    }
    schedule = jsonGet(ioto->config, 0, "update.schedule", "* * * * *");
    jitter = svalue(jsonGet(ioto->config, 0, "update.jitter", "0")) * TPS;
    period = svalue(jsonGet(ioto->config, 0, "update.period", "24 hrs")) * TPS;

    lastUpdate = (Time) rParseIsoDate(dbGetField(ioto->db, "SyncState", "lastUpdate", NULL, DB_PARAMS()));
    delay = lastUpdate + period - rGetTime();
    if (delay < 0) {
        delay = cronUntil(schedule, rGetTime());
    }
    if (!ioto->api && delay <= 0) {
        //  Not yet provisioned
        delay = 60 * TPS;
    }
    if (delay > 0 || !enable) {
        if (jitter) {
            jitter = rand() % jitter;
            delay += jitter;
        }
        rStartEvent((REventProc) ioUpdate, 0, delay);
        return 0;
    }
    json = jsonAlloc();
    jsonBlend(json, 0, 0, ioto->config, 0, "device", 0);
    jsonSet(json, 0, "version", ioto->version, JSON_STRING);
    jsonSet(json, 0, "iotoVersion", ME_VERSION, JSON_STRING);
    body = jsonToString(json, 0, 0, JSON_JSON);
    jsonFree(json);

    SFMT(url, "%s/tok/provision/update", ioto->api);
    rTrace("update", "Builder at %s", ioto->api);

    up = urlAlloc(0);
    urlSetTimeout(up, svalue(jsonGet(ioto->config, 0, "timeouts.api", "30 secs")) * TPS);

    headers = sfmt("Authorization: bearer %s\r\nContent-Type: application/json\r\n", ioto->apiToken);
    rDebug("update", "Request \n%s\n%s\n%s\n\n", url, headers, body);

    if ((json = urlJson(up, "POST", url, body, 0, headers)) == 0) {
        response = urlGetResponse(up);
        rError("ioto", "%s", response);
        if (smatch(response, "Cannot find device") || smatch(response, "Authentication failed")) {
            /*
                The device has either been removed or released. Release certs and re-provision after a restart
             */
            rInfo("ioto", "%s: releasing device and reprovisioning ...", response);
            ioDeprovision();
        } else {
            rError("update", "Cannot update device from device cloud");
        }
    }
    rFree(headers);
    rFree(body);
    urlFree(up);

    if (json) {
        /*
            Got an update response with checksum, version and image url
            SECURITY Acceptable:: The update url is provided by the device cloud and is secure. So an
            additional signature is not required.
         */
        image = jsonGet(json, 0, "url", 0);
        if (image) {
            checksum = jsonGet(json, 0, "checksum", 0);
            version = jsonGet(json, 0, "version", 0);
            path = rGetFilePath("@state/update.bin");
            rInfo("ioto", "Device has updated firmware: %s", version);

            /*
                Download the update
             */
            if (download(image, path) == 0) {
                //  Validate
                if (!checkSum(path, checksum)) {
                    rError("provision", "Checksum does not match for update image %s: %s", path, checksum);
                } else {
                    //  Delayed application -- perhaps till off hours
                    apply = jsonGet(ioto->config, 0, "update.apply", "* * * * *");
                    delay = cronUntil(apply, rGetTime());
                    rStartEvent((REventProc) applyUpdate, sclone(path), delay);
                }
            }
            rFree(path);
        } else {
            rInfo("ioto", "Device has no pending updates for version: %s", ioto->version);
        }
        jsonFree(json);
    }

    date = rGetIsoDate(rGetTime());
    dbUpdate(ioto->db, "SyncState", DB_PROPS("lastUpdate", date), DB_PARAMS(.upsert = 1));

    delay = cronUntil(schedule, rGetTime() + period + jitter);
    rStartEvent((REventProc) ioUpdate, 0, delay);
    return json ? 1 : 0;
}

/*
    Apply the update by invoking the "scripts.update" script
    This may exit or reboot if instructed by the update script
    NOTE: This routine frees the path
 */
static void applyUpdate(char *path)
{
#if ME_UNIX_LIKE
    cchar *script;
    char  command[ME_MAX_FNAME + 256], *directive;
    int   status;

    //  Need hook/way for apps to prevent update
    rSignalSync("device:update", path);

    script = jsonGet(ioto->config, 0, "scripts.update", 0);
    if (script) {
        /*
            SECURITY Acceptable:: The command is configured by device developer and is deemed secure.
         */
        SFMT(command, "%s \"%s\"", script, path);
        status = rRun(command, &directive);
        rInfo("ioto", "Update returned status %d, directive: %s", status, directive);

        if (status != 0) {
            rError("update", "Update command failed: %s", directive);
        } else {
            if (smatch(directive, "exit\n")) {
                rGracefulStop();
            } else if (smatch(directive, "restart\n")) {
                rSetState(R_RESTART);
            }
        }
        rFree(directive);
    }
    unlink(path);
    rFree(path);
#else
    rSignalSync("device:update", path);
#endif
}

/*
    Download a software update
 */
static int download(cchar *url, cchar *path)
{
    Url   *up;
    ssize total;
    int   fd, nbytes, throttle;
    char  *buf;

    if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600)) < 0) {
        rError("provision", "Cannot open image temp file");
        return R_ERR_CANT_OPEN;
    }
    up = urlAlloc(0);

    // If throttling, the download timeout may need to be increased
    urlSetTimeout(up, svalue(jsonGet(ioto->config, 0, "timeouts.download", "4 hrs")) * TPS);

    if (urlStart(up, "GET", url) < 0 || urlGetStatus(up) != 200) {
        rError("update", "Cannot fetch %s\n%s", url, urlGetResponse(up));
        urlFree(up);
        return R_ERR_CANT_READ;
    }
    total = 0;
    throttle = (int) jsonGetNum(ioto->config, 0, "update.throttle", 0);
    if ((buf = rAlloc(ME_BUFSIZE)) == 0) {
        return R_ERR_CANT_ALLOCATE;
    }
    do {
        if ((nbytes = (int) urlRead(up, buf, ME_BUFSIZE)) < 0) {
            rError("update", "Cannot read response");
            urlFree(up);
            rFree(buf);
            return R_ERR_CANT_READ;
        }
        if (write(fd, buf, (size_t) nbytes) < nbytes) {
            rError("update", "Cannot save response");
            urlFree(up);
            rFree(buf);
            return R_ERR_CANT_WRITE;
        }
        total += nbytes;
        if (throttle) {
            throttle = min(throttle, 5 * TPS);
            if (throttle > 0) {
                rSleep(throttle);
            }
        }
    } while (nbytes > 0);
    rFree(buf);
    rInfo("ioto", "Downloaded %d bytes", (int) total);

    urlFree(up);
    close(fd);
    return 0;
}

static bool checkSum(cchar *path, cchar *checksum)
{
    char *hash;
    int  match;

    hash = cryptGetFileSha256(path);
    match = smatch(hash, checksum);
    rFree(hash);
    return match;
}

#else
void dummyUpdate(void)
{
}
#endif /* SERVICES_UPDATE */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
