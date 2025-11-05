/*
    upsert.c.tst - Unit tests for upsert operations (update or insert)

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/

static void testCreateUpsert()
{
    Db          *db;
    CDbItem     *item;
    cchar       *id, *role;

    db = dbOpen("./db/upsert-create.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Create item with upsert flag
    item = dbCreate(db, "User", DB_PROPS(
        "username", "upsert1",
        "email", "upsert1@test.com",
        "role", "user"
    ), DB_PARAMS(.upsert = true));
    tnotnull(item);
    id = dbField(item, "id");
    role = dbField(item, "role");
    tmatch(role, "user");

    //  Create again with upsert - should update existing
    item = dbCreate(db, "User", DB_PROPS(
        "id", id,
        "username", "upsert1",
        "email", "upsert1@test.com",
        "role", "admin"
    ), DB_PARAMS(.upsert = true));
    tnotnull(item);
    role = dbField(item, "role");
    tmatch(role, "admin");  // Should be updated

    //  Verify only one item exists
    RList *items = dbFind(db, "User", DB_PROPS("username", "upsert1"), NULL);
    teqi(rGetListLength(items), 1);
    rFreeList(items);

    dbClose(db);
}

static void testUpdateUpsert()
{
    Db          *db;
    CDbItem     *item;
    cchar       *id, *role;

    db = dbOpen("./db/upsert-update.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Create initial item
    item = dbCreate(db, "User", DB_PROPS(
        "username", "upsert2",
        "email", "upsert2@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    id = dbField(item, "id");

    //  Update existing item with upsert
    item = dbUpdate(db, "User", DB_PROPS(
        "id", id,
        "role", "admin"
    ), DB_PARAMS(.upsert = true));
    tnotnull(item);
    role = dbField(item, "role");
    tmatch(role, "admin");

    //  Try to update non-existent item with upsert - should create
    item = dbUpdate(db, "User", DB_PROPS(
        "username", "upsert3",
        "email", "upsert3@test.com",
        "role", "guest"
    ), DB_PARAMS(.upsert = true));
    tnotnull(item);
    role = dbField(item, "role");
    tmatch(role, "guest");

    //  Verify two items exist
    RList *items = dbFind(db, "User", NULL, NULL);
    teqi(rGetListLength(items), 2);
    rFreeList(items);

    dbClose(db);
}

static void testUpsertWithoutFlag()
{
    Db          *db;
    CDbItem     *item;
    cchar       *error, *id;

    db = dbOpen("./db/upsert-noflag.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Create initial item
    item = dbCreate(db, "User", DB_PROPS(
        "username", "noupdate",
        "email", "noupdate@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    id = dbField(item, "id");

    //  Try to create duplicate without upsert flag - should fail
    item = dbCreate(db, "User", DB_PROPS(
        "id", id,
        "username", "noupdate",
        "email", "different@test.com",
        "role", "admin"
    ), NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    //  Try to update non-existent item without upsert - should fail
    item = dbUpdate(db, "User", DB_PROPS(
        "id", "non-existent-id",
        "role", "admin"
    ), NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testCreateUpsert();
    testUpdateUpsert();
    testUpsertWithoutFlag();

    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */