/*
    sec-delayed-remove.tst.c - Regression for db#10024

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/


static bool removedAfterCrash(cchar *path, cchar *id, int delay)
{
    Db           *db, *recovered;
    const DbItem *item;
    bool         removed;
    int          count;

    db = dbOpen(path, "./sec-regress.json5", DB_OPEN_RESET);
    tnotnull(db);
    item = dbCreate(db, "Thing", DB_PROPS("id", id, "name", "n"), DB_PARAMS(.delay = DB_NODELAY));
    tnotnull(item);

    count = dbRemove(db, "Thing", DB_PROPS("id", id), DB_PARAMS(.delay = delay));
    teqi(count, 1);

    //  Wait well past the delay so any pending commit has run
    rSleep(delay > 0 ? delay * 5 : 50);

    //  Simulate a crash: the first handle is never saved, so the journal is the only record
    recovered = dbOpen(path, "./sec-regress.json5", DB_READ_ONLY);
    tnotnull(recovered);
    item = dbGet(recovered, "Thing", DB_PROPS("id", id), NULL);
    removed = item == 0;
    dbClose(recovered);
    dbClose(db);
    return removed;
}

static void run(void)
{
    //  Control: an undelayed removal is journalled immediately
    ttrue(removedAfterCrash("./db/sec-delayed-nodelay.db", "u1", DB_NODELAY));

    //  A delayed removal must also reach the journal
    ttrue(removedAfterCrash("./db/sec-delayed-remove.db", "u2", 20));
    rStop();
}

int main(void)
{
    rInit((RFiberProc) run, 0);
    rSetLog("stdout:all,!debug,!trace:all", 0, 1);
    rServiceEvents();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
