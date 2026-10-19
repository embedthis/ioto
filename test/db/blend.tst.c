/*
    blend.c.tst - Unit tests for blended schemas, model processing and generated ids

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/

/*
    Models from the blended include are available alongside the models in the main schema
 */
static void testBlendedModels()
{
    Db      *db;
    CDbItem *item;

    db = dbOpen("./db/blend.db", "./blend.json5", DB_OPEN_RESET);
    tnotnull(db);
    tmatch(dbGetError(db), NULL);

    //  Declared in the main schema
    tnotnull(dbGetModel(db, "Local"));

    //  Declared in the blended include
    tnotnull(dbGetModel(db, "Device"));
    tnotnull(dbGetModel(db, "Token"));
    tnotnull(dbGetModel(db, "Reading"));

    //  A model marked as cloud only is not loaded
    tnull(dbGetModel(db, "Cached"));

    item = dbCreate(db, "Local", DB_PROPS("name", "loopback"), NULL);
    tnotnull(item);
    teqz(slen(dbField(item, "id")), 26);

    dbClose(db);
}

/*
    The generate directive supports ulid, uid and a sized uid
 */
static void testGeneratedIds()
{
    Db      *db;
    CDbItem *item;

    db = dbOpen("./db/blend-ids.db", "./blend.json5", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "Device", DB_PROPS("name", "sensor-1"), NULL);
    tnotnull(item);
    teqz(slen(dbField(item, "id")), 10);

    item = dbCreate(db, "Token", DB_PROPS("label", "session"), NULL);
    tnotnull(item);
    teqz(slen(dbField(item, "id")), 16);

    dbClose(db);
}

/*
    A model with created and updated fields has both stamped on create
 */
static void testTimestampFields()
{
    Db      *db;
    CDbItem *item;
    cchar   *created, *id;

    db = dbOpen("./db/blend-stamps.db", "./blend.json5", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "Device", DB_PROPS("name", "sensor-2"), NULL);
    tnotnull(item);
    tnotnull(dbField(item, "created"));
    tnotnull(dbField(item, "updated"));
    tmatch(dbField(item, "created"), dbField(item, "updated"));

    id = dbField(item, "id");
    created = sclone(dbField(item, "created"));
    rSleep(20);

    //  An update refreshes updated but leaves created alone
    item = dbUpdate(db, "Device", DB_PROPS("id", id, "name", "sensor-2b"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "created"), created);
    rFree((char*) created);

    dbClose(db);
}

/*
    A model default supplies a value only when the caller omits the property
 */
static void testModelDefaults()
{
    Db      *db;
    CDbItem *item;

    db = dbOpen("./db/blend-defaults.db", "./blend.json5", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "Device", DB_PROPS("name", "sensor-3"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "kind"), "sensor");

    item = dbCreate(db, "Device", DB_PROPS("name", "relay-1", "kind", "relay"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "kind"), "relay");

    item = dbCreate(db, "Device", DB_PROPS("name", "bad-1", "kind", "switch"), NULL);
    tnull(item);
    tnotnull(dbGetError(db));

    dbClose(db);
}

/*
    A process delay declared in the schema defers the journal write
 */
static void testModelDelay()
{
    Db      *db;
    CDbItem *item;
    ssize   before, after;

    db = dbOpen("./db/blend-delay.db", "./blend.json5", DB_OPEN_RESET);
    tnotnull(db);
    before = rGetFileSize("./db/blend-delay.db.jnl");

    //  The Device model declares a one second delay
    item = dbCreate(db, "Device", DB_PROPS("name", "slow-1"), NULL);
    tnotnull(item);
    teqll(rGetFileSize("./db/blend-delay.db.jnl"), before);

    rSleep(1100);
    rRunEvents();
    after = rGetFileSize("./db/blend-delay.db.jnl");
    tgtll(after, before);

    //  The Token model is memory only and is never journalled
    before = after;
    item = dbCreate(db, "Token", DB_PROPS("label", "mem-only"), NULL);
    tnotnull(item);
    teqll(rGetFileSize("./db/blend-delay.db.jnl"), before);

    //  The Reading model declares a zero period, so it is written immediately
    item = dbCreate(db, "Reading", DB_PROPS("value", "12"), NULL);
    tnotnull(item);
    tgtll(rGetFileSize("./db/blend-delay.db.jnl"), before);

    dbClose(db);
}

/*
    Array and object defaults are stored with their declared json type
 */
static void testStructuredDefaults()
{
    Db      *db;
    CDbItem *item;
    cchar   *str;

    db = dbOpen("./db/blend-structured.db", "./blend.json5", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "Defaults", NULL, NULL);
    tnotnull(item);
    tmatch(dbField(item, "label"), "none");

    //  The array and object defaults are not quoted as strings
    str = dbString(item, 0);
    tcontains(str, "tags:[]");
    tcontains(str, "net:{}");

    dbClose(db);
}

/*
    The index hash property is reserved for the cloud side and is ignored here
 */
static void testIndexHashProperty()
{
    Db      *db;
    CDbItem *item;

    db = dbOpen("./db/blend-hash.db", "./blend.json5", DB_OPEN_RESET);
    tnotnull(db);

    //  The primary index declares "pk" as its hash, so a supplied pk is accepted and dropped
    item = dbCreate(db, "Config", DB_PROPS("id", "h1", "pk", "tenant#1", "label", "hashed"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "label"), "hashed");
    tnull(dbField(item, "pk"));

    dbClose(db);
}

/*
    A model with no sort key template cannot resolve a key for keyed operations
 */
static void testMissingSortKey()
{
    Db      *db;
    CDbItem *item;
    RList   *items;

    db = dbOpen("./db/blend-loose.db", "./blend.json5", DB_OPEN_RESET);
    tnotnull(db);

    item = dbGet(db, "Loose", DB_PROPS("id", "x1"), NULL);
    tnull(item);
    tcontains(dbGetError(db), "Missing sort key");

    item = dbCreate(db, "Loose", DB_PROPS("id", "x1", "name", "nameless"), NULL);
    tnull(item);
    tcontains(dbGetError(db), "Missing sort key");

    //  Supplying the sort key explicitly works
    item = dbCreate(db, "Loose", DB_PROPS("sk", "loose#x1", "id", "x1", "name", "named"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "name"), "named");

    //  Find does not need the key
    items = dbFind(db, "Loose", NULL, NULL);
    teqi(rGetListLength(items), 1);
    rFreeList(items);

    dbClose(db);
}

/*
    Blend errors are reported rather than producing a half loaded schema
 */
static void testBlendErrors()
{
    Db *db;

    db = dbOpen("./db/blend-missing.db", "./blend-missing.json5", DB_OPEN_RESET);
    tnull(db);
    db = dbOpen("./db/blend-broken.db", "./blend-broken.json5", DB_OPEN_RESET);
    tnull(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testBlendedModels();
    testGeneratedIds();
    testTimestampFields();
    testModelDefaults();
    testModelDelay();
    testStructuredDefaults();
    testIndexHashProperty();
    testMissingSortKey();
    testBlendErrors();

    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
