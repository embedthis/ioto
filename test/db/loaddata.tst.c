/*
    loaddata.c.tst - Unit tests for bulk data loading, compaction and item printing

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"
#include    "json.h"

/************************************ Code ************************************/

static int countModel(Db *db, cchar *model)
{
    RList *items;
    int   count;

    items = dbFind(db, model, NULL, NULL);
    count = rGetListLength(items);
    rFreeList(items);
    return count;
}

/*
    Bulk loading creates every item in the file and is safe to repeat
 */
static void testLoadData()
{
    Db      *db;
    CDbItem *item;
    int     rc;

    db = dbOpen("./db/loaddata.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    rc = dbLoadData(db, "./seed.json5");
    teqi(rc, 0);
    teqi(countModel(db, "Port"), 3);
    teqi(countModel(db, "User"), 2);
    teqi(countModel(db, "Vlan"), 1);
    teqi(countModel(db, "Event"), 1);

    //  Values and schema defaults are both applied
    item = dbGet(db, "Port", DB_PROPS("id", "p2"), NULL);
    tnotnull(item);
    teqll(dbFieldNumber(item, "speed"), 100);
    tmatch(dbField(item, "duplex"), "full");

    //  A numeric date in the source file is converted to an ISO date
    item = dbGet(db, "Settings", NULL, NULL);
    tnotnull(item);
    tcontains(dbField(item, "timestamp"), "Z");
    teqll(dbFieldDate(item, "timestamp"), 1581636633399LL);

    //  Loading again is a no-op because the items already exist
    rc = dbLoadData(db, "./seed.json5");
    teqi(rc, 0);
    teqi(countModel(db, "Port"), 3);
    teqi(countModel(db, "User"), 2);

    dbClose(db);
}

/*
    A missing or invalid source file is reported rather than ignored
 */
static void testLoadDataErrors()
{
    Db  *db;
    int rc;

    db = dbOpen("./db/loaddata-errors.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    rc = dbLoadData(db, "./no-such-seed.json5");
    tltei(rc, 0);
    tnotnull(dbGetError(db));

    //  An item that violates the schema aborts the load
    rc = dbLoadData(db, "./seed-bad.json5");
    tltei(rc, 0);
    tnotnull(dbGetError(db));
    teqi(countModel(db, "Event"), 0);

    dbClose(db);
}

/*
    Compaction converts parsed items back to their string form without losing data
 */
static void testCompact()
{
    Db      *db;
    CDbItem *item;
    int     rc;

    db = dbOpen("./db/loaddata-compact.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);
    rc = dbLoadData(db, "./seed.json5");
    teqi(rc, 0);

    //  Force every item to be parsed, then compact them back to strings
    teqi(countModel(db, "Port"), 3);
    dbCompact(db);

    item = dbGet(db, "Port", DB_PROPS("id", "p1"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "name"), "tty01");

    //  Compacting twice is harmless
    dbCompact(db);
    teqi(countModel(db, "Port"), 3);

    //  The compacted database saves and reloads intact
    rc = dbSave(db, NULL);
    teqi(rc, 0);
    dbClose(db);

    db = dbOpen("./db/loaddata-compact.db", "./schema.json", 0);
    tnotnull(db);
    teqi(countModel(db, "Port"), 3);
    item = dbGet(db, "Port", DB_PROPS("id", "p1"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "name"), "tty01");
    dbClose(db);
}

/*
    Expiry works on compacted items, which hold a string rather than parsed json
 */
static void testCompactExpiry()
{
    Db      *db;
    CDbItem *item;
    char    idbuf[80];
    char    *expires;
    int     count, i;

    db = dbOpen("./db/loaddata-compact-expiry.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    expires = rGetIsoDate(rGetTime() - TPS);
    for (i = 0; i < 4; i++) {
        item = dbCreate(db, "Event", DB_PROPS(
            "message", "stale event",
            "subject", "Expiry",
            "source", "test",
            "severity", "info",
            "expires", expires,
            "id", SFMT(idbuf, "c-%d", i)
        ), NULL);
        tnotnull(item);
    }
    rFree(expires);

    //  Compaction discards the parsed json, so the sweep must parse the stored string
    dbCompact(db);

    count = dbRemoveExpired(db, 0);
    teqi(count, 4);
    teqi(countModel(db, "Event"), 0);

    dbClose(db);
}

/*
    The diagnostic printers accept items, lists, trees and null arguments
 */
static void testPrinters()
{
    Db      *db;
    CDbItem *item;
    RList   *items;
    Json    *props;
    int     rc;

    db = dbOpen("./db/loaddata-print.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);
    rc = dbLoadData(db, "./seed.json5");
    teqi(rc, 0);

    item = dbGet(db, "Port", DB_PROPS("id", "p0"), NULL);
    tnotnull(item);
    dbPrintItem(item);
    dbPrintItem(NULL);

    items = dbFind(db, "Port", NULL, NULL);
    dbPrintList(items);
    rFreeList(items);

    props = DB_PROPS("id", "p0");
    dbPrintProperties(props);
    jsonFree(props);

    dbPrint(db);
    dbPrintTree(db);

    dbClose(db);
}

/*
    The log parameter traces each operation without changing its result
 */
static void testLogParam()
{
    Db      *db;
    CDbItem *item;
    RList   *items;
    int     count;

    db = dbOpen("./db/loaddata-log.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "User", DB_PROPS(
        "id", "logged",
        "username", "logger",
        "email", "logger@test.com",
        "role", "user"
    ), DB_PARAMS(.log = 1));
    tnotnull(item);

    item = dbGet(db, "User", DB_PROPS("id", "logged"), DB_PARAMS(.log = 1));
    tnotnull(item);

    items = dbFind(db, "User", NULL, DB_PARAMS(.log = 1));
    teqi(rGetListLength(items), 1);
    rFreeList(items);

    count = dbRemove(db, "User", DB_PROPS("id", "logged"), DB_PARAMS(.log = 1));
    teqi(count, 1);

    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testLoadData();
    testLoadDataErrors();
    testCompact();
    testCompactExpiry();
    testPrinters();
    testLogParam();

    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
