/*
    fields.c.tst - Unit tests for get/set fields

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

    db = dbOpen("./db/fields.db", "./schema.json", DB_OPEN_RESET);
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

static void getset(Db *db)
{
    CDbItem     *item;
    cchar       *id, *value;

    item = dbFindOne(db, "User", DB_PROPS("username", "admin"), NULL);
    tnotnull(item);
    id = dbField(item, "id");
    tnotnull(id);

    item = dbGet(db, "User", DB_PROPS("id", id), NULL);
    tnotnull(item);

    //  Read an item's property
    value = dbGetField(db, "User", "role", DB_PROPS("id", id), NULL);
    tmatch(value, "admin");

    item = dbSetField(db, "User", "username", "ralph", DB_PROPS("id", id), NULL);
    tmatch(dbField(item, "username"), "ralph");
}

int main(void)
{
    Db  *db;

    rInit(0, 0);
    rSetLog("stdout:all,!debug,trace:all,!mbedtls", 0, 1);
    db = createDb();
    getset(db);
    closeDb(db);
    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
