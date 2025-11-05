/*
    error-handling.c.tst - Unit tests for error conditions and edge cases

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

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

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testInvalidOperations();
    testNullParameters();
    testDataTypeValidation();
    testLargeData();

    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */