/*
    expire.c.tst - Unit tests for TTL expiry

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"
#include    "json.h"
#include    "leak.h"

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

/*
    Removing expired items journals each removal. When that fills the journal, the save is
    deferred until the sweep finishes and then taken.
 */
static void expireFillsJournal()
{
    Db      *db;
    CDbItem *item;
    RList   *grid;
    char    idbuf[80], *expired;
    ssize   fresh;
    int     i;

    db = dbOpen("./db/expire-journal.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);
    //  A recreated journal has this size until the next change is flushed to it
    fresh = rGetFileSize("./db/expire-journal.db.jnl");
    expired = rGetIsoDate(rGetTime() - 1000);
    for (i = 0; i < 5; i++) {
        item = dbCreate(db, "Event", DB_PROPS(
            "message", "stale",
            "subject", "Greeting",
            "expires", expired,
            "source", "test",
            "severity", "info",
            "id", SFMT(idbuf, "old-%d", i)
        ), NULL);
        tnotnull(item);
    }
    rFree(expired);

    //  Any journalled change now exceeds the size limit
    dbSetJournalParams(db, 60 * 1000, 1);
    teqi(dbRemoveExpired(db, 1), 5);

    //  The deferred save ran and recreated the journal
    teqll(rGetFileSize("./db/expire-journal.db.jnl"), fresh);
    dbClose(db);

    db = dbOpen("./db/expire-journal.db", "./schema.json", 0);
    tnotnull(db);
    grid = dbFind(db, "Event", NULL, NULL);
    tnotnull(grid);
    teqi(rGetListLength(grid), 0);
    rFreeList(grid);
    dbClose(db);
}

/*
    Check that repeated expiry cleanup returns allocations to baseline.
 */
static void expiredListLeak()
{
    Db      *db;
    char    idbuf[80], *expired;
    ssize   baseline, growth;
    int     i, iterations;

    if (!leakCheckEnabled()) {
        return;
    }
    db = dbOpen("./db/expire-leak.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    iterations = 2000;
    baseline = leakBytesInUse();

    expired = rGetIsoDate(rGetTime() - 1000);
    for (i = 0; i < iterations; i++) {
        dbCreate(db, "Event", DB_PROPS(
            "message", "stale",
            "subject", "Greeting",
            "expires", expired,
            "source", "test",
            "severity", "info",
            "id", SFMT(idbuf, "leak-%d", i)
        ), NULL);
        dbGet(db, "Event", DB_PROPS("id", idbuf), NULL);
        rRunEvents();
    }
    rFree(expired);

    growth = leakBytesInUse() - baseline;
    tinfo("expiredItems leak check: baseline bytes=%lld growth=%lld", (long long) baseline, (long long) growth);
    ttrue(growth < (ssize) iterations * 96);
    dbClose(db);
}

/*
    Re-create the expired key after reload to detect missing journal removals.
    Reads would hide a stale entry by checking its expiry again.
 */
static void expiryPersistsAcrossRestart()
{
    Db      *db;
    CDbItem *item;
    char    *expires;

    db = dbOpen("./db/expire-restart.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    expires = rGetIsoDate(rGetTime() + 200);
    item = dbCreate(db, "Event", DB_PROPS(
        "message", "short-lived",
        "subject", "Greeting",
        "expires", expires,
        "source", "test",
        "severity", "info",
        "id", "restart-1"
    ), NULL);
    tnotnull(item);
    rFree(expires);

    // Persist first so expiry is the only pending journal change.
    teqi(dbSave(db, NULL), 0);

    rSleep(300);

    item = dbGet(db, "Event", DB_PROPS("id", "restart-1"), NULL);
    tnull(item);

    dbClose(db);

    db = dbOpen("./db/expire-restart.db", "./schema.json", 0);
    tnotnull(db);

    item = dbCreate(db, "Event", DB_PROPS(
        "message", "fresh",
        "subject", "Greeting",
        "source", "test",
        "severity", "info",
        "id", "restart-1"
    ), NULL);
    tnotnull(item);
    tmatch(dbField(item, "message"), "fresh");

    dbClose(db);
}

int main(int argc, char **argv)
{
    Db  *db;

    leakCheckInit(argv);
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    db = openDb();
    createItems(db);
    getItems(db);
    expireItems(db);
    closeDb(db);
    expireFillsJournal();
    expiredListLeak();
    expiryPersistsAcrossRestart();
    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
