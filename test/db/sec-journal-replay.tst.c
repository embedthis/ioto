/*
    sec-journal-replay.tst.c - Journal replay of many records and of removes

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/

/*
    A journal of many records must replay.
 */
static void testManyRecords(void)
{
    Db           *db, *recovered;
    const DbItem *item;
    RList        *list;
    char         idbuf[32];
    int          i;

    db = dbOpen("./db/sec-journal-many.db", "./sec-regress.json5", DB_OPEN_RESET);
    tnotnull(db);
    for (i = 0; i < 40; i++) {
        item = dbCreate(db, "Thing", DB_PROPS("id", SFMT(idbuf, "k%d", i), "name", "n"),
                        DB_PARAMS(.delay = DB_NODELAY));
        tnotnull(item);
    }
    //  Unclean shutdown: the first handle is never saved, so the journal holds all 40 records
    recovered = dbOpen("./db/sec-journal-many.db", "./sec-regress.json5", DB_READ_ONLY);
    tnotnull(recovered);
    tmatch(dbGetError(recovered), NULL);

    list = dbFind(recovered, "Thing", NULL, NULL);
    tnotnull(list);
    teqi(rGetListLength(list), 40);
    rFreeList(list);
    dbClose(recovered);
    dbClose(db);
}

/*
    An item removed before an unclean shutdown must stay removed after replay.
 */
static void testRemoveReplay(void)
{
    Db           *db, *recovered;
    const DbItem *item;
    int          count;

    db = dbOpen("./db/sec-journal-remove.db", "./sec-regress.json5", DB_OPEN_RESET);
    tnotnull(db);
    item = dbCreate(db, "Thing", DB_PROPS("id", "victim", "name", "n1"),
                    DB_PARAMS(.delay = DB_NODELAY));
    tnotnull(item);
    rSleep(20);

    count = dbRemove(db, "Thing", DB_PROPS("id", "victim"), DB_PARAMS(.delay = DB_NODELAY));
    teqi(count, 1);
    item = dbGet(db, "Thing", DB_PROPS("id", "victim"), NULL);
    tnull(item);
    rSleep(20);

    //  Unclean shutdown: replay the create and then the remove
    recovered = dbOpen("./db/sec-journal-remove.db", "./sec-regress.json5", DB_READ_ONLY);
    tnotnull(recovered);
    item = dbGet(recovered, "Thing", DB_PROPS("id", "victim"), NULL);
    tnull(item);
    dbClose(recovered);
    dbClose(db);
}

/*
    Replay must preserve the stored timestamps rather than stamping the replay time.
 */
static void testReplayTimestamps(void)
{
    Db           *db, *recovered;
    const DbItem *item;
    char         *created, *updated;

    db = dbOpen("./db/sec-journal-stamps.db", "./sec-regress.json5", DB_OPEN_RESET);
    tnotnull(db);
    item = dbCreate(db, "Thing", DB_PROPS("id", "stamped", "name", "n1"),
                    DB_PARAMS(.delay = DB_NODELAY));
    tnotnull(item);
    created = sclone(dbField(item, "created"));
    updated = sclone(dbField(item, "updated"));
    rSleep(1100);

    recovered = dbOpen("./db/sec-journal-stamps.db", "./sec-regress.json5", DB_READ_ONLY);
    tnotnull(recovered);
    item = dbGet(recovered, "Thing", DB_PROPS("id", "stamped"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "created"), created);
    tmatch(dbField(item, "updated"), updated);

    rFree(created);
    rFree(updated);
    dbClose(recovered);
    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:error", 0, 1);
    testManyRecords();
    testRemoveReplay();
    testReplayTimestamps();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
