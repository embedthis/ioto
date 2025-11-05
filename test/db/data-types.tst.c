/*
    data-types.c.tst - Unit tests for various data types and conversions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/

static void testBooleanFields()
{
    Db          *db;
    CDbItem     *item;
    cchar       *id;
    bool        value;

    db = dbOpen("./db/data-types-bool.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Create item with boolean fields
    item = dbCreate(db, "Port", DB_PROPS(
        "name", "eth0",
        "enable", "true",
        "negotiate", "false",
        "flowControl", "1",
        "jumbo", "0"
    ), NULL);
    tnotnull(item);
    id = dbField(item, "id");

    //  Test boolean field access
    value = dbFieldBool(item, "enable");
    ttrue(value);

    value = dbFieldBool(item, "negotiate");
    tfalse(value);

    value = dbFieldBool(item, "flowControl");
    ttrue(value);

    value = dbFieldBool(item, "jumbo");
    tfalse(value);

    //  Test setting boolean fields
    item = dbSetBool(db, "Port", "enable", false, DB_PROPS("id", id), NULL);
    tnotnull(item);
    value = dbFieldBool(item, "enable");
    tfalse(value);

    item = dbSetBool(db, "Port", "negotiate", true, DB_PROPS("id", id), NULL);
    tnotnull(item);
    value = dbFieldBool(item, "negotiate");
    ttrue(value);

    dbClose(db);
}

static void testNumericFields()
{
    Db          *db;
    CDbItem     *item;
    cchar       *id;
    int64       intValue;
    double      doubleValue;

    db = dbOpen("./db/data-types-numeric.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Create item with numeric fields
    item = dbCreate(db, "Port", DB_PROPS(
        "name", "eth0",
        "speed", "1000",
        "rxBytes", "1234567890",
        "txPackets", "999"
    ), NULL);
    tnotnull(item);
    id = dbField(item, "id");

    //  Test numeric field access
    intValue = dbFieldNumber(item, "speed");
    teqll(intValue, 1000);

    intValue = dbFieldNumber(item, "rxBytes");
    teqll(intValue, 1234567890LL);

    intValue = dbFieldNumber(item, "txPackets");
    teqll(intValue, 999);

    //  Test setting numeric fields
    item = dbSetNum(db, "Port", "speed", 10000, DB_PROPS("id", id), NULL);
    tnotnull(item);
    intValue = dbFieldNumber(item, "speed");
    teqll(intValue, 10000);

    //  Test double values
    item = dbSetDouble(db, "Port", "rxBytes", 123.456, DB_PROPS("id", id), NULL);
    tnotnull(item);
    doubleValue = dbFieldDouble(item, "rxBytes");
    ttrue(doubleValue == 123.456);

    dbClose(db);
}

static void testDateFields()
{
    Db          *db;
    CDbItem     *item;
    cchar       *id, *expires;
    Time        dateValue, now;

    db = dbOpen("./db/data-types-date.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    now = rGetTime();
    expires = rGetIsoDate(now + 5 * TPS);

    //  Create item with date field
    item = dbCreate(db, "Event", DB_PROPS(
        "message", "Test event",
        "source", "test",
        "severity", "info",
        "subject", "Testing",
        "expires", expires
    ), NULL);
    tnotnull(item);
    id = dbField(item, "id");

    //  Test date field access
    dateValue = dbFieldDate(item, "expires");
    tgtll(dateValue, 0);
    teqll(dateValue, now + 5 * TPS);

    //  Test setting date fields
    Time newTime = rGetTime() + 3600000;  // Add 1 hour

    item = dbSetDate(db, "Event", "expires", newTime, DB_PROPS("id", id), NULL);
    tnotnull(item);
    dateValue = dbFieldDate(item, "expires");
    teqll(dateValue, newTime);

    dbClose(db);
}

static void testStringFields()
{
    Db          *db;
    CDbItem     *item;
    cchar       *id, *value;

    db = dbOpen("./db/data-types-string.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Create item with string fields
    item = dbCreate(db, "User", DB_PROPS(
        "username", "testuser",
        "email", "test@example.com",
        "role", "admin"
    ), NULL);
    tnotnull(item);
    id = dbField(item, "id");

    //  Test string field access
    value = dbField(item, "username");
    tmatch(value, "testuser");

    value = dbField(item, "email");
    tmatch(value, "test@example.com");

    value = dbField(item, "role");
    tmatch(value, "admin");

    //  Test setting string fields
    item = dbSetString(db, "User", "username", "newuser", DB_PROPS("id", id), NULL);
    tnotnull(item);
    value = dbField(item, "username");
    tmatch(value, "newuser");

    item = dbSetField(db, "User", "role", "user", DB_PROPS("id", id), NULL);
    tnotnull(item);
    value = dbField(item, "role");
    tmatch(value, "user");

    dbClose(db);
}

static void testDefaultValues()
{
    Db          *db;
    CDbItem     *item;
    bool        boolVal;
    int64       numVal;
    cchar       *strVal;

    db = dbOpen("./db/data-types-defaults.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Create item without specifying fields with defaults
    item = dbCreate(db, "Port", DB_PROPS("name", "eth0"), NULL);
    tnotnull(item);

    //  Test default values are set
    boolVal = dbFieldBool(item, "enable");
    ttrue(boolVal);  // Default is true

    boolVal = dbFieldBool(item, "flowControl");
    ttrue(boolVal);  // Default is true

    boolVal = dbFieldBool(item, "jumbo");
    ttrue(boolVal);  // Default is true

    boolVal = dbFieldBool(item, "negotiate");
    ttrue(boolVal);  // Default is true

    numVal = dbFieldNumber(item, "speed");
    teqll(numVal, 1000);  // Default is 1000

    strVal = dbField(item, "duplex");
    tmatch(strVal, "full");  // Default is "full"

    strVal = dbField(item, "status");
    tmatch(strVal, "online");  // Default is "online"

    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testBooleanFields();
    testNumericFields();
    testDateFields();
    testStringFields();
    testDefaultValues();

    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
