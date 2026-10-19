/*
    sec-clone-depth.tst.c - Regression for db#10019

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"
#include    "json.h"

/************************************ Code ************************************/

/*
    Parse props to avoid USER_ALLOC so setup must clone them.
 */
static Json *deepProps(cchar *id, int depth)
{
    RBuf *buf;
    Json *json;
    char *str;
    int  i;

    buf = rAllocBuf(depth * 8 + 128);
    rPutToBuf(buf, "{\"id\":\"%s\",\"name\":\"n\",\"deep\":", id);
    for (i = 0; i < depth; i++) {
        rPutStringToBuf(buf, "{\"a\":");
    }
    rPutStringToBuf(buf, "\"leaf\"");
    for (i = 0; i < depth; i++) {
        rPutCharToBuf(buf, '}');
    }
    rPutCharToBuf(buf, '}');
    str = rBufToString(buf);
    json = jsonParse(str, 0);
    rFreeBuf(buf);
    return json;
}


static void testDeepPropsRejected(void)
{
    Db           *db;
    Json         *props;
    const DbItem *item;
    RList        *list;
    cchar        *error;
    int          count;

    db = dbOpen("./db/sec-clone-depth.db", "./sec-regress.json5", DB_OPEN_RESET);
    tnotnull(db);

    //  40 is deeper than ME_JSON_MAX_RECURSION (32), so jsonClone inside setup() returns NULL
    props = deepProps("deep-1", 40);
    tnotnull(props);
    //  USER_ALLOC (0x1) is private to dbLib. Not set here, which is the vulnerable path
    ttrue(!(props->userFlags & 0x1));

    item = dbCreate(db, "Thing", props, NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    item = dbGet(db, "Thing", props, NULL);
    tnull(item);

    list = dbFind(db, "Thing", props, NULL);
    tnull(list);

    item = dbUpdate(db, "Thing", props, NULL);
    tnull(item);

    count = dbRemove(db, "Thing", props, NULL);
    ttrue(count < 0);

    item = dbFindOne(db, "Thing", props, NULL);
    tnull(item);

    jsonFree(props);
    dbClose(db);
}


static void testShallowPropsAccepted(void)
{
    Db           *db;
    Json         *props;
    const DbItem *item;

    db = dbOpen("./db/sec-clone-shallow.db", "./sec-regress.json5", DB_OPEN_RESET);
    tnotnull(db);

    props = deepProps("shallow-1", 4);
    tnotnull(props);
    item = dbCreate(db, "Thing", props, NULL);
    tnotnull(item);
    jsonFree(props);

    item = dbGet(db, "Thing", DB_PROPS("id", "shallow-1"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "name"), "n");

    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:error:all", 0, 1);
    testDeepPropsRejected();
    testShallowPropsAccepted();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
