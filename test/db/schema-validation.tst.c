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

static void testRequiredOnUpdate()
{
    Db          *db;
    CDbItem     *item;
    cchar       *error, *id;

    db = dbOpen("./db/schema-required-update.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "User", DB_PROPS(
        "username", "reqtest",
        "email", "reqtest@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    id = sclone(dbField(item, "id"));

    item = dbUpdate(db, "User", DB_JSON("{id:'%s', username: undefined}", id), NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    item = dbGet(db, "User", DB_PROPS("id", id), NULL);
    tnotnull(item);
    tmatch(dbField(item, "username"), "reqtest");

    item = dbSetField(db, "User", "email", NULL, DB_PROPS("id", id), NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    item = dbGet(db, "User", DB_PROPS("id", id), NULL);
    tnotnull(item);
    tmatch(dbField(item, "email"), "reqtest@test.com");

    item = dbUpdate(db, "User", DB_PROPS(
        "id", id, "email", "reqtest@test.com", "role", "user"
    ), DB_PARAMS(.upsert = 1));
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    item = dbUpdate(db, "User", DB_PROPS("id", id, "role", "admin"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "role"), "admin");

    rFree((char*) id);
    dbClose(db);
}

static void testUniqueFields()
{
    Db          *db;
    CDbItem     *item;
    cchar       *error, *otherId, *uniqueId;

    db = dbOpen("./db/schema-unique.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Create first user with unique username
    item = dbCreate(db, "User", DB_PROPS(
        "username", "unique",
        "email", "unique1@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    uniqueId = sclone(dbField(item, "id"));

    //  Try to create second user with same username (should fail due to unique constraint)
    item = dbCreate(db, "User", DB_PROPS(
        "username", "unique",
        "email", "unique2@test.com",
        "role", "admin"
    ), NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    //  Create VLAN with unique name
    item = dbCreate(db, "Vlan", DB_PROPS(
        "name", "unique-vlan",
        "description", "Test VLAN"
    ), NULL);
    tnotnull(item);

    //  Try to create second VLAN with same name (should fail)
    item = dbCreate(db, "Vlan", DB_PROPS(
        "name", "unique-vlan",
        "description", "Another VLAN"
    ), NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    item = dbCreate(db, "User", DB_PROPS(
        "username", "other",
        "email", "other@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    otherId = sclone(dbField(item, "id"));

    item = dbUpdate(db, "User", DB_PROPS(
        "id", otherId,
        "username", "unique"
    ), NULL);
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    item = dbGet(db, "User", DB_PROPS("id", otherId), NULL);
    tnotnull(item);
    tmatch(dbField(item, "username"), "other");

    item = dbUpdate(db, "User", DB_PROPS(
        "id", otherId,
        "username", "other",
        "role", "admin"
    ), NULL);
    tnotnull(item);
    tmatch(dbField(item, "role"), "admin");

    item = dbUpdate(db, "User", DB_PROPS(
        "id", otherId,
        "username", "other",
        "email", "other@test.com",
        "role", "admin"
    ), DB_PARAMS(.upsert = 1));
    tnotnull(item);

    item = dbUpdate(db, "User", DB_PROPS(
        "id", otherId,
        "username", "unique",
        "email", "other@test.com",
        "role", "admin"
    ), DB_PARAMS(.upsert = 1));
    tnull(item);
    error = dbGetError(db);
    tnotnull(error);

    rFree((char*) otherId);
    rFree((char*) uniqueId);
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

/*
    Reject unsupported validate and crypt attributes; accept map with a warning.
 */
static void testUnsupportedAttributes()
{
    Db      *db;
    CDbItem *item;

    db = dbOpen("./db/schema-attr-validate.db", "./schema-attr-validate.json5", DB_OPEN_RESET);
    tnull(db);

    db = dbOpen("./db/schema-attr-crypt.db", "./schema-attr-crypt.json5", DB_OPEN_RESET);
    tnull(db);

    db = dbOpen("./db/schema-attr-map.db", "./schema-attr-map.json5", DB_OPEN_RESET);
    tnotnull(db);
    if (db) {
        item = dbCreate(db, "Item", DB_PROPS("id", "1", "alias", "first"), NULL);
        tnotnull(item);
        tmatch(dbField(item, "alias"), "first");
        dbClose(db);
    }
}


static void testUidGenerateSize()
{
    tnull(dbOpen("./db/schema-uid-negative.db", "./schema-uid-negative.json5", DB_OPEN_RESET));
    tnull(dbOpen("./db/schema-uid-zero.db", "./schema-uid-zero.json5", DB_OPEN_RESET));
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testRequiredFields();
    testRequiredOnUpdate();
    testUniqueFields();
    testFieldTypes();
    testGeneratedFields();
    testModelValidation();
    testUnsupportedAttributes();
    testUidGenerateSize();

    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */