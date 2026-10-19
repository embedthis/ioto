/*
    model-isolation.tst.c - Schema keys and model boundaries
 */
#include "testme.h"
#include "db.h"

static void testKeys(Db *db)
{
    CDbItem *item;
    RList   *items;
    cchar   *value;
    int     count;

    item = dbCreate(db, "User", DB_PROPS("id", "alice", "username", "alice", "email", "a@test", "role", "user"), NULL);
    tnotnull(item);
    item = dbCreate(db, "Vlan", DB_PROPS("id", "v1", "name", "vlan"), NULL);
    tnotnull(item);
    item = dbGet(db, "User", DB_PROPS("sk", "vlan#v1"), NULL);
    tnull(item);
    item = dbUpdate(db, "User", DB_PROPS("sk", "user#alice", "role", "super"), NULL);
    tnull(item);
    item = dbCreate(db, "User",
        DB_PROPS("id", "forged", "sk", "vlan#forged", "username", "x", "email", "x@test"), NULL);
    tnull(item);
    count = dbRemove(db, "User", DB_PROPS("sk", "vlan#v1"), NULL);
    ttrue(count <= 0);
    items = dbFind(db, "User", DB_PROPS("sk", "vlan#v1"), NULL);
    ttrue(!items || rGetListLength(items) == 0);
    rFreeList(items);
    item = dbCreate(db, "Item", DB_PROPS("id", "zz#admin"), NULL);
    tnull(item);
    item = dbUpdate(db, "User", DB_PROPS("id", "alice", "_type", "Vlan"), NULL);
    tnull(item);
    item = dbSetField(db, "User", "_type", "Vlan", DB_PROPS("id", "alice"), NULL);
    tnull(item);
    item = dbSetField(db, "User", "sk", "vlan#v1", DB_PROPS("id", "alice"), NULL);
    tnull(item);
    item = dbGet(db, "User", DB_PROPS("id", "alice"), NULL);
    tnotnull(item);
    value = dbField(item, "role");
    tmatch(value, "user");
    item = dbUpdate(db, "User", DB_PROPS("id", "alice", "sk", "user#alice", "role", "guest"), NULL);
    tnotnull(item);
    value = dbField(item, "role");
    tmatch(value, "guest");
}

/* Simulate a legacy record whose stored type disagrees with its key prefix. */
static void testStoredModel(Db *db)
{
    CDbItem *item;
    Json    *json;
    RList   *items;
    cchar   *value;
    int     count;

    item = dbCreate(db, "Item", DB_PROPS("id", "legacy"), NULL);
    tnotnull(item);
    json = (Json*) dbJson(item);
    jsonSet(json, 0, "_type", "Vlan", JSON_STRING);
    item = dbGet(db, "Item", DB_PROPS("id", "legacy"), NULL);
    tnull(item);
    value = dbGetField(db, "Item", "id", DB_PROPS("id", "legacy"), NULL);
    tnull(value);
    item = dbFindOne(db, "Item", DB_PROPS("id", "legacy"), NULL);
    tnull(item);
    items = dbFind(db, "Item", NULL, NULL);
    teqi(rGetListLength(items), 0);
    rFreeList(items);
    item = dbUpdate(db, "Item", DB_PROPS("id", "legacy"), NULL);
    tnull(item);
    item = dbUpdate(db, "Item", DB_PROPS("id", "legacy"), DB_PARAMS(.upsert = 1));
    tnull(item);
    item = dbCreate(db, "Item", DB_PROPS("id", "legacy"), DB_PARAMS(.upsert = 1));
    tnull(item);
    item = dbSetField(db, "Item", "id", "legacy", DB_PROPS("id", "legacy"), DB_PARAMS(.upsert = 1));
    tnull(item);
    count = dbRemove(db, "Item", DB_PROPS("id", "legacy"), NULL);
    teqi(count, 0);
}

static void testRawKeys(void)
{
    Db      *db;
    CDbItem *item;
    RList   *items;
    cchar   *value;
    char    path[128];
    int     count;

    rSnprintf(path, sizeof(path), ".test/raw-model-isolation-%d.db", getpid());
    db = dbOpen(path, "model-isolation.json5", DB_OPEN_RESET);
    tnotnull(db);
    item = dbCreate(db, "RawB", DB_PROPS("key", "shared", "value", "original"), NULL);
    tnotnull(item);
    item = dbGet(db, "RawA", DB_PROPS("key", "shared"), NULL);
    tnull(item);
    value = dbGetField(db, "RawA", "value", DB_PROPS("key", "shared"), NULL);
    tnull(value);
    item = dbFindOne(db, "RawA", DB_PROPS("key", "shared"), NULL);
    tnull(item);
    items = dbFind(db, "RawA", DB_PROPS("key", "shared"), NULL);
    teqi(rGetListLength(items), 0);
    rFreeList(items);
    item = dbUpdate(db, "RawA", DB_PROPS("key", "shared", "value", "changed"), NULL);
    tnull(item);
    item = dbUpdate(db, "RawA", DB_PROPS("key", "shared"), DB_PARAMS(.upsert = 1));
    tnull(item);
    item = dbCreate(db, "RawA", DB_PROPS("key", "shared"), DB_PARAMS(.upsert = 1));
    tnull(item);
    item = dbSetField(db, "RawA", "value", "changed", DB_PROPS("key", "shared"), NULL);
    tnull(item);
    count = dbRemove(db, "RawA", DB_PROPS("key", "shared"), NULL);
    teqi(count, 0);
    item = dbGet(db, "RawB", DB_PROPS("key", "shared"), NULL);
    tnotnull(item);
    value = dbField(item, "value");
    tmatch(value, "original");
    //  Cloud hash fields are deliberately stripped by this local sort-key-only database.
    item = dbCreate(db, "Scoped", DB_PROPS("id", "one", "account", "a", "partition", "account#b"), NULL);
    tnotnull(item);
    value = dbField(item, "key");
    tmatch(value, "item#one");
    value = dbField(item, "partition");
    tnull(value);
    dbClose(db);
    dbReset(path);
}

int main(void)
{
    Db   *db;
    char path[128];

    rInit(0, 0);
    mkdir(".test", 0755);
    rSnprintf(path, sizeof(path), ".test/model-isolation-%d.db", getpid());
    db = dbOpen(path, "schema.json", DB_OPEN_RESET);
    tnotnull(db);
    testKeys(db);
    testStoredModel(db);
    testRawKeys();
    dbClose(db);
    dbReset(path);
    rTerm();
    return 0;
}
