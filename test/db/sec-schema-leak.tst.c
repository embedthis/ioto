/*
    sec-schema-leak.tst.c - Blended schema includes are not leaked

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"
#include    "leak.h"

/************************************ Code ************************************/

/*
    blend.json5 includes blend-models.json5, so each open parses and blends an extra tree.
 */
static void testSchemaIncludeLeak(void)
{
    Db    *db;
    ssize baseline, growth;
    int   i, iterations;

    if (!leakCheckEnabled()) {
        return;
    }
    iterations = 200;

    //  Open once first so any one-off allocations are outside the measurement
    db = dbOpen("./db/sec-leak.db", "./blend.json5", DB_OPEN_RESET);
    tnotnull(db);
    dbClose(db);

    baseline = leakBytesInUse();

    for (i = 0; i < iterations; i++) {
        db = dbOpen("./db/sec-leak.db", "./blend.json5", DB_OPEN_RESET);
        tnotnull(db);
        dbClose(db);
    }
    growth = leakBytesInUse() - baseline;

    tinfo("schema include leak check: baseline bytes=%lld growth=%lld", (long long) baseline, (long long) growth);
    //  The blended include is several K per open, so a leak dwarfs this bound
    ttrue(growth < (ssize) iterations * 64);
}

int main(int argc, char **argv)
{
    leakCheckInit(argv);
    rInit(0, 0);
    rSetLog("stdout:error", 0, 1);
    testSchemaIncludeLeak();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
