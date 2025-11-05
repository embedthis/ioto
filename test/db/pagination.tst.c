/*
    paginate.c.tst - Unit tests for pagination

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"
#include    "json.h"

/*********************************** Locals ***********************************/

#define ITEM_COUNT 100

/************************************ Code ************************************/

static Db *openDb()
{
    Db  *db;

    db = dbOpen("db/pagination.db", "./schema.json", DB_OPEN_RESET);
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
    RList       *list;
    CDbItem     *item;
    char        idbuf[16];
    cchar       *id;
    int         i;

    for (i = 0; i < ITEM_COUNT; i++) {
        item = dbCreate(db, "Item", DB_PROPS(
            "id", sfmtbuf(idbuf, sizeof(idbuf), "%04d", i)
        ), DB_PARAMS(.index = "primary"));
        id = dbField(item, "id");
        tnotnull(id);
        tmatch(id, idbuf);
    }
    list = dbFind(db, NULL, NULL, NULL);
    teqi(rGetListLength(list), ITEM_COUNT);
}

static void paginate(Db *db)
{
    RList       *list;
    CDbItem     *item;
    cchar       *id, *next;
    int         count, i, limit;

    count = 0;
    limit = 25;
    next = NULL;
    do {
        list = dbFind(db, NULL, NULL, DB_PARAMS(.next = next, .limit = limit));
        next = dbNext(db, list);
        for (ITERATE_ITEMS(list, item, i)) {
            id = dbField(item, "id");
            teqi(atoi(id), count + i);
        }
        count += rGetListLength(list);
        rFreeList(list);
    } while (next);
    teqi(count, ITEM_COUNT);
}

int main(void)
{
    Db  *db;

    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    db = openDb();
    createItems(db);
    paginate(db);
    closeDb(db);
    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
