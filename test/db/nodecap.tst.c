/*
    nodecap.tst.c - Regression for db#10029

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"
#include    "json.h"

/************************************ Code ************************************/

static Json *arrayProps(cchar *name, int count)
{
    RBuf *buf;
    Json *json;
    int  i;

    buf = rAllocBuf((size_t) count * 2 + 64);
    rPutToBuf(buf, "{id: 'x', %s: [", name);
    for (i = 0; i < count; i++) {
        rPutStringToBuf(buf, i ? ",0" : "0");
    }
    rPutStringToBuf(buf, "]}");
    json = jsonParse(rBufToString(buf), 0);
    rFreeBuf(buf);
    return json;
}

static void testUpdatePastNodeCap(void)
{
    Db           *db;
    Json         *create, *update;
    const DbItem *item;
    cchar        *error;

    db = dbOpen("./db/nodecap.db", "./nodecap.json5", DB_OPEN_RESET);
    tnotnull(db);


    create = arrayProps("b", 60000);
    jsonSet(create, 0, "a", "", JSON_ARRAY);
    item = dbCreate(db, "Doc", create, NULL);
    tnotnull(item);

    //  60000 + 50000 exceeds ME_JSON_MAX_NODES
    update = arrayProps("a", 50000);
    item = dbUpdate(db, "Doc", update, NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    //  The stored item is unchanged and still readable
    item = dbGet(db, "Doc", DB_PROPS("id", "x"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "id"), "x");
    tmatch(dbField(item, "a"), "[]");
    tmatch(dbField(item, "b[59999]"), "0");

    //  A small update to the same item still succeeds
    jsonFree(update);
    update = arrayProps("a", 3);
    item = dbUpdate(db, "Doc", update, NULL);
    tnotnull(item);
    tmatch(dbField(item, "a[2]"), "0");

    jsonFree(create);
    jsonFree(update);
    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:error:all", 0, 1);
    testUpdatePastNodeCap();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
