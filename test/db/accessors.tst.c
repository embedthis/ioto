/*
    accessors.c.tst - Unit tests for the typed getters, setters and item accessors

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"
#include    "json.h"

/************************************ Code ************************************/

static Db *createDb(cchar *path)
{
    Db      *db;
    CDbItem *item;

    db = dbOpen(path, "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "Port", DB_PROPS(
        "id", "port-1",
        "name", "eth0",
        "speed", "2500",
        "rxBytes", "1024",
        "enable", "true"
    ), NULL);
    tnotnull(item);
    return db;
}

/*
    The typed getters return the stored value when the item and field exist
 */
static void testTypedGetters()
{
    Db      *db;
    CDbItem *item;
    Time    when;

    db = createDb("./db/accessors-get.db");

    teqll(dbGetNum(db, "Port", "speed", DB_PROPS("id", "port-1"), NULL, -1), 2500);
    ttrue(dbGetBool(db, "Port", "enable", DB_PROPS("id", "port-1"), NULL, false));
    ttrue(dbGetBool(db, "Port", "jumbo", DB_PROPS("id", "port-1"), NULL, false));
    ttrue(dbGetDouble(db, "Port", "rxBytes", DB_PROPS("id", "port-1"), NULL, -1.0) == 1024.0);
    tmatch(dbGetString(db, "Port", "name", DB_PROPS("id", "port-1"), NULL, "none"), "eth0");
    tmatch(dbGetField(db, "Port", "duplex", DB_PROPS("id", "port-1"), NULL), "full");

    //  A date field round trips through the setter and the getter
    item = dbCreate(db, "Settings", NULL, NULL);
    tnotnull(item);
    when = rGetTime();
    item = dbSetDate(db, "Settings", "timestamp", when, NULL, NULL);
    tnotnull(item);
    teqll(dbGetDate(db, "Settings", "timestamp", NULL, NULL, 0), when);

    dbClose(db);
}

/*
    The typed getters fall back to the supplied default when the item is missing
 */
static void testGetterDefaults()
{
    Db *db;

    db = createDb("./db/accessors-defaults.db");

    teqll(dbGetNum(db, "Port", "speed", DB_PROPS("id", "absent"), NULL, 99), 99);
    ttrue(dbGetBool(db, "Port", "enable", DB_PROPS("id", "absent"), NULL, true));
    ttrue(dbGetDouble(db, "Port", "rxBytes", DB_PROPS("id", "absent"), NULL, 3.5) == 3.5);
    teqll(dbGetDate(db, "Settings", "timestamp", NULL, NULL, 1234), 1234);
    tmatch(dbGetString(db, "Port", "name", DB_PROPS("id", "absent"), NULL, "fallback"), "fallback");

    //  A field that does not exist on a found item also yields the default
    teqll(dbGetNum(db, "Port", "missingField", DB_PROPS("id", "port-1"), NULL, 7), 7);

    //  Null arguments are tolerated
    tnull(dbGetField(NULL, "Port", "name", NULL, NULL));
    tnull(dbGetField(db, NULL, "name", NULL, NULL));
    tnull(dbGetField(db, "Port", NULL, NULL, NULL));

    dbClose(db);
}

/*
    The typed setters write through and are readable as the same type
 */
static void testTypedSetters()
{
    Db      *db;
    CDbItem *item;
    Time    when;

    db = createDb("./db/accessors-set.db");

    item = dbSetNum(db, "Port", "txBytes", 4096, DB_PROPS("id", "port-1"), NULL);
    tnotnull(item);
    teqll(dbFieldNumber(item, "txBytes"), 4096);

    item = dbSetBool(db, "Port", "jumbo", false, DB_PROPS("id", "port-1"), NULL);
    tnotnull(item);
    tfalse(dbFieldBool(item, "jumbo"));

    item = dbSetDouble(db, "Port", "rxBytes", 2.5, DB_PROPS("id", "port-1"), NULL);
    tnotnull(item);
    ttrue(dbFieldDouble(item, "rxBytes") == 2.5);

    item = dbSetString(db, "Port", "name", "eth9", DB_PROPS("id", "port-1"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "name"), "eth9");

    item = dbCreate(db, "Settings", NULL, NULL);
    tnotnull(item);
    when = rGetTime();
    item = dbSetDate(db, "Settings", "timestamp", when, NULL, NULL);
    tnotnull(item);
    teqll(dbFieldDate(item, "timestamp"), when);

    //  A null value removes the property
    item = dbSetField(db, "Port", "name", NULL, DB_PROPS("id", "port-1"), NULL);
    tnotnull(item);
    tnull(dbField(item, "name"));

    //  Setting a field on a missing item fails unless upsert is requested
    item = dbSetField(db, "Port", "name", "eth7", DB_PROPS("id", "absent"), NULL);
    tnull(item);
    tnotnull(dbGetError(db));

    item = dbSetField(db, "Port", "name", "eth7", DB_PROPS("id", "upserted"),
                      DB_PARAMS(.upsert = 1));
    tnotnull(item);
    tmatch(dbField(item, "name"), "eth7");

    //  Null arguments are tolerated
    item = dbSetField(NULL, "Port", "name", "x", NULL, NULL);
    tnull(item);
    item = dbSetField(db, NULL, "name", "x", NULL, NULL);
    tnull(item);
    item = dbSetField(db, "Port", NULL, "x", NULL, NULL);
    tnull(item);

    dbClose(db);
}

