/*
    sync.c - Sync database data to the cloud

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

/*********************************** Locals ************************************/
#if SERVICES_SYNC

/*
    Delay waiting for an acknowledgement after sending a sync message to the cloud
 */
#define SYNC_DELAY (5 * TPS)

/*
    Sync Log On-Disk Format

    The sync log stores database changes to disk for crash recovery. Each log entry contains:
    - Total size (uint32_t): Sum of all string lengths + 4 null terminators
    - Command string (uint32_t length + data): Database command ("create", "update", "remove")
    - Data string (uint32_t length + data): JSON representation of item data
    - Key string (uint32_t length + data): Database item key
    - Updated timestamp (uint32_t length + data): ISO 8601 timestamp string

    All 32-bit size fields are stored in host byte order.
    String lengths include the trailing null terminator.

    Note: Sync logs are local-only files and do not move between systems.
          No cross-platform compatibility or endianness conversion is required.
 */
typedef uint32_t SyncSize;

#define SYNC_MAX_SIZE 0x7FFFFFFF   // Maximum safe size for any field

/**
    Database sync change record. One allocated for each mutation to the database.
    Changes implement a buffer cache for database changes. The ioto.json5 provides a maxSyncSize.
    For performance, change items are be buffered to aggregate multiple mutations into
    a single sync message to the cloud.
 */
typedef struct Change {
    DbItem *item;
    char *cmd;
    char *key;          //  Local database item key
    char *data;
    char *updated;      //  When updated
    Time due;           //  When change is due to be sent
    int seq;
} Change;

/*
    Sequence number for change sets sent to the cloud
 */
static int nextSeq = 1;

/************************************ Forwards *********************************/

static Change *allocChange(cchar *cmd, cchar *key, char *data, cchar *updated, Ticks due);
static void applySyncLog(void);
static void cleanSyncChanges(Json *json);
static void dbCallback(void *arg, Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd, int events);
static void deviceCommand(void *arg, struct Db *db, struct DbModel *model, struct DbItem *item,
                          struct DbParams *params, cchar *cmd, int event);
static void freeChange(Change *change);
static void initSyncConnection(void);
static void logChange(Change *change);
static void processDeviceCommand(DbItem *item);
static cchar *readBlock(FILE *fp, RBuf *buf);
static size_t readSize(FILE *fp);
static void receiveSync(const MqttRecv *rp);
static void recreateSyncLog(void);
static void scheduleSync(Change *change);
static void syncItem(DbModel *model, CDbItem *item, DbParams *params, cchar *cmd, bool guarantee);
static void updateChange(Change *change, cchar *cmd, char *data, cchar *updated, Ticks due);
static int writeBlock(cchar *buf);
static int writeSize(int len);

/************************************* Code ***********************************/

PUBLIC int ioInitSync(void)
{
    cchar *lastSync;

    /*
        SECURITY Acceptable: - the use of rand here is acceptable as it is only used for the sync sequence number and is
           not a security risk.
     */
    nextSeq = rand();
    ioto->syncDue = MAXINT64;
    ioto->syncHash = rAllocHash(0, 0);
    ioto->maxSyncSize = svaluei(jsonGet(ioto->config, 0, "database.maxSyncSize", "1k"));
    if ((lastSync = dbGetField(ioto->db, "SyncState", "lastSync", NULL, DB_PARAMS())) != NULL) {
        ioto->lastSync = sclone(lastSync);
    } else {
        ioto->lastSync = rGetIsoDate(0);
    }
    recreateSyncLog();
    dbAddCallback(ioto->db, (DbCallbackProc) dbCallback, NULL, NULL, DB_ON_COMMIT | DB_ON_FREE);
    rWatch("mqtt:connected", (RWatchProc) initSyncConnection, 0);
    return 0;
}

