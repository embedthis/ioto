/*
    find.c.tst - Unit tests for find

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

    db = dbOpen("./db/find.db", "./schema.json", DB_OPEN_RESET);
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
    CDbItem     *item;
    cchar       *id;

    item = dbCreate(db, "User", DB_PROPS(
        "username", "admin",
        "password", "bad-password",
        "email", "admin@embedthis.com",
        "role", "admin"
    ), DB_PARAMS(.index = "primary"));

    id = dbField(item, "id");
    tnotnull(id);
}


static void findItems(Db *db)
{
    CDbItem     *item;
    RList       *items;
    cchar       *id;
    int         count, n;

    items = dbFind(db, "User", 0, 0);
    count = 0;
    for (ITERATE_ITEMS(items, item, n)) {
        count++;
    }
    rFreeList(items);
    teqi(count, 1);

    items = dbFind(db, "User", DB_JSON("{username:'admin'}"), 0);

    id = dbField(rGetItem(items, 0), "id");
    items = dbFind(db, "User", DB_PROPS("id", id), 0);
    teqi(items->length, 1);
}

/*
    Filter callback for custom filtering needs
 */
static int whereCallback(Json *data, int nid, cchar *arg)
{
    cchar   *username;

    tmatch(arg, "whereArg");
    username = jsonGet(data, nid, "username", 0);
    return smatch(username, "admin");
}

static void findCallback(Db *db)
{
    RList      *items;

    //  Use a callback for filtering
    items = dbFind(db, "User", NULL, DB_PARAMS(.where = (DbWhere) whereCallback, .arg = "whereArg"));
    teqi(items->length, 1);
}

int main(void)
{
    Db  *db;

    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    db = openDb();
    createItems(db);
    findItems(db);
    findCallback(db);
    closeDb(db);
    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
