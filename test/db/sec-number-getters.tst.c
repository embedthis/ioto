/*
    sec-number-getters.tst.c - Regression for db#10027

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/

static void testNumberGetters(void)
{
    Db           *db;
    const DbItem *item;

    db = dbOpen("./db/sec-numbers.db", "./sec-regress.json5", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "Thing", DB_JSON("{id: 'q1', quota: 1e9}"), DB_PARAMS(.delay = DB_INMEM));
    tnotnull(item);
    tmatch(dbField(item, "quota"), "1e9");
    teqll(dbFieldNumber(item, "quota"), 1000000000LL);
    teqll(dbGetNum(db, "Thing", "quota", DB_PROPS("id", "q1"), NULL, -1), 1000000000LL);

    item = dbCreate(db, "Thing", DB_JSON("{id: 'q2', quota: 2.5e3}"), DB_PARAMS(.delay = DB_INMEM));
    tnotnull(item);
    teqll(dbFieldNumber(item, "quota"), 2500LL);
    teqll(dbGetNum(db, "Thing", "quota", DB_PROPS("id", "q2"), NULL, -1), 2500LL);

    //  Plain integers are unchanged
    item = dbCreate(db, "Thing", DB_JSON("{id: 'q3', quota: 42}"), DB_PARAMS(.delay = DB_INMEM));
    tnotnull(item);
    teqll(dbFieldNumber(item, "quota"), 42LL);
    teqll(dbGetNum(db, "Thing", "quota", DB_PROPS("id", "q3"), NULL, -1), 42LL);

    //  A missing field yields the default, and a missing item yields the default
    teqll(dbFieldNumber(item, "missing"), 0LL);
    teqll(dbGetNum(db, "Thing", "quota", DB_PROPS("id", "nope"), NULL, -1), -1LL);
    teqll(dbGetNum(db, "Thing", "missing", DB_PROPS("id", "q3"), NULL, -7), -7LL);

    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:error", 0, 1);
    testNumberGetters();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