PUBLIC void ioTermSync(void)
{
    RName  *np;
    Change *change;
    char   path[ME_MAX_FNAME];

    if (ioto->db) {
        dbUpdate(ioto->db, "SyncState", DB_PROPS("lastSync", ioto->lastSync), DB_PARAMS(.bypass = 1));
    }
    for (ITERATE_NAMES(ioto->syncHash, np)) {
        change = np->value;
        freeChange(change);
    }
    rFreeHash(ioto->syncHash);
    ioto->syncHash = 0;

    /*
        The sync log is used to recover from crashes only
        As we're doing an orderly shutdown, can remove here
     */
    if (ioto->syncLog) {
        fclose(ioto->syncLog);
        ioto->syncLog = 0;
        unlink(SFMT(path, "%s.sync", ioto->db->path));
    }
}

/*
    Force a sync of ALL syncing items in the database
    This is called after provisioning to sync the entire database for the first time.
    Users can call this if necessary.
    If guarantee is true, then reliably save the change record until the cloud acknowledges receipt.
 */
PUBLIC void ioSyncUp(Time when, bool guarantee)
{
    RbNode  *node;
    DbModel *model;
    DbItem  *item;
    Time    updated;

    dbRemoveExpired(ioto->db, 1);

    for (node = rbFirst(ioto->db->primary); node; node = rbNext(ioto->db->primary, node)) {
        item = node->data;
        model = dbGetItemModel(ioto->db, item);
        if (model && model->sync) {
            if (when > 0) {
                updated = rParseIsoDate(dbField(item, "updated"));
                //  If updated at the same time as when, then send update
                if (updated < when) {
                    continue;
                }
            }
            syncItem(model, item, NULL, "update", guarantee);
        }
    }
    ioFlushSync(0);
}

/*
    Send a sync down message to the cloud
    When is set to retrieve items updated after this time. If when is negative, the call will items updated
    since the last sync.
 */
PUBLIC void ioSyncDown(Time when)
{
    char msg[160], *timestamp;

    //  Send SyncDown message
    if (when >= 0) {
        timestamp = rGetIsoDate(when);
        SFMT(msg, "{\"timestamp\":\"%s\"}", timestamp);
        rFree(timestamp);
    } else {
        SFMT(msg, "{\"timestamp\":\"%s\"}", ioto->lastSync);
    }
    mqttPublish(ioto->mqtt, msg, 0, 1, MQTT_WAIT_NONE, "$aws/rules/IotoDevice/ioto/service/%s/db/syncDown", ioto->id);
}

PUBLIC void ioSync(Time when, bool guarantee)
{
    ioSyncUp(when, guarantee);
    ioSyncDown(when);
}

/*
    Send sync changes to the cloud. Process the sync log and re-create the change hash.
    The sync log contains a fail-safe record of local database changes that must be replicated to
    the cloud. It is applied on Ioto restart after an unexpected exit. It is erased after processing.
 */
static void applySyncLog(void)
{
    FILE   *fp;
    RBuf   *buf;
    Change *change;
    Ticks  now;
    char   path[ME_MAX_FNAME];
    cchar  *cmd, *data, *key, *updated;
    size_t bufsize;

    if (ioto->nosave) return;

    if (ioto->syncLog) {
        fclose(ioto->syncLog);
        ioto->syncLog = 0;
    }
    if ((fp = fopen(SFMT(path, "%s.sync", ioto->db->path), "r+")) != NULL) {
        ioto->syncLog = fp;
        now = rGetTicks();

        buf = rAllocBuf(ME_BUFSIZE);
        while ((bufsize = readSize(fp)) > 0) {
            if (rGrowBuf(buf, bufsize) < 0) {
                break;
            }
            cmd = readBlock(fp, buf);
            data = readBlock(fp, buf);
            key = readBlock(fp, buf);
            updated = readBlock(fp, buf);
            if (!cmd || !data || !key || !updated) {
                //  Log is corrupt
                recreateSyncLog();
                break;
            }
            if ((change = rLookupName(ioto->syncHash, key)) != NULL) {
                updateChange(change, cmd, sclone(data), updated, now);
            } else {
                change = allocChange(cmd, key, sclone(data), updated, now);
                rAddName(ioto->syncHash, change->key, change, 0);
            }
            rFlushBuf(buf);
        }
        rFreeBuf(buf);
    }
    if (rGetHashLength(ioto->syncHash) > 0) {
        ioFlushSync(0);
    }
}

