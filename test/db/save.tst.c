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

/*
    A save to another path is an export and must leave the journal untouched. A save to the
    database path recreates the journal, even when given in a different buffer.
 */
static void saveElsewhere()
{
    Db      *db;
    CDbItem *item;
    char    path[] = "./db/save-export.db";
    ssize   before, fresh;

    db = dbOpen(path, "./schema.json", DB_OPEN_RESET);
    tnotnull(db);
    //  A recreated journal has this size until the next change is flushed to it
    fresh = rGetFileSize("./db/save-export.db.jnl");
    item = dbCreate(db, "User", DB_PROPS(
        "username", "export",
        "email", "export@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    before = rGetFileSize("./db/save-export.db.jnl");
    tgtll((int64) before, (int64) fresh);

    teqi(dbSave(db, "./db/save-export.tmp"), 0);
    ttrue(rFileExists("./db/save-export.tmp"));
    teqll(rGetFileSize("./db/save-export.db.jnl"), before);
    unlink("./db/save-export.tmp");

    teqi(dbSave(db, path), 0);
    teqll(rGetFileSize("./db/save-export.db.jnl"), fresh);
    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);
    save();
    saveElsewhere();
    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
