/*
    sec-enum-quoted.tst.c - Regression for db#10026

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/

static void testEnumQuoted(void)
{
    Db           *db;
    const DbItem *item;

    db = dbOpen("./db/sec-enum.db", "./sec-regress.json5", DB_OPEN_RESET);
    tnotnull(db);

    //  Every declared element is valid, including those holding a quote, a space and a comma
    item = dbCreate(db, "Thing", DB_PROPS("id", "e1", "kind", "a"), DB_PARAMS(.delay = DB_INMEM));
    tnotnull(item);
    item = dbCreate(db, "Thing", DB_PROPS("id", "e2", "kind", "it's"), DB_PARAMS(.delay = DB_INMEM));
    tnotnull(item);
    item = dbCreate(db, "Thing", DB_PROPS("id", "e3", "kind", "b c"), DB_PARAMS(.delay = DB_INMEM));
    tnotnull(item);
    item = dbCreate(db, "Thing", DB_PROPS("id", "e4", "kind", "x,y"), DB_PARAMS(.delay = DB_INMEM));
    tnotnull(item);

    //  Fragments of the serialized list are not elements
    item = dbCreate(db, "Thing", DB_PROPS("id", "e5", "kind", ","), DB_PARAMS(.delay = DB_INMEM));
    tnull(item);
    item = dbCreate(db, "Thing", DB_PROPS("id", "e6", "kind", "it\\"), DB_PARAMS(.delay = DB_INMEM));
    tnull(item);
    item = dbCreate(db, "Thing", DB_PROPS("id", "e7", "kind", "it"), DB_PARAMS(.delay = DB_INMEM));
    tnull(item);
    item = dbCreate(db, "Thing", DB_PROPS("id", "e8", "kind", "unknown"), DB_PARAMS(.delay = DB_INMEM));
    tnull(item);

    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:error", 0, 1);
    testEnumQuoted();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