static Change *allocChange(cchar *cmd, cchar *key, char *data, cchar *updated, Ticks now)
{
    Change *change;

    if ((change = rAllocType(Change)) == 0) {
        return 0;
    }
    change->cmd = sclone(cmd);
    change->key = sclone(key);
    change->updated = sclone(updated);
    change->data = data;
    change->due = now;
    return change;
}

static void freeChange(Change *change)
{
    rRemoveName(ioto->syncHash, change->key);
    rFree(change->cmd);
    rFree(change->data);
    rFree(change->key);
    rFree(change->updated);
    rFree(change);
}

static void updateChange(Change *change, cchar *cmd, char *data, cchar *updated, Ticks now)
{
    rFree(change->cmd);
    change->cmd = sclone(cmd);
    rFree(change->data);
    change->data = data;
    rFree(change->updated);
    change->updated = sclone(updated);
    change->due = now;
}

/*
    Read a 32-bit size field from the sync log.
    Size is stored in host byte order (local files only).

    @param fp File pointer to read from
    @return Size value, or 0 on EOF/error
 */
static size_t readSize(FILE *fp)
{
    uint32_t size32;

    if (fread(&size32, sizeof(uint32_t), 1, fp) != 1) {
        //  EOF
        return 0;
    }
    return (size_t) size32;
}

/*
    Read a string block from the sync log.
    Format: [32-bit length][string data including null terminator]

    @param fp File pointer to read from
    @param buf Buffer to read into
    @return Pointer to string data, or NULL on error
 */
static cchar *readBlock(FILE *fp, RBuf *buf)
{
    cchar    *result;
    uint32_t size32;
    size_t   len;

    if (!buf) {
        return NULL;
    }
    //  Read 32-bit length field
    if (fread(&size32, sizeof(uint32_t), 1, fp) != 1) {
        rError("sync", "Corrupt sync log - cannot read size");
        return 0;
    }
    len = (size_t) size32;

    if (len > DB_MAX_ITEM) {
        rError("sync", "Corrupt sync log - block too large: %zu", len);
        return 0;
    }
    //  Read string data (includes null terminator)
    if (fread(rGetBufStart(buf), len, 1, fp) != 1) {
        rError("sync", "Corrupt sync log - cannot read data");
        return 0;
    }
    result = rGetBufStart(buf);
    rAdjustBufEnd(buf, (ssize) len);
    rAdjustBufStart(buf, (ssize) rGetBufLength(buf));
    rAddNullToBuf(buf);
    return result;
}

/*
    Database trigger invoked for local database changes
 */
static void dbCallback(void *arg, Db *db, DbModel *model, DbItem *item, DbParams *params,
                       cchar *cmd, int events)
{
    Change *change;

    if (events & DB_ON_FREE) {
        if ((change = rLookupName(ioto->syncHash, item->key)) != 0) {
            rRemoveName(ioto->syncHash, item->key);
            freeChange(change);
        }
    } else if (events & DB_ON_COMMIT) {
        //  Bypass is set for items that should not be sent to the cloud
        if (model->sync && !(params && params->bypass)) {
            syncItem(model, item, params, cmd, 1);
        }
    }
}

/*
    Synchronize state to the cloud and local disk.
    If guarantee is true, then reliably save the change record until the cloud acknowledges receipt.
 */
