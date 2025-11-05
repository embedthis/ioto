/*
    expire.c.tst - Unit tests for TTL expiry

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"
#include    "json.h"

/************************************ Code ************************************/

static Db *openDb()
{
    Db  *db;

    db = dbOpen("./db/expire.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);
    tmatch(dbGetError(db), 0);
    return db;
}

static void closeDb(Db *db)
{
    dbClose(db);
}

static void createItems(Db *db)
{
    Time        now;
    RList       *grid;
    char        idbuf[80], mbuf[80], *expires;
    int         i, count;

    now = rGetTime();
    expires = rGetIsoDate(now + 999);

    count = 10;
    for (i = 0; i < count; i++) {
        dbCreate(db, "Event", DB_PROPS(
            "message", SFMT(mbuf, "Hello World %d", i),
            "subject", "Greeting",
            "expires", expires,
            "source", "test",
            "severity", "info",
            "id", SFMT(idbuf, "id-%d", i)
        ), DB_PARAMS(.index = "primary"));
    }
    grid = dbFind(db, "Event", NULL, NULL);
    tnotnull(grid);
    teqi(rGetListLength(grid), count);
}

static void getItems(Db *db)
{
    CDbItem     *item;

    //  Before expiry, should pass (unless debugging and paused before)
    item = dbGet(db, "Event", DB_PROPS("id", "id-0"), NULL);
    tnotnull(item);
    rSleep(1000);

    item = dbGet(db, "Event", DB_PROPS("id", "id-0"), NULL);
    tnull(item);
}

static void expireItems(Db *db)
{
    RList   *grid;

    dbRemoveExpired(db, 0);

    grid = dbFind(db, "Event", NULL, NULL);
    tnotnull(grid);
    teqi(rGetListLength(grid), 0);
}

int main(void)
{
    Db  *db;

    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    db = openDb();
    createItems(db);
    getItems(db);
    expireItems(db);
    closeDb(db);
    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
