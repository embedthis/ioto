/*
    remove.c.tst - Unit tests for item remove

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/

static Db *createDb()
{
    Db          *db;
    CDbItem     *item;

    db = dbOpen("./db/remove.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);
    tmatch(dbGetError(db), 0);

    item = dbCreate(db, "User", DB_PROPS(
        "username", "admin",
        "password", "bad-password",
        "email", "admin@embedthis.com",
        "role", "admin"
    ), DB_PARAMS(.index = "primary"));
    tnotnull(item);
    tcontains(item->key, "user#");
    return db;
}

static void closeDb(Db *db)
{
    dbClose(db);
}

static void update(Db *db)
{
    CDbItem     *item;
    cchar       *id, *role;
    int         count;

    item = dbFindOne(db, "User", DB_PROPS("username", "admin"), NULL);
    tnotnull(item);
    id = dbField(item, "id");
    tnotnull(id);
    role = dbField(item, "role");
    tmatch(role, "admin");

    count = dbRemove(db, "User", DB_PROPS("id", id), NULL);
    teqi(count, 1);
    item = dbGet(db, "User", DB_PROPS("id", id), NULL);
    tnull(item);

    //  Recreate
    db = createDb();
    tnotnull(db);

    //  Remove item that does not exist. Use limit > 1 to use non-sort key properties
    count = dbRemove(db, "User", DB_PROPS("username", "unknown"), DB_PARAMS(.limit = 2));
    teqi(count, 0);

    count = dbRemove(db, "User", DB_PROPS("username", "admin"), DB_PARAMS(.limit = 2));
    teqi(count, 1);
}

int main(void)
{
    Db  *db;

    rInit(0, 0);
    rSetLog("stdout:all,!debug,trace:all,!mbedtls", 0, 1);

    db = createDb();
    update(db);
    closeDb(db);
    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
