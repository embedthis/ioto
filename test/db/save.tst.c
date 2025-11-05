/*
    save.c.tst - Unit tests for saving

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/

static void save()
{
    Db          *db;
    CDbItem     *item;
    int         status;

    db = dbOpen("./db/save.db", "./schema.json", DB_OPEN_RESET);
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

    status = dbSave(db, NULL);
    teqi(status, 0);
    ttrue(rFileExists("./db/save.db"));

    status = dbSave(db, "./db/save.tmp");
    ttrue(rFileExists("./db/save.tmp"));
    unlink("db/save.tmp");
    teqi(status, 0);


    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);
    save();
    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