static void syncItem(DbModel *model, CDbItem *item, DbParams *params, cchar *cmd, bool guarantee)
{
    Change *change;
    cchar  *updated;
    char   *data;
    Ticks  now;

    if (!model || !model->sync || (params && params->bypass)) {
        /*
            Don't prep a change record to sync to the cloud if the model does not want it, or
            if this update came from a cloud update (i.e. stop infinite looping updates).
         */
        return;
    }
    /*
        Overwrite prior buffered change records if the item has changed.
        If change->seq is set, the change has been sent, but not acknowledged, so cannot overwrite.
        The prior ack will just be ignored and this change will have a new seq.
        FUTURE: could compare the item and only sync if changed (excluding the updated timestamp)
     */
    if ((change = rLookupName(ioto->syncHash, item->key)) == 0 || change->seq) {
        //  Item.json takes precedence over item.value
        data = item->json ? jsonToString(item->json, 0, 0, JSON_JSON) : sclone(item->value);
        updated = dbField(item, "updated");
        now = rGetTicks();

        if (change) {
            updateChange(change, cmd, data, updated, now);
        } else {
            change = allocChange(cmd, item->key, data, updated, now);
            rAddName(ioto->syncHash, change->key, change, 0);
        }
    }
    if (guarantee) {
        logChange(change);
    }
    if (ioto->mqtt) {
        scheduleSync(change);
    }
    rSignalSync("db:change", change);
}

/*
    Fail-safe sync. Write to the sync log and replay after a crash.
 */
static void logChange(Change *change)
{
    int len;

    if (!ioto->syncLog || ioto->nosave) {
        return;
    }
    /*
        Calculate total size for sync log entry.
        Total = sum of string lengths + 4 null terminators (one per string).
        Safe to cast to int since DB_MAX_ITEM limit ensures no overflow.
     */
    len = (int) (slen(change->cmd) + slen(change->data) + slen(change->key) + slen(change->updated) + 4);

    writeSize(len);
    writeBlock(change->cmd);
    writeBlock(change->data);
    writeBlock(change->key);
    writeBlock(change->updated);
    fflush(ioto->syncLog);
    rFlushFile(fileno(ioto->syncLog));

    ioto->syncSize += len;
}

/*
    Write a string block to the sync log.
    Format: [32-bit length][string data including null terminator]

    @param buf String to write (must include null terminator in length calculation)
    @return 0 on success, error code otherwise
 */
static int writeBlock(cchar *buf)
{
    ssize    len;
    uint32_t size32;

    len = (ssize) slen(buf) + 1;  // +1 for null terminator

    if (len < 0 || len > SYNC_MAX_SIZE) {
        rError("sync", "String too large for sync log: %lld bytes", (long long) len);
        return R_ERR_BAD_ARGS;
    }
    size32 = (uint32_t) len;

    if (fwrite(&size32, sizeof(uint32_t), 1, ioto->syncLog) != 1) {
        rError("sync", "Cannot write to sync log");
        return R_ERR_CANT_WRITE;
    }
    if (fwrite(buf, (size_t) len, 1, ioto->syncLog) != 1) {
        rError("sync", "Cannot write to sync log");
        return R_ERR_CANT_WRITE;
    }
    return 0;
}

/*
    Write a 32-bit size field to the sync log.
    Size is written in host byte order (local files only).

    @param len The size value to write (must be <= SYNC_MAX_SIZE)
    @return 0 on success, error code otherwise
 */
static int writeSize(int len)
{
    uint32_t size32;

    if (len < 0 || len > SYNC_MAX_SIZE) {
        rError("sync", "Invalid sync log size: %d", len);
        return R_ERR_BAD_ARGS;
    }
    size32 = (uint32_t) len;
    if (fwrite(&size32, sizeof(uint32_t), 1, ioto->syncLog) != 1) {
        rError("sync", "Cannot write to sync log");
        return R_ERR_CANT_WRITE;
    }
    return 0;
}

/*
    Schedule sync when sufficient changes or a change due
 */
