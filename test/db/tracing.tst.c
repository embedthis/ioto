/*
    tracing.c.tst - Unit tests for the debug and trace diagnostic paths

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/

/*
    Opening with debug enabled dumps the resolved schema
 */
static void testSchemaTrace()
{
    Db      *db;
    CDbItem *item;

    db = dbOpen("./db/tracing-schema.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);
    tmatch(dbGetError(db), NULL);

    item = dbCreate(db, "User", DB_PROPS(
        "username", "traced",
        "email", "traced@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);

    dbClose(db);
}

/*
    Expiring items with tracing enabled logs each removal, both through the explicit
    sweep and through the lazy removal done while querying
 */
static void testExpiryTrace()
{
    Db      *db;
    CDbItem *item;
    RList   *items;
    char    idbuf[80];
    char    *expires;
    int     count, i;

    db = dbOpen("./db/tracing-expiry.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    expires = rGetIsoDate(rGetTime() - TPS);
    for (i = 0; i < 4; i++) {
        item = dbCreate(db, "Event", DB_PROPS(
            "message", "stale event",
            "subject", "Expiry",
            "source", "test",
            "severity", "info",
            "expires", expires,
            "id", SFMT(idbuf, "old-%d", i)
        ), NULL);
        tnotnull(item);
    }

    //  A find omits expired items and they are removed from a subsequent event
    items = dbFind(db, "Event", NULL, DB_PARAMS(.log = 1));
    teqi(rGetListLength(items), 0);
    rFreeList(items);
    rRunEvents();

    //  They have already gone, so the explicit sweep finds nothing left
    count = dbRemoveExpired(db, 0);
    teqi(count, 0);

    //  Add more and remove them through the explicit sweep with tracing on
    for (i = 0; i < 4; i++) {
        item = dbCreate(db, "Event", DB_PROPS(
            "message", "stale event",
            "subject", "Expiry",
            "source", "test",
            "severity", "info",
            "expires", expires,
            "id", SFMT(idbuf, "new-%d", i)
        ), NULL);
        tnotnull(item);
    }
    rFree(expires);

    count = dbRemoveExpired(db, 0);
    teqi(count, 4);

    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    //  Enable the debug and trace paths that are compiled out of a normal test run
    rSetLog("stdout:all:all,!mbedtls", 0, 1);

    testSchemaTrace();
    testExpiryTrace();

    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
