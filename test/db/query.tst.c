/*
    query.c.tst - Unit tests for find filtering, pagination and nested property matching

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"
#include    "json.h"

/************************************ Code ************************************/

static Db *seedConfigs(cchar *path, int count)
{
    Db      *db;
    CDbItem *item;
    int     i;

    db = dbOpen(path, "./blend.json5", DB_OPEN_RESET);
    tnotnull(db);

    for (i = 0; i < count; i++) {
        item = dbCreate(db, "Config", DB_JSON(
            "{id: 'c%02d', label: '%s', net: {host: 'h%02d', port: '80'}, tags: ['a', 'b']}",
            i, (i % 2) ? "odd" : "even", i), NULL);
        tnotnull(item);
    }
    return db;
}

/*
    Paging through a model steps forward from the supplied token
 */
static void testModelPagination()
{
    Db      *db;
    RList   *page;
    CDbItem *item;
    cchar   *next;
    int     total, pages;

    db = seedConfigs("./db/query-page.db", 10);

    total = 0;
    pages = 0;
    next = NULL;
    do {
        page = dbFind(db, "Config", NULL, DB_PARAMS(.limit = 3, .next = next));
        tnotnull(page);
        total += rGetListLength(page);
        next = dbNext(db, page);
        if (rGetListLength(page) < 3) {
            next = NULL;
        }
        rFreeList(page);
    } while (next && ++pages < 10);

    teqi(total, 10);

    //  A token that matches no item yields an empty page
    page = dbFind(db, "Config", NULL, DB_PARAMS(.limit = 3, .next = "config#zz"));
    tnotnull(page);
    teqi(rGetListLength(page), 0);
    rFreeList(page);

    //  dbNext of an empty list has no token
    page = dbFind(db, "Config", DB_PROPS("id", "absent"), NULL);
    tnull(dbNext(db, page));
    rFreeList(page);

    item = dbGet(db, "Config", DB_PROPS("id", "c00"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "label"), "even");

    dbClose(db);
}

static int matchOdd(Json *data, int nid, cchar *arg)
{
    tmatch(arg, "odd");
    return smatch(jsonGet(data, nid, "label", 0), arg);
}

/*
    A where callback filters results and cooperates with the limit
 */
static void testWhereAndLimit()
{
    Db      *db;
    CDbItem *item;
    RList   *items;
    int     count;

    db = seedConfigs("./db/query-where.db", 10);

    items = dbFind(db, "Config", NULL, DB_PARAMS(.where = (DbWhere) matchOdd, .arg = "odd"));
    tnotnull(items);
    teqi(rGetListLength(items), 5);
    rFreeList(items);

    items = dbFind(db, "Config", NULL,
                   DB_PARAMS(.where = (DbWhere) matchOdd, .arg = "odd", .limit = 2));
    teqi(rGetListLength(items), 2);
    rFreeList(items);

    //  A where callback also applies to findOne and to remove
    item = dbFindOne(db, "Config", NULL, DB_PARAMS(.where = (DbWhere) matchOdd, .arg = "odd"));
    tnotnull(item);

    count = dbRemove(db, "Config", DB_PROPS("label", "odd"),
                     DB_PARAMS(.where = (DbWhere) matchOdd, .arg = "odd", .limit = 100));
    teqi(count, 5);

    items = dbFind(db, "Config", NULL, NULL);
    teqi(rGetListLength(items), 5);
    rFreeList(items);

    dbClose(db);
}

/*
    Nested objects and arrays in the search properties are matched recursively
 */
static void testNestedMatching()
{
    Db      *db;
    CDbItem *item;
    RList   *items;
    cchar   *str;

    db = seedConfigs("./db/query-nested.db", 4);

    //  A matching nested object selects the item
    item = dbFindOne(db, "Config", DB_JSON("{net: {host: 'h02', port: '80'}}"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "id"), "c02");

    //  A nested value that differs matches nothing
    item = dbFindOne(db, "Config", DB_JSON("{net: {host: 'h02', port: '99'}}"), NULL);
    tnull(item);

    //  A nested property that the item does not have matches nothing
    item = dbFindOne(db, "Config", DB_JSON("{net: {missing: 'x'}}"), NULL);
    tnull(item);

    //  An array property is stored and read back intact
    item = dbFindOne(db, "Config", DB_JSON("{id: 'c01'}"), NULL);
    tnotnull(item);
    str = dbString(item, 0);
    tcontains(str, "tags:['a','b']");

    //  Arrays are matched element by element, in order
    items = dbFind(db, "Config", DB_JSON("{tags: ['a', 'b']}"), NULL);
    teqi(rGetListLength(items), 4);
    rFreeList(items);

    //  A differing element does not match
    items = dbFind(db, "Config", DB_JSON("{tags: ['a', 'z']}"), NULL);
    teqi(rGetListLength(items), 0);
    rFreeList(items);

    //  Order matters
    items = dbFind(db, "Config", DB_JSON("{tags: ['b', 'a']}"), NULL);
    teqi(rGetListLength(items), 0);
    rFreeList(items);

    //  A shorter or longer array does not match
    items = dbFind(db, "Config", DB_JSON("{tags: ['a']}"), NULL);
    teqi(rGetListLength(items), 0);
    rFreeList(items);

    items = dbFind(db, "Config", DB_JSON("{tags: ['a', 'b', 'c']}"), NULL);
    teqi(rGetListLength(items), 0);
    rFreeList(items);

    //  A scalar does not match an array valued field
    items = dbFind(db, "Config", DB_JSON("{tags: 'a'}"), NULL);
    teqi(rGetListLength(items), 0);
    rFreeList(items);

    dbClose(db);
}

static int rejectAll(Json *data, int nid, cchar *arg)
{
    return 0;
}

/*
    A where callback that rejects the keyed item blocks the update
 */
static void testWhereBlocksUpdate()
{
    Db      *db;
    CDbItem *item;

    db = seedConfigs("./db/query-reject.db", 4);

    item = dbUpdate(db, "Config", DB_PROPS("id", "c01", "label", "changed"),
                    DB_PARAMS(.where = (DbWhere) rejectAll));
    tnull(item);
    tnotnull(dbGetError(db));

    item = dbSetField(db, "Config", "label", "changed", DB_PROPS("id", "c01"),
                      DB_PARAMS(.where = (DbWhere) rejectAll));
    tnull(item);
    tnotnull(dbGetError(db));

    //  The item is unchanged
    item = dbGet(db, "Config", DB_PROPS("id", "c01"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "label"), "odd");

    dbClose(db);
}

/*
    A find with a limit stops as soon as the limit is reached
 */
static void testFindLimit()
{
    Db      *db;
    CDbItem *item;
    RList   *items;
    int     count;

    db = seedConfigs("./db/query-limit.db", 10);

    items = dbFind(db, "Config", NULL, DB_PARAMS(.limit = 1));
    teqi(rGetListLength(items), 1);
    rFreeList(items);

    items = dbFind(db, "Config", NULL, DB_PARAMS(.limit = 100));
    teqi(rGetListLength(items), 10);
    rFreeList(items);

    //  A find across all models returns every item
    items = dbFind(db, NULL, NULL, NULL);
    teqi(rGetListLength(items), 10);
    rFreeList(items);

    //  Finding an unknown model is an error, not an empty list
    items = dbFind(db, "NoSuchModel", NULL, NULL);
    tnull(items);
    tnotnull(dbGetError(db));

    //  Null database arguments are tolerated
    items = dbFind(NULL, "Config", NULL, NULL);
    tnull(items);
    item = dbFindOne(NULL, "Config", NULL, NULL);
    tnull(item);
    item = dbGet(NULL, "Config", NULL, NULL);
    tnull(item);
    count = dbRemove(NULL, "Config", NULL, NULL);
    tltei(count, 0);
    count = dbRemove(db, "Config", NULL, NULL);
    tltei(count, 0);

    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testModelPagination();
    testWhereAndLimit();
    testNestedMatching();
    testWhereBlocksUpdate();
    testFindLimit();

    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
