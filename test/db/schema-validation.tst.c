/*
    schema-validation.c.tst - Unit tests for schema validation and constraints

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/

static void testRequiredFields()
{
    Db          *db;
    CDbItem     *item;
    cchar       *error;

    db = dbOpen("./db/schema-required.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Try to create User without required fields
    item = dbCreate(db, "User", DB_PROPS("username", "test"), NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    //  Try to create User without email (required)
    item = dbCreate(db, "User", DB_PROPS(
        "username", "test",
        "role", "user"
    ), NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    //  Create User with all required fields
    item = dbCreate(db, "User", DB_PROPS(
        "username", "validuser",
        "email", "valid@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    tmatch(dbGetError(db), NULL);

    //  Try to create Event without required fields
    item = dbCreate(db, "Event", DB_PROPS("message", "test"), NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    //  Create Event with all required fields
    item = dbCreate(db, "Event", DB_PROPS(
        "message", "Valid event",
        "source", "test",
        "severity", "info",
        "subject", "Testing"
    ), NULL);
    tnotnull(item);

    dbClose(db);
}

static void testUniqueFields()
{
    Db          *db;
    CDbItem     *item;

    db = dbOpen("./db/schema-unique.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Create first user with unique username
    item = dbCreate(db, "User", DB_PROPS(
        "username", "unique",
        "email", "unique1@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);

#if FUTURE
    // The unique constraint is not implemented yet
    //  Try to create second user with same username (should fail due to unique constraint)
    item = dbCreate(db, "User", DB_PROPS(
        "username", "unique",
        "email", "unique2@test.com",
        "role", "admin"
    ), NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);
#endif

    //  Create VLAN with unique name
    item = dbCreate(db, "Vlan", DB_PROPS(
        "name", "unique-vlan",
        "description", "Test VLAN"
    ), NULL);
    tnotnull(item);

#if FUTURE
    //  Try to create second VLAN with same name (should fail)
    item = dbCreate(db, "Vlan", DB_PROPS(
        "name", "unique-vlan",
        "description", "Another VLAN"
    ), NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);
#endif

    dbClose(db);
}

static void testFieldTypes()
{
    Db          *db;
    CDbItem     *item;
    cchar       *value;

    db = dbOpen("./db/schema-types.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Create item with various field types
    item = dbCreate(db, "Port", DB_PROPS(
        "name", "eth0",
        "enable", "true",
        "speed", "1000",
        "negotiate", "false"
    ), NULL);
    tnotnull(item);

    //  Verify string field
    value = dbField(item, "name");
    tmatch(value, "eth0");

    //  Verify boolean fields
    ttrue(dbFieldBool(item, "enable"));
    tfalse(dbFieldBool(item, "negotiate"));

    //  Verify numeric field
    teqll(dbFieldNumber(item, "speed"), 1000);

    dbClose(db);
}

static void testGeneratedFields()
{
    Db          *db;
    CDbItem     *item;
    cchar       *id1, *id2;

    db = dbOpen("./db/schema-generated.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Create items with generated ID fields
    item = dbCreate(db, "User", DB_PROPS(
        "username", "user1",
        "email", "user1@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    id1 = dbField(item, "id");
    tnotnull(id1);

    item = dbCreate(db, "User", DB_PROPS(
        "username", "user2",
        "email", "user2@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    id2 = dbField(item, "id");
    tnotnull(id2);

    //  Generated IDs should be different
    tfalse(smatch(id1, id2));

    //  Test ULID generation for Event
    item = dbCreate(db, "Event", DB_PROPS(
        "message", "Test event",
        "source", "test",
        "severity", "info",
        "subject", "Testing"
    ), NULL);
    tnotnull(item);
    id1 = dbField(item, "id");
    tnotnull(id1);
    teqz(strlen(id1), 26);  // ULID length

    dbClose(db);
}

static void testModelValidation()
{
    Db          *db;
    CDbItem     *item;
    cchar       *error;

    db = dbOpen("./db/schema-model.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Try to create item with unknown model
    item = dbCreate(db, "UnknownModel", DB_PROPS("name", "test"), NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    //  Try to create item with unknown field (should be ignored or fail)
    item = dbCreate(db, "User", DB_PROPS(
        "username", "test",
        "email", "test@example.com",
        "role", "user",
        "unknownField", "value"
    ), NULL);
    //  This might succeed but unknown field should be ignored
    if (item) {
        cchar *unknown = dbField(item, "unknownField");
        tnull(unknown);  // Unknown field should not be stored
    }

    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testRequiredFields();
    testUniqueFields();
    testFieldTypes();
    testGeneratedFields();
    testModelValidation();

    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */