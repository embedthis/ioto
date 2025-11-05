/*
    open.c.tst - Unit tests for open

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/

static void openDb()
{
    Db  *db;
    
    //  Database file does not need to exist
    unlink("./db/open.db");
    db = dbOpen("./db/open.db", "./schema.json", 0);
    tnotnull(db);
    tmatch(dbGetError(db), 0);
    dbClose(db);

#if KEEP
    //  Missing schema
    db = dbOpen("./new.db", "./unknown-schema.json", 0);
    tnull(db);
#endif
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);
    openDb();
    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
