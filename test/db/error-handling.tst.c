/*
    error-handling.c.tst - Unit tests for error conditions and edge cases

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

#if defined(__APPLE__)
    #include    <malloc/malloc.h>
#elif defined(__linux__)
    #include    <malloc.h>
#endif

/************************************ Code ************************************/

static void testInvalidOperations()
{
    Db          *db;
    CDbItem     *item;
    int         count;
    cchar       *error;

    db = dbOpen("./db/error-handling.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Try to get non-existent item
    item = dbGet(db, "User", DB_PROPS("id", "non-existent-id"), NULL);
    tnull(item);

    //  Try to update non-existent item
    item = dbUpdate(db, "User", DB_PROPS("id", "non-existent-id", "role", "admin"), NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    //  Try to create with invalid model
    item = dbCreate(db, "InvalidModel", DB_PROPS("name", "test"), NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    //  Try to remove non-existent item
    count = dbRemove(db, "User", DB_PROPS("id", "non-existent-id"), NULL);
    teqi(count, 0);

    dbClose(db);
}

static void testNullParameters()
{
    Db          *db;
    CDbItem     *item;
    RList       *items;

    db = dbOpen("./db/error-handling-null.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Test NULL database parameter (should not crash)
    item = dbCreate(NULL, "User", DB_PROPS("username", "test"), NULL);
    tnull(item);

    //  Test NULL properties
    item = dbCreate(db, "User", NULL, NULL);
    tnull(item);

    //  Test empty properties
    item = dbCreate(db, "User", DB_PROPS(NULL), NULL);
    tnull(item);

    //  Test find with NULL model and props
    items = dbFind(db, NULL, NULL, NULL);
    tnotnull(items);
    rFreeList(items);

    dbClose(db);
}

static void testMissingModel()
{
    Db          *db;
    CDbItem     *item;
    RList       *items;
    Json        *props;
    char        path[128];
    int         bypass, count;

    sfmtbuf(path, sizeof(path), "./db/missing-model-%d.db", getpid());
    db = dbOpen(path, "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    for (bypass = 0; bypass <= 1; bypass++) {
        props = jsonParse("{sk:'item#missing', id:'missing'}", 0);
        item = dbCreate(db, NULL, props, DB_PARAMS(.bypass = bypass));
        tnull(item);
        teqi(db->code, R_ERR_BAD_ARGS);
        tmatch(dbGetError(db), "Missing model");

        item = dbUpdate(db, NULL, props, DB_PARAMS(.bypass = bypass));
        tnull(item);
        teqi(db->code, R_ERR_BAD_ARGS);
        tmatch(dbGetError(db), "Missing model");

        item = dbUpdate(db, NULL, props, DB_PARAMS(.bypass = bypass, .upsert = 1));
        tnull(item);
        teqi(db->code, R_ERR_BAD_ARGS);
        tmatch(dbGetError(db), "Missing model");

        //  A well-formed payload still resolves its model and creates the item.
        item = dbCreate(db, NULL, DB_PROPS("_type", "Item", "id", "missing"), DB_PARAMS(.bypass = bypass));
        tnotnull(item);

        count = dbRemove(db, NULL, props, DB_PARAMS(.bypass = bypass));
        teqi(count, R_ERR_BAD_ARGS);
        tmatch(dbGetError(db), "Missing model");
        jsonFree(props);

        //  Model-less reads remain valid, and the rejected remove preserves the item.
        item = dbGet(db, NULL, DB_PROPS("sk", "item#missing"), NULL);
        tnotnull(item);
        items = dbFind(db, NULL, NULL, NULL);
        tnotnull(items);
        teqi(rGetListLength(items), 1);
        rFreeList(items);

        item = dbUpdate(db, NULL, DB_PROPS("_type", "Item", "id", "missing"), DB_PARAMS(.bypass = bypass));
        tnotnull(item);
        item = dbUpdate(db, NULL, DB_PROPS("_type", "Item", "id", "upsert"),
                        DB_PARAMS(.bypass = bypass, .upsert = 1));
        tnotnull(item);
        count = dbRemove(db, NULL, DB_PROPS("_type", "Item", "id", "missing"), DB_PARAMS(.bypass = bypass));
        teqi(count, 1);
        count = dbRemove(db, NULL, DB_PROPS("_type", "Item", "id", "upsert"), DB_PARAMS(.bypass = bypass));
        teqi(count, 1);
    }
    dbClose(db);
    unlink(path);
}

static void testDataTypeValidation()
{
    Db          *db;
    CDbItem     *item;
    bool        boolVal;
    int64       numVal;
    double      doubleVal;
    Time        dateVal;

    db = dbOpen("./db/error-handling-types.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Create item with various data types
    item = dbCreate(db, "Port", DB_PROPS(
        "name", "eth0",
        "enable", "true",
        "speed", "1000",
        "negotiate", "false"
    ), NULL);
    tnotnull(item);

    //  Test field access as different types
    boolVal = dbFieldBool(item, "enable");
    ttrue(boolVal);

    boolVal = dbFieldBool(item, "negotiate");
    tfalse(boolVal);

    numVal = dbFieldNumber(item, "speed");
    teqll(numVal, 1000);

    //  Test accessing non-existent field
    boolVal = dbFieldBool(item, "non-existent");
    tfalse(boolVal);

    numVal = dbFieldNumber(item, "non-existent");
    teqll(numVal, 0);

    doubleVal = dbFieldDouble(item, "non-existent");
    ttrue(doubleVal == 0.0);

    dateVal = dbFieldDate(item, "non-existent");
    teqll(dateVal, 0);

    dbClose(db);
}

static void testLargeData()
{
    Db          *db;
    CDbItem     *item;
    char        *largeString;
    cchar       *retrieved;

    db = dbOpen("./db/error-handling-large.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Create a large string (within limits)
    largeString = rAlloc(8192);
    memset(largeString, 'A', 8191);
    largeString[8191] = '\0';

    item = dbCreate(db, "User", DB_PROPS(
        "username", "largeuser",
        "email", largeString
    ), NULL);
    tnotnull(item);

    retrieved = dbField(item, "email");
    tnotnull(retrieved);
    teqz(strlen(retrieved), 8191);

    rFree(largeString);
    dbClose(db);
}

/*
    Check allocation counts across each setup rejection path.
 */
static void testSetupFailureLeak()
{
    Db      *db;
    CDbItem *item;
    char    *longId;
    ssize   blockGrowth;
    int     i, iterations;
#if defined(__APPLE__)
    malloc_zone_t      *zone;
    malloc_statistics_t baseline, after;
#elif defined(__linux__)
    struct mallinfo2   baseline, after;
#endif

    db = dbOpen("./db/error-handling-leak.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    iterations = 2000;
    longId = rAlloc(DB_MAX_KEY + 1);
    memset(longId, 'k', DB_MAX_KEY);
    longId[DB_MAX_KEY] = '\0';

#if defined(__APPLE__)
    zone = malloc_default_zone();
    malloc_zone_statistics(zone, &baseline);
#elif defined(__linux__)
    baseline = mallinfo2();
#endif

    for (i = 0; i < iterations; i++) {
        item = dbCreate(db, "NoSuchModel", DB_PROPS("id", "x"), NULL);
        tnull(item);

        item = dbCreate(db, "Event", DB_PROPS(
            "message", "m", "subject", "s", "source", "src", "severity", "bogus"
        ), NULL);
        tnull(item);

        item = dbCreate(db, "Event", DB_PROPS(
            "message", "m", "subject", "s", "source", "src", "severity", "info", "expires", "not-a-date"
        ), NULL);
        tnull(item);

        item = dbGet(db, NULL, DB_PROPS("message", "x"), NULL);
        tnull(item);

        item = dbGet(db, "Item", DB_PROPS("dummy", "x"), NULL);
        tnull(item);

        item = dbCreate(db, "Item", DB_PROPS("id", longId), NULL);
        tnull(item);
    }
    rFree(longId);

#if defined(__APPLE__)
    malloc_zone_statistics(zone, &after);
    blockGrowth = (ssize) after.blocks_in_use - (ssize) baseline.blocks_in_use;
    tinfo("setup failure leak check: baseline blocks=%u after blocks=%u growth=%lld", baseline.blocks_in_use,
        after.blocks_in_use, (long long) blockGrowth);
    ttrue(blockGrowth < (ssize) iterations);
#elif defined(__linux__)
    after = mallinfo2();
    blockGrowth = (ssize) after.uordblks - (ssize) baseline.uordblks;
    tinfo("setup failure leak check: baseline bytes=%zu after bytes=%zu growth=%lld", baseline.uordblks,
        after.uordblks, (long long) blockGrowth);
    ttrue(blockGrowth < (ssize) iterations * 96);
#else
    tinfo("Skipping setup failure leak check: no malloc statistics API for this platform");
#endif
    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testInvalidOperations();
    testNullParameters();
    testMissingModel();
    testDataTypeValidation();
    testLargeData();
    testSetupFailureLeak();

    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
