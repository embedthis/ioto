/*
    template-key.tst.c - Regression tests for issue 10003
 */

#include "testme.h"
#include "db.h"

static void checkCount(Db *db, cchar *model, int expected)
{
    RList *items;

    items = dbFind(db, model, NULL, NULL);
    tnotnull(items);
    if (items) {
        teqi(items->length, expected);
    }
    rFreeList(items);
}

static void rejectSuppliedTokens(Db *db)
{
    cchar *keys[] = { "${", "${x", "v${", "vlan#${id}", "user#${id}" };
    RList *items;
    Json  *props;
    int   i, count;

    for (i = 0; i < (int) (sizeof(keys) / sizeof(keys[0])); i++) {
        props = jsonAlloc();
        jsonSet(props, 0, "sk", keys[i], JSON_STRING);
        items = dbFind(db, "User", props, NULL);
        tnull(items);
        tnotnull(dbGetError(db));
        rFreeList(items);
        count = dbRemove(db, "User", props, DB_PARAMS(.limit = 1000));
        teqi(count, R_ERR_BAD_ARGS);
        tnotnull(dbGetError(db));
        tmatch(jsonGet(props, 0, "sk", NULL), keys[i]);
        jsonFree(props);
        checkCount(db, NULL, 4);
    }
    //  Model-less calls must not enable token-driven prefix searches either.
    items = dbFind(db, NULL, DB_PROPS("sk", "${"), NULL);
    tnull(items);
    rFreeList(items);
    count = dbRemove(db, NULL, DB_PROPS("sk", "${x"), DB_PARAMS(.limit = 1000));
    teqi(count, R_ERR_BAD_ARGS);
    checkCount(db, NULL, 4);
}

static void rejectEmptyTemplatePrefix(Db *db)
{
    RList *items;
    int   count;

    items = dbFind(db, "Unprefixed", NULL, NULL);
    tnull(items);
    rFreeList(items);
    count = dbRemove(db, "Unprefixed", DB_JSON("{}"), DB_PARAMS(.limit = 1000));
    teqi(count, R_ERR_BAD_ARGS);
    checkCount(db, NULL, 4);
}

static void preserveTemplateQueries(Db *db)
{
    CDbItem *item;
    RList   *items;
    int     count;

    checkCount(db, "User", 2);
    checkCount(db, "Port", 1);
    checkCount(db, "Vlan", 1);
    items = dbFind(db, "User", DB_PROPS("id", "one", "sk", "user#one"), NULL);
    tnotnull(items);
    if (items) {
        teqi(items->length, 1);
        item = rGetItem(items, 0);
        tnotnull(item);
        if (item) {
            tmatch(item->key, "user#one");
        }
    }
    rFreeList(items);
    count = dbRemove(db, "User", DB_JSON("{}"), DB_PARAMS(.limit = 1000));
    teqi(count, 2);
    checkCount(db, NULL, 2);
    checkCount(db, "Port", 1);
    checkCount(db, "Vlan", 1);
}

int main(void)
{
    Db      *db;
    CDbItem *item;
    char    path[128];

    rInit(0, 0);
    mkdir(".test", 0755);
    sfmtbuf(path, sizeof(path), ".test/template-key-%d.db", getpid());
    db = dbOpen(path, "template-key.json5", DB_OPEN_RESET);
    tnotnull(db);
    if (db) {
        item = dbCreate(db, "User", DB_PROPS("id", "one"), NULL);
        tnotnull(item);
        item = dbCreate(db, "User", DB_PROPS("id", "two"), NULL);
        tnotnull(item);
        item = dbCreate(db, "Port", DB_PROPS("id", "one"), NULL);
        tnotnull(item);
        item = dbCreate(db, "Vlan", DB_PROPS("id", "one"), NULL);
        tnotnull(item);
        rejectSuppliedTokens(db);
        rejectEmptyTemplatePrefix(db);
        preserveTemplateQueries(db);
        dbClose(db);
    }
    unlink(path);
    sfmtbuf(path, sizeof(path), ".test/template-key-%d.db.jnl", getpid());
    unlink(path);
    rTerm();
    return 0;
}
