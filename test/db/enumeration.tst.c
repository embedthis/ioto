/*
    enumeration.c.tst - Unit tests for enumerated field values

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/

static void testValidEnumValues()
{
    Db          *db;
    CDbItem     *item;
    cchar       *value;

    db = dbOpen("./db/enumeration-valid.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Test valid User role enum values
    item = dbCreate(db, "User", DB_PROPS(
        "username", "user1",
        "email", "user1@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    value = dbField(item, "role");
    tmatch(value, "user");

    item = dbCreate(db, "User", DB_PROPS(
        "username", "admin1",
        "email", "admin1@test.com",
        "role", "admin"
    ), NULL);
    tnotnull(item);
    value = dbField(item, "role");
    tmatch(value, "admin");

    item = dbCreate(db, "User", DB_PROPS(
        "username", "guest1",
        "email", "guest1@test.com",
        "role", "guest"
    ), NULL);
    tnotnull(item);
    value = dbField(item, "role");
    tmatch(value, "guest");

    item = dbCreate(db, "User", DB_PROPS(
        "username", "super1",
        "email", "super1@test.com",
        "role", "super"
    ), NULL);
    tnotnull(item);
    value = dbField(item, "role");
    tmatch(value, "super");

    dbClose(db);
}

static void testValidEventSeverityEnums()
{
    Db          *db;
    CDbItem     *item;
    cchar       *value;

    db = dbOpen("./db/enumeration-event.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Test valid Event severity enum values
    item = dbCreate(db, "Event", DB_PROPS(
        "message", "Info message",
        "source", "test",
        "severity", "info",
        "subject", "Test"
    ), NULL);
    tnotnull(item);
    value = dbField(item, "severity");
    tmatch(value, "info");

    item = dbCreate(db, "Event", DB_PROPS(
        "message", "Warning message",
        "source", "test",
        "severity", "warn",
        "subject", "Test"
    ), NULL);
    tnotnull(item);
    value = dbField(item, "severity");
    tmatch(value, "warn");

    item = dbCreate(db, "Event", DB_PROPS(
        "message", "Error message",
        "source", "test",
        "severity", "error",
        "subject", "Test"
    ), NULL);
    tnotnull(item);
    value = dbField(item, "severity");
    tmatch(value, "error");

    item = dbCreate(db, "Event", DB_PROPS(
        "message", "Critical message",
        "source", "test",
        "severity", "critical",
        "subject", "Test"
    ), NULL);
    tnotnull(item);
    value = dbField(item, "severity");
    tmatch(value, "critical");

    item = dbCreate(db, "Event", DB_PROPS(
        "message", "Fatal message",
        "source", "test",
        "severity", "fatal",
        "subject", "Test"
    ), NULL);
    tnotnull(item);
    value = dbField(item, "severity");
    tmatch(value, "fatal");

    dbClose(db);
}

static void testValidPortStatusEnums()
{
    Db          *db;
    CDbItem     *item;
    cchar       *value;

    db = dbOpen("./db/enumeration-port.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Test valid Port status enum values
    item = dbCreate(db, "Port", DB_PROPS(
        "name", "eth0",
        "status", "online"
    ), NULL);
    tnotnull(item);
    value = dbField(item, "status");
    tmatch(value, "online");

    item = dbCreate(db, "Port", DB_PROPS(
        "name", "eth1",
        "status", "offline"
    ), NULL);
    tnotnull(item);
    value = dbField(item, "status");
    tmatch(value, "offline");

    dbClose(db);
}

static void testEnumUpdates()
{
    Db          *db;
    CDbItem     *item;
    char        *id;
    cchar       *value;

    db = dbOpen("./db/enumeration-updates.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Create item with initial enum value
    item = dbCreate(db, "User", DB_PROPS(
        "username", "testuser",
        "email", "test@example.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    id = sclone(dbField(item, "id"));
    value = dbField(item, "role");
    tmatch(value, "user");

    //  Update to different valid enum value
    item = dbUpdate(db, "User", DB_PROPS("id", id, "role", "admin"), NULL);
    tnotnull(item);
    value = dbField(item, "role");
    tmatch(value, "admin");

    //  Update using set field method
    item = dbSetField(db, "User", "role", "guest", DB_PROPS("id", id), NULL);
    tnotnull(item);
    value = dbField(item, "role");
    tmatch(value, "guest");

    rFree(id);
    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testValidEnumValues();
    testValidEventSeverityEnums();
    testValidPortStatusEnums();
    testEnumUpdates();

    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */