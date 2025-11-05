/*
    create.c.tst - Unit tests for create

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/

static void createDb()
{
    Db          *db;
    CDbItem     *item;
    cchar       *id, *username;

    db = dbOpen("./db/create-items.db", "./schema.json", DB_OPEN_RESET);
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

    id = dbField(item, "id");
    tnotnull(id);
    username = dbField(item, "username");
    tmatch(username, "admin");

    //  Refetch using id
    item = dbGet(db, "User", DB_PROPS("id", id), NULL);
    tnotnull(item);
    username = dbField(item, "username");
    tmatch(username, "admin");
    dbClose(db);
}

static void createJson()
{
    Db          *db;
    CDbItem     *item;
    cchar       *id, *username;

    db = dbOpen("./db/create-items-json.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "User", DB_JSON(
        "{username: 'admin', password: 'bad-password', email: 'admin@embedthis.com', role: 'admin'}"
    ), DB_PARAMS(.index = "primary"));

    tnotnull(item);
    tcontains(item->key, "user#");
    id = dbField(item, "id");
    tnotnull(id);
    username = dbField(item, "username");
    tmatch(username, "admin");
    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);
    createDb();
    createJson();
    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
