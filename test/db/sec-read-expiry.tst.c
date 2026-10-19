/*
    sec-read-expiry.tst.c - Regression for db#10025

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"
#include    "json.h"

/************************************ Code ************************************/

static int calls;

/*
    When the expired item is removed, remove the live item too
 */
static void onRemove(void *arg, Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd,
                     int events)
{
    if (!smatch(cmd, "remove")) {
        return;
    }
    calls++;
    if (smatch(dbField(item, "id"), "old")) {
        dbRemove(db, "Thing", DB_PROPS("id", "live"), NULL);
    }
}

static void testReadDoesNotNotify(void)
{
    Db           *db;
    RList        *list;
    const DbItem *item;
    char         *expired;
    int          next;

    db = dbOpen("./db/sec-read-expiry.db", "./sec-regress.json5", DB_OPEN_RESET);
    tnotnull(db);

    expired = rGetIsoDate(rGetTime() - 10000);
    item = dbCreate(db, "Thing", DB_PROPS("id", "old", "name", "stale", "expires", expired), NULL);
    tnotnull(item);
    rFree(expired);
    item = dbCreate(db, "Thing", DB_PROPS("id", "live", "name", "fresh"), NULL);
    tnotnull(item);
    dbAddCallback(db, (DbCallbackProc) onRemove, "Thing", NULL, DB_ON_CHANGE);


    calls = 0;
    list = dbFind(db, "Thing", NULL, NULL);
    tnotnull(list);
    teqi(calls, 0);
    teqz(rGetListLength(list), 1);
    for (ITERATE_ITEMS(list, item, next)) {
        tmatch(dbField(item, "id"), "live");
        tmatch(dbField(item, "name"), "fresh");
    }
    rFreeList(list);

    //  The deferred removal runs from the event loop and notifies then
    rRunEvents();
    teqi(calls, 2);
    item = dbGet(db, "Thing", DB_PROPS("id", "live"), NULL);
    tnull(item);
    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:error:all", 0, 1);
    testReadDoesNotNotify();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