static void scheduleSync(Change *change)
{
    Ticks delay, now;

    if (!ioto->connected) {
        rWatch("mqtt:connected", (RWatchProc) scheduleSync, change);
        return;
    }
    /*
        Changes come via db callback and set change->due to now
        Sync retransmits set change->due +5 secs
     */
    now = rGetTicks();
    if (change->due < ioto->syncDue) {
        ioto->syncDue = change->due;
        if (ioto->syncEvent) {
            rStopEvent(ioto->syncEvent);
            ioto->syncEvent = 0;
        }
    }
    if (ioto->syncSize >= ioto->maxSyncSize) {
        ioFlushSync(0);

    } else if (rGetHashLength(ioto->syncHash) > 0 && !ioto->syncEvent) {
        delay = (ioto->syncDue > now) ? ioto->syncDue - now : 0;
        ioto->syncDue = now + delay;
        ioto->syncEvent = rStartEvent((REventProc) ioFlushSync, 0, delay);
    }
}

/*
    Publish changes to cloud
 */
PUBLIC void ioFlushSync(bool force)
{
    RName  *np;
    RBuf   *buf;
    Ticks  now, nextDue;
    Change *change;
    size_t len;
    int    count, pending, seq;

    if (!ioto->connected) {
        return;
    }
    now = rGetTicks();
    buf = 0;
    count = 0;
    pending = 0;
    nextDue = now + (60 * TPS);

    if (rGetHashLength(ioto->syncHash) > 0) {
        rTrace("sync", "Applying sync log with %d changes", rGetHashLength(ioto->syncHash));
    }
    for (ITERATE_NAME_DATA(ioto->syncHash, np, change)) {
        if (force || change->due <= now) {
            if (buf == 0) {
                buf = rAllocBuf(ME_BUFSIZE);
                seq = nextSeq++;
                rPutToBuf(buf, "{\"seq\":%d,\"changes\":[", seq);
            }
            len = slen(change->cmd) + slen(change->key) + slen(change->data) + 27;
            if (rGetBufLength(buf) + len > (IO_MESSAGE_SIZE - 1024)) {
                nextDue = now;
                break;
            }
            rPutToBuf(buf, "{\"cmd\":\"%s\",\"key\":\"%s\",\"item\":%s},", change->cmd, change->key, change->data);
            change->seq = seq;
            //  Set the delay to +5 secs to give time for sync to be received before retransmit
            change->due += SYNC_DELAY;
            count++;
        } else {
            pending++;
            rDebug("sync", "Change due in %d msecs, %s", (int) (change->due - now), change->key);
        }
        nextDue = min(nextDue, change->due);
    }
    ioto->syncEvent = 0;
    ioto->syncSize = 0;
    ioto->syncDue = nextDue;

    if (!buf) {
        return;
    }
    rAdjustBufEnd(buf, -1);
    rPutStringToBuf(buf, "]}");

    //  Pending changes are buffered and not yet due to be sent
    rTrace("sync", "Sending %d sync changes to the cloud, %d changes pending", count, pending);

    mqttPublish(ioto->mqtt, rBufToString(buf), 0, 1, force ? MQTT_WAIT_ACK : MQTT_WAIT_NONE,
                "$aws/rules/IotoDevice/ioto/service/%s/db/syncToDynamo", ioto->id);
    rFreeBuf(buf);

#if KEEP
    if (force) {
        Ticks deadline = rGetTicks() + 5 * TPS;
        while (rGetTicks() < deadline) {
            if ((count = mqttMsgsToSend(ioto->mqtt)) == 0) {
                break;
            }
            rSleep(20);
        }
    }
#endif
}

/*
    Remove changes that have been replicated to the cloud.
    Changes are acknowledged by sequence number.
 */
