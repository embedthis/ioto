/*
    sec-remove-lifetime.tst.c - Regression for db#10020 and db#10021

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"
#include    "json.h"

/************************************ Code ************************************/

static cchar *mode;
static int   depth;
static int   calls;

/*
    Clone IDs before removal callbacks can free their items.
 */
static void cascadeRemove(Db *db, DbItem *item)
{
    RList  *items;
    DbItem *other;
    RList  *ids;
    char   *id;
    int    i;

    ids = rAllocList(0, 0);
    items = dbFind(db, "Thing", NULL, NULL);
    for (ITERATE_ITEMS(items, other, i)) {
        if (other != item) {
            rAddItem(ids, sclone(dbField(other, "id")));
        }
    }
    rFreeList(items);
    for (ITERATE_ITEMS(ids, id, i)) {
        dbRemove(db, "Thing", DB_PROPS("id", id), DB_PARAMS(.delay = DB_INMEM));
        rFree(id);
    }
    rFreeList(ids);
}

static void onRemove(void *arg, Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd,
                     int events)
{
    RList *items;

    if (!smatch(cmd, "remove") || depth) {
        return;
    }
    calls++;
    depth++;
    if (smatch(mode, "cascade")) {
        cascadeRemove(db, item);
    } else {
        //  Read the same model, as a sync or summary callback would
        items = dbFind(db, "Thing", NULL, NULL);
        rFreeList(items);
    }
    depth--;
}

/*
    Create count items that have already expired.
 */
static Db *openWithExpired(cchar *path, int count)
{
    Db           *db;
    const DbItem *item;
    char         idbuf[32];
    char         *expired;
    int          i;

    db = dbOpen(path, "./sec-regress.json5", DB_OPEN_RESET);
    tnotnull(db);
    expired = rGetIsoDate(rGetTime() - 10000);
    for (i = 0; i < count; i++) {
        item = dbCreate(db, "Thing", DB_PROPS(
            "id", SFMT(idbuf, "e%d", i),
            "name", "stale",
            "expires", expired
        ), DB_PARAMS(.delay = DB_INMEM));
        tnotnull(item);
    }
    rFree(expired);
    dbAddCallback(db, (DbCallbackProc) onRemove, "Thing", NULL, DB_ON_CHANGE);
    return db;
}


static void testExpiryReentry(void)
{
    Db           *db;
    RList        *list;
    const DbItem *item;

    mode = "reentry";
    depth = calls = 0;
    db = openWithExpired("./db/sec-expire-reentry.db", 4);

    list = dbFind(db, "Thing", NULL, DB_PARAMS(.delay = DB_INMEM));
    rFreeList(list);
    rRunEvents();

    item = dbFindOne(db, "Thing", NULL, NULL);
    tnull(item);
    ttrue(calls >= 1);
    dbClose(db);
}

/*
    A callback that removes the other queued expired items by key.
 */
static void testExpiryCascade(void)
{
    Db           *db;
    RList        *list;
    const DbItem *item;

    mode = "cascade";
    depth = calls = 0;
    db = openWithExpired("./db/sec-expire-cascade.db", 4);

    list = dbFind(db, "Thing", NULL, DB_PARAMS(.delay = DB_INMEM));
    rFreeList(list);
    rRunEvents();

    item = dbFindOne(db, "Thing", NULL, NULL);
    tnull(item);
    ttrue(calls >= 1);
    dbClose(db);
}


static void testExpiryService(void)
{
    Db           *db;
    const DbItem *item;
    int          count;

    mode = "service";
    depth = calls = 0;
    db = openWithExpired("./db/sec-expire-service.db", 4);

    count = dbRemoveExpired(db, 1);
    ttrue(count >= 1);

    item = dbFindOne(db, "Thing", NULL, NULL);
    tnull(item);
    dbClose(db);
}

/*
    dbRemove must survive a remove callback that removes the rest of the matching items.
 */
static void testRemoveCascade(void)
{
    Db           *db;
    const DbItem *item;
    char         idbuf[32];
    int          i, count;

    mode = "cascade";
    depth = calls = 0;

    db = dbOpen("./db/sec-remove-cascade.db", "./sec-regress.json5", DB_OPEN_RESET);
    tnotnull(db);
    for (i = 0; i < 6; i++) {
        item = dbCreate(db, "Thing", DB_PROPS("id", SFMT(idbuf, "r%d", i), "name", "n"),
                        DB_PARAMS(.delay = DB_INMEM));
        tnotnull(item);
    }
    dbAddCallback(db, (DbCallbackProc) onRemove, "Thing", NULL, DB_ON_CHANGE);

    //  The first removal's callback removes the rest
    count = dbRemove(db, "Thing", DB_JSON("{}"), DB_PARAMS(.limit = 100, .delay = DB_INMEM));
    ttrue(count >= 1);
    teqi(calls, 1);

    item = dbFindOne(db, "Thing", NULL, NULL);
    tnull(item);
    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all", 0, 1);
    testExpiryReentry();
    testExpiryCascade();
    testExpiryService();
    testRemoveCascade();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