/*
    Item level accessors expose the key, the json and the string representation
 */
static void testItemAccessors()
{
    Db          *db;
    CDbItem     *item;
    const Json  *json;
    cchar       *str;

    db = createDb("./db/accessors-item.db");

    item = dbGet(db, "Port", DB_PROPS("id", "port-1"), NULL);
    tnotnull(item);

    json = dbJson(item);
    tnotnull(json);
    tmatch(jsonGet((Json*) json, 0, "name", 0), "eth0");
    tnull(dbJson(NULL));

    //  dbString serializes the parsed json back to text
    str = dbString(item, 0);
    tnotnull(str);
    tcontains(str, "eth0");

    str = dbString(item, JSON_HUMAN);
    tnotnull(str);
    tcontains(str, "name: 'eth0'");

    //  Field accessors tolerate a null item or field name
    tnull(dbField(NULL, "name"));
    tnull(dbField(item, NULL));

    //  The model of an item can be recovered from the item itself
    tnotnull(dbGetModel(db, "Port"));
    tnull(dbGetModel(db, "NoSuchModel"));
    tnotnull(dbGetItemModel(db, item));
    tmatch(dbGetItemModel(db, item)->name, "Port");

    dbClose(db);
}

/*
    Database level accessors report the schema type field and sort key
 */
static void testDbAccessors()
{
    Db    *db;
    RList *items;
    char  *str, *uid, *ulid;

    db = createDb("./db/accessors-db.db");

    tmatch(dbType(db), "_type");
    tmatch(dbGetSortKey(db), "sk");

    //  A list of items can be rendered as a json array
    items = dbFind(db, "Port", NULL, NULL);
    tnotnull(items);
    str = dbListToString(items);
    tnotnull(str);
    tcontains(str, "eth0");
    ttrue(str[0] == '[');
    rFree(str);

    //  The pagination token is the sort key of the last item
    tmatch(dbNext(db, items), "port#port-1");
    rFreeList(items);

    tnull(dbListToString(NULL));

    //  Generated identifiers are unique and correctly sized
    uid = dbGetUID(10);
    tnotnull(uid);
    teqz(slen(uid), 10);
    rFree(uid);

    ulid = dbGetULID(rGetTime());
    tnotnull(ulid);
    teqz(slen(ulid), 26);
    rFree(ulid);

    dbClose(db);
}

/*
    Global context properties are applied to every operation and take precedence
 */
static void testContext()
{
    Db      *db;
    CDbItem *item;

    db = dbOpen("./db/accessors-context.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    dbAddContext(db, "role", "super");

    //  The context value overrides the property supplied by the caller
    item = dbCreate(db, "User", DB_PROPS(
        "username", "ctx",
        "email", "ctx@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    tmatch(dbField(item, "role"), "super");

    //  Context properties that are not model fields are dropped
    dbAddContext(db, "notAField", "ignored");
    item = dbCreate(db, "User", DB_PROPS(
        "username", "ctx2",
        "email", "ctx2@test.com"
    ), NULL);
    tnotnull(item);
    tnull(dbField(item, "notAField"));

    dbClose(db);
}

/*
    Opening with bad arguments returns no database rather than crashing
 */
static void testOpenErrors()
{
    Db      *db;
    CDbItem *item;

    //  Errors are reported for a null database rather than crashing
    tnull(dbGetError(NULL));

    db = createDb("./db/accessors-errors.db");

    //  Update needs properties to locate the item
    item = dbUpdate(db, "Port", NULL, NULL);
    tnull(item);
    tcontains(dbGetError(db), "bad properties");
    dbClose(db);

    db = dbOpen(NULL, "./schema.json", 0);
    tnull(db);
    db = dbOpen("", "./schema.json", 0);
    tnull(db);
    db = dbOpen("./db/accessors-bad.db", NULL, 0);
    tnull(db);
    db = dbOpen("./db/accessors-bad.db", "", 0);
    tnull(db);
    db = dbOpen("./db/accessors-bad.db", "./no-such-schema.json", 0);
    tnull(db);

    //  Closing a null database is a no-op
    dbClose(NULL);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testTypedGetters();
    testGetterDefaults();
    testTypedSetters();
    testItemAccessors();
    testDbAccessors();
    testContext();
    testOpenErrors();

    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
