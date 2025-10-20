/*
    database.c - Embedded database

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

/********************************** Forwards **********************************/

#if SERVICES_DATABASE

static void dbService(void);

/************************************* Code ***********************************/

PUBLIC int ioInitDb(void)
{
    RList  *devices;
    DbItem *device;
    Ticks  maxAge, service;
    size_t maxSize;
    cchar  *id;
    char   *path, *schema;
    int    flags, index;

    schema = rGetFilePath(jsonGet(ioto->config, 0, "database.schema", "@config/schema.json5"));
    path = rGetFilePath(jsonGet(ioto->config, 0, "database.path", "@db/device.db"));

    flags = ioto->nosave ? DB_READ_ONLY : 0;
    if ((ioto->db = dbOpen(path, schema, flags)) == 0) {
        rError("database", "Cannot open database %s or schema %s", path, schema);
        rFree(path);
        rFree(schema);
        return R_ERR_CANT_OPEN;
    }
    rFree(path);
    rFree(schema);

    maxAge = svalue(jsonGet(ioto->config, 0, "database.maxJournalAge", "1min")) * TPS;
    service = svalue(jsonGet(ioto->config, 0, "database.service", "1hour")) * TPS;
    maxSize = (size_t) svalue(jsonGet(ioto->config, 0, "database.maxJournalSize", "1mb"));
    dbSetJournalParams(ioto->db, maxAge, maxSize);

    dbAddContext(ioto->db, "deviceId", ioto->id);
#if SERVICES_CLOUD
    if (ioto->account) {
        dbAddContext(ioto->db, "accountId", ioto->account);
    }
#endif
#if SERVICES_SYNC
    if (dbGet(ioto->db, "SyncState", NULL, DB_PARAMS()) == 0) {
        dbCreate(ioto->db, "SyncState", DB_PROPS("lastSync", "0", "lastUpdate", "0"), DB_PARAMS());
    }
    if (ioto->syncService && (ioInitSync() < 0)) {
        return R_ERR_CANT_READ;
    }
#endif
    /*
        When testing, can have multiple devices in the database. Remove all but the current device.
     */
    devices = dbFind(ioto->db, "Device", NULL, DB_PARAMS());
    for (ITERATE_ITEMS(devices, device, index)) {
        id = dbField(device, "id");
        if (!smatch(id, ioto->id)) {
            dbRemove(ioto->db, "Device", DB_PROPS("id", id), DB_PARAMS());
        }
    }
    rFreeList(devices);

    /*
        Update Device entry. Delay if not yet provisioned.
     */
#if SERVICES_CLOUD
    if (!ioto->account) {
        rWatch("device:provisioned", (RWatchProc) ioUpdateDevice, 0);
    } else
#endif
    if (!dbGet(ioto->db, "Device", DB_PROPS("id", ioto->id), DB_PARAMS())) {
        ioUpdateDevice();
    }
    if (service) {
        rStartEvent((RFiberProc) dbService, 0, service);
    }
    return 0;
}

PUBLIC void ioTermDb(void)
{
    if (ioto->db) {
        if (ioto->nosave) {
            dbSave(ioto->db, NULL);
        }
        dbClose(ioto->db);
        ioto->db = 0;
    }
}

PUBLIC void ioRestartDb(void)
{
    ioTermDb();
    ioInitDb();
}

/*
    Perform periodic database maintenance. Remove TTL expired items.
 */
static void dbService(void)
{
    Ticks frequency;

    dbRemoveExpired(ioto->db, 1);
    frequency = svalue(jsonGet(ioto->config, 0, "database.service", "1day")) * TPS;
    rStartEvent((RFiberProc) dbService, 0, frequency);
}

/*
    Update the ioto-device Device entry with properties from device.json
 */
PUBLIC void ioUpdateDevice(void)
{
    Json *json;

    assert(ioto->id);

    json = jsonAlloc();
    jsonSet(json, 0, "id", ioto->id, JSON_STRING);
#if SERVICES_CLOUD
    if (!ioto->account) {
        //  Update later when we have an account ID
        return;
    }
    jsonSet(json, 0, "accountId", ioto->account, JSON_STRING);
#endif
    jsonSet(json, 0, "description", jsonGet(ioto->config, 0, "device.description", 0), JSON_STRING);
    jsonSet(json, 0, "model", jsonGet(ioto->config, 0, "device.model", 0), JSON_STRING);
    jsonSet(json, 0, "name", jsonGet(ioto->config, 0, "device.name", 0), JSON_STRING);
    jsonSet(json, 0, "product", jsonGet(ioto->config, 0, "device.product", 0), JSON_STRING);

    if (dbCreate(ioto->db, "Device", json, DB_PARAMS(.upsert = 1)) == 0) {
        rError("sync", "Cannot update device item in database: %s", dbGetError(ioto->db));
    }
    jsonFree(json);
}

#else
void dummyDatabase()
{
}
#endif /* SERVICES_DATABASE */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
