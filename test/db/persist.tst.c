/*
    persist.c.tst - Unit tests for database persistence

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/

static void testPersistParameters()
{
    Db          *db;
    CDbItem     *item;

    db = dbOpen("./db/persist-params.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Set journal parameters
    dbSetJournalParams(db, 500, 1024);  // 500ms delay, 1KB max size

    //  Create some items
    item = dbCreate(db, "User", DB_PROPS(
        "username", "user1",
        "email", "user1@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);

    item = dbCreate(db, "User", DB_PROPS(
        "username", "user2",
        "email", "user2@test.com",
        "role", "admin"
    ), NULL);
    tnotnull(item);

    //  Check that database file exists
    ttrue(rFileExists("./db/persist-params.db.jnl"));

    dbClose(db);
}

static void testPersistence()
{
    Db          *db;
    CDbItem     *item;
    RList       *items;
    char        *id;
    cchar       *username;

    //  Create database and add items
    db = dbOpen("./db/persist-persist.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "User", DB_PROPS(
        "username", "persistent",
        "email", "persist@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    id = sclone(dbField(item, "id"));
    tnotnull(id);

    //  Save and close
    teqi(dbSave(db, NULL), 0);
    dbClose(db);

    //  Reopen database - should load persisted data
    db = dbOpen("./db/persist-persist.db", "./schema.json", 0);
    tnotnull(db);

    //  Verify item still exists
    item = dbGet(db, "User", DB_PROPS("id", id), NULL);
    tnotnull(item);
    username = dbField(item, "username");
    tmatch(username, "persistent");

    //  Verify we can find the item
    items = dbFind(db, "User", DB_PROPS("username", "persistent"), NULL);
    tnotnull(items);
    teqi(rGetListLength(items), 1);
    rFreeList(items);

    rFree(id);
    dbClose(db);
}

static void testPersistRecovery()
{
    Db          *db;
    CDbItem     *item;
    char        *id;

    //  Create database with some data
    db = dbOpen("./db/persist-recovery.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "User", DB_PROPS(
        "username", "recovery",
        "email", "recovery@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    id = sclone(dbField(item, "id"));

    //  Force a write but don't save
    dbSetJournalParams(db, 1, 1);  // Very small size/delay to force persist

    item = dbUpdate(db, "User", DB_PROPS("id", id, "role", "admin"), NULL);
    tnotnull(item);

    //  Close without explicit save (simulates crash)
    dbClose(db);

    //  Reopen - should recover from persist
    db = dbOpen("./db/persist-recovery.db", "./schema.json", 0);
    tnotnull(db);

    //  Verify recovered data
    item = dbGet(db, "User", DB_PROPS("id", id), NULL);
    tnotnull(item);
    tmatch(dbField(item, "role"), "admin");

    rFree(id);
    dbClose(db);
}

static void testDelayedCommits()
{
    Db          *db;
    CDbItem     *item;
    char        *id;

    db = dbOpen("./db/persist-delayed.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Create item with delayed commit
    item = dbCreate(db, "User", DB_PROPS(
        "username", "delayed",
        "email", "delayed@test.com",
        "role", "user"
    ), DB_PARAMS(.delay = 100));  // 100ms delay
    tnotnull(item);

    id = sclone(dbField(item, "id"));

    //  Update with no delay (immediate)
    item = dbUpdate(db, "User", DB_PROPS("id", id, "role", "admin"), DB_PARAMS(.delay = DB_NODELAY));
    tnotnull(item);

    item = dbUpdate(db, "User", DB_PROPS("id", id, "email", "new@test.com"), DB_PARAMS(.mem = true));
    tnotnull(item);
    dbClose(db);
    rFree(id);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testPersistParameters();
    testPersistence();
    testPersistRecovery();
    testDelayedCommits();

    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
