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
    Ticks  maxAge, service;
    size_t maxSize;
    char   *path, *schema;
    int    flags;

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

#if SERVICES_CLOUD
    if (ioInitCloudDb() < 0) {
        return R_ERR_CANT_READ;
    }
#endif
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

    json = jsonAlloc();

#if SERVICES_CLOUD
    if (!ioSetCloudDevice(json)) {
        return;
    }
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