static void cleanSyncChanges(Json *json)
{
    Change   *change;
    JsonNode *key, *keys;
    cchar    *updated;
    ssize    count;
    int      seq;

    keys = jsonGetNode(json, 0, "keys");
    seq = jsonGetInt(json, 0, "seq", 0);
    updated = jsonGet(json, 0, "updated", 0);
    if (!keys) {
        return;
    }
    count = rGetHashLength(ioto->syncHash);

    for (ITERATE_JSON(json, keys, key, kid)) {
        change = rLookupName(ioto->syncHash, key->value);
        if (change) {
            if (change->seq == seq) {
                if (scmp(change->updated, ioto->lastSync) > 0) {
                    rFree(ioto->lastSync);
                    //  Prefer cloud-side updated time
                    ioto->lastSync = sclone(updated ? updated : change->updated);
                    //  OPTIMIZE
                    dbUpdate(ioto->db, "SyncState", DB_PROPS("lastSync", ioto->lastSync), DB_PARAMS(.bypass = 1));
                }
                freeChange(change);
            }
        }
    }
    rDebug("sync", "After syncing %ld changes %d changes pending", count, rGetHashLength(ioto->syncHash));
    if (count && rGetHashLength(ioto->syncHash) == 0) {
        //  OPTIMIZE
        recreateSyncLog();
    }
    rSignal("db:sync:done");
}

static void recreateSyncLog(void)
{
    char path[ME_MAX_FNAME];

    if (ioto->nosave) return;

    SFMT(path, "%s.sync", ioto->db->path);

    if (ioto->syncLog) {
        fclose(ioto->syncLog);
        ioto->syncLog = 0;
    }
    if ((ioto->syncLog = fopen(path, "w")) == NULL) {
        rError("sync", "Cannot open sync log '%s'", path);
    }
}

/*
    When connected to the cloud, subscribe for incoming sync changes.
    Also fetch database updates made in the cloud since the last sync down from the cloud.
    And then send pending changes to the cloud.
 */
static void initSyncConnection(void)
{
    Time timestamp;

    if (!ioto->syncService) return;

    timestamp = rParseIsoDate(ioto->lastSync);

    dbAddCallback(ioto->db,  (DbCallbackProc) deviceCommand, "Command", NULL, DB_ON_CHANGE);

    //  The "+" matches the sync command: INSERT, REMOVE, UPSERT, SYNC (responses)
    mqttSubscribe(ioto->mqtt, receiveSync, 1, MQTT_WAIT_NONE, "ioto/device/%s/sync/+", ioto->id);
    mqttSubscribe(ioto->mqtt, receiveSync, 1, MQTT_WAIT_NONE, "ioto/account/all/sync/+");
    mqttSubscribe(ioto->mqtt, receiveSync, 1, MQTT_WAIT_NONE, "ioto/account/%s/#", ioto->account);

    /*
        Sync up. Apply prior changes that have been made locally but not yet applied to the cloud
     */
    applySyncLog();

    //  Sync from cloud to device - non-blocking
    if (!ioto->cmdSync) {
        //  Sync down all changes made since the last sync down (while we were offline)
        ioSyncDown(timestamp);
    } else if (smatch(ioto->cmdSync, "up")) {
        ioSyncUp(0, 1);
    } else if (smatch(ioto->cmdSync, "down")) {
        ioSyncDown(0);
    } else if (smatch(ioto->cmdSync, "both")) {
        ioSyncUp(0, 1);
        ioSyncDown(0);
    }
}

/*
    Receive sync down responses
 */
static void receiveSync(const MqttRecv *rp)
{
    Db      *db;
    CDbItem *prior;
    cchar   *modelName, *msg, *priorUpdated, *sk, *updated;
    char    sigbuf[80], *str;
    DbModel *model;
    Json    *json;
    bool    stale;

    db = ioto->db;
    msg = rp->data;

    if ((json = jsonParse(msg, 0)) == 0) {
        rError("sync", "Cannot parse sync message: %s for %s", msg, rp->topic);
        return;
    }
    if (sends(rp->topic, "SYNC")) {
        //  Response for a syncItem to DynamoDB
        rTrace("sync", "Received sync ack %s", rp->topic);
        cleanSyncChanges(json);

    } else if (sends(rp->topic, "SYNCDOWN")) {
        //  Response for syncdown
        rDebug("sync", "Received syncdown ack");
        if ((updated = jsonGet(json, 0, "updated", 0)) != NULL && scmp(updated, ioto->lastSync) > 0) {
            rFree(ioto->lastSync);
            ioto->lastSync = sclone(updated);
            dbUpdate(ioto->db, "SyncState", DB_PROPS("lastSync", ioto->lastSync), DB_PARAMS(.bypass = 1));
        }
        if (!ioto->cloudReady) {
            /*
                Signal post-connect syncdown complete. May get multiple syncdown responses.
             */
            ioto->cloudReady = 1;
            rSignal("cloud:ready");
        }

    } else {
        sk = jsonGet(json, 0, "sk", "");
        prior = dbGet(db, NULL, DB_PROPS("sk", sk), DB_PARAMS());
        stale = 0;
        if (prior) {
            updated = jsonGet(json, 0, "updated", 0);
            priorUpdated = dbField(prior, "updated");
            if (updated && priorUpdated && scmp(updated, priorUpdated) < 0) {
                stale = 1;
            }
        }
        if (stale) {
            rTrace("sync", "Discard stale sync update and send item back to peer");
            model = dbGetItemModel(ioto->db, prior);
            syncItem(model, prior, NULL, "update", 1);

        } else {
            if (rEmitLog("debug", "sync")) {
                rTrace("sync", "Received sync response %s: %s", rp->topic, msg);
                str = jsonToString(json, 0, 0, JSON_HUMAN);
                rDebug("sync", "Response %s", str);
                rFree(str);
            } else if (rEmitLog("trace", "sync")) {
                rTrace("sync", "Received sync response %s", rp->topic);
            }
            if (sends(rp->topic, "REMOVE")) {
                jsonRemove(json, 0, "updated");
                dbRemove(db, NULL, json, DB_PARAMS(.bypass = 1));

            } else if (sends(rp->topic, "INSERT")) {
                dbCreate(db, NULL, json, DB_PARAMS(.bypass = 1));

            } else if (sends(rp->topic, "UPSERT") || sends(rp->topic, "MODIFY")) {
                dbUpdate(db, NULL, json, DB_PARAMS(.bypass = 1, .upsert = 1));

            } else {
                rError("db", "Bad sync topic %s", rp->topic);
            }
        }
        modelName = jsonGet(json, 0, dbType(ioto->db), 0);
        rSignalSync(SFMT(sigbuf, "db:sync:%s", modelName), json);
    }
    jsonFree(json);
}


/*
    Watch updates to the command table
 */
static void deviceCommand(void *arg, struct Db *db, struct DbModel *model,
                          struct DbItem *item, struct DbParams *params, cchar *cmd, int event)
{
    if (event & DB_ON_CHANGE) {
        if (smatch(cmd, "create") || smatch(cmd, "upsert") || smatch(cmd, "update")) {
            processDeviceCommand(item);
        }
    }
}


/*
    Act on standard device commands
 */
static void processDeviceCommand(DbItem *item)
{
    cchar *cmd;
    char  *name;

    cmd = dbField(item, "command");

    rInfo("ioto", "Device command \"%s\"\nData: %s", cmd, dbString(item, JSON_HUMAN));

    if (smatch(cmd, "reboot")) {
        rSetState(R_RESTART);
#if SERVICES_PROVISION
    } else if (smatch(cmd, "release") || smatch(cmd, "reprovision")) {
        ioDeprovision();
#endif
#if SERVICES_UPDATE
    } else if (smatch(cmd, "update")) {
        ioUpdate();
#endif
    } else {
        name = sfmt("device:command:%s", cmd);
        rSignalSync(name, item);
        rFree(name);
    }
}

#else
void dummySync(void)
{
}
#endif /* SERVICES_SYNC */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
