/*
    hash.tst.c - Unit tests for the Hash class

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/*********************************** Locals ***********************************/

#define HASH_COUNT 256                 /* Number of items to enter */

/************************************ Code ************************************/

static void createTable()
{
    RHash *table;

    table = rAllocHash(128, 0);
    tnotnull(table);
    rFreeHash(table);

    table = rAllocHash(0, 0);
    tnotnull(table);
    rFreeHash(table);

    table = rAllocHash(1, 0);
    tnotnull(table);
    rFreeHash(table);
}


static void isTableEmpty()
{
    RHash *table;

    table = rAllocHash(0, 0);
    tnotnull(table);

    teqz(rGetHashLength(table), 0);
    tnull(rLookupName(table, ""));
    rFreeHash(table);
}


static void inserAndRemoveHash()
{
    RHash *table;
    RName *sp;
    cchar *str;
    int   rc;

    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    tnotnull(table);

    /*
        Single insert
     */
    sp = rAddName(table, "Peter", "123 Madison Ave", 0);
    tnotnull(sp);

    sp = rLookupNameEntry(table, "Peter");
    tnotnull(sp);
    tnotnull(sp->name);
    tmatch(sp->name, "Peter");
    tnotnull(sp->value);
    tmatch(sp->value, "123 Madison Ave");

    /*
        Lookup
     */
    str = rLookupName(table, "Peter");
    tnotnull(str);
    tmatch(str, "123 Madison Ave");

    rc = rRemoveName(table, "Peter");
    teqi(rc, 0);

    teqz(rGetHashLength(table), 0);

    str = rLookupName(table, "Peter");
    tnull(str);

    rFreeHash(table);
}


static void hashScale()
{
    RHash *table;
    RName *sp;
    char  *str, *name, *address;
    int   i;

    table = rAllocHash(HASH_COUNT, R_DYNAMIC_NAME | R_DYNAMIC_VALUE);
    teqz(rGetHashLength(table), 0);

    /*
        All insers below will inser allocated strings. We must free before
        deleting the table.
     */
    for (i = 0; i < HASH_COUNT; i++) {
        name = sfmt("name.%d", i);
        address = sfmt("%d Park Ave", i);
        sp = rAddName(table, name, address, 0);
        tnotnull(sp);
    }
    teqz(rGetHashLength(table), HASH_COUNT);

    /*
        Check data entered into the hash
     */
    for (i = 0; i < HASH_COUNT; i++) {
        name = sfmt("name.%d", i);
        str = rLookupName(table, name);
        tnotnull(str);
        address = sfmt("%d Park Ave", i);
        tmatch(str, address);
    }
    rFreeHash(table);
}


static void iterateHash()
{
    RHash *table;
    RName *sp;
    cchar *where;
    char  *name, *address;
    int   count, i, check[HASH_COUNT];

    table = rAllocHash(HASH_COUNT, R_DYNAMIC_NAME | R_DYNAMIC_VALUE);

    memset(check, 0, sizeof(check));

    /*
        Fill the table
     */
    for (i = 0; i < HASH_COUNT; i++) {
        name = sfmt("Bit longer name.%d", i);
        address = sfmt("%d Park Ave", i);
        sp = rAddName(table, name, address, 0);
        tnotnull(sp);
    }
    teqz(rGetHashLength(table), HASH_COUNT);

    /*
        Check data entered into the table
     */
    count = 0;
    for (ITERATE_NAMES(table, sp)) {
        tnotnull(sp);
        where = sp->value;
        ttrue(isdigit((int) where[0]));
        i = atoi(where);
        check[i] = 1;
        count++;
    }
    teqi(count, HASH_COUNT);

    count = 0;
    for (i = 0; i < HASH_COUNT; i++) {
        if (check[i]) {
            count++;
        }
    }
    teqi(count, HASH_COUNT);
    rFreeHash(table);
}


/*
    The TestMe comparison macros expand their arguments twice, so every call below with a side
    effect is bound to a local before it is asserted.
 */

/*
    rAddName replaces an existing entry; rAddDuplicateName keeps both. The duplicate is added at
    the head of the bucket chain, so a lookup finds the most recent.
 */
static void duplicateNames()
{
    RHash *table;
    RName *np;
    cchar *value;
    int   count;

    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    tnotnull(table);

    np = rAddName(table, "colour", "red", 0);
    tnotnull(np);
    np = rAddName(table, "colour", "green", 0);
    tnotnull(np);
    teqi(rGetHashLength(table), 1);
    value = rLookupName(table, "colour");
    tmatch(value, "green");

    np = rAddDuplicateName(table, "colour", "blue", 0);
    tnotnull(np);
    teqi(rGetHashLength(table), 2);
    value = rLookupName(table, "colour");
    tmatch(value, "blue");

    /*
        Both entries are still reachable by iteration.
     */
    count = 0;
    for (np = rGetNextName(table, NULL); np; np = rGetNextName(table, np)) {
        if (smatch(np->name, "colour")) {
            count++;
        }
    }
    teqi(count, 2);

    /*
        Removing the duplicate uncovers the original rather than deleting both.
     */
    count = rRemoveName(table, "colour");
    teqi(count, 0);
    teqi(rGetHashLength(table), 1);
    value = rLookupName(table, "colour");
    tmatch(value, "green");

    np = rAddDuplicateName(NULL, "x", "y", 0);
    tnull(np);
    np = rAddDuplicateName(table, NULL, "y", 0);
    tnull(np);

    /*
        Enough duplicates to force the bucket and name arrays to grow.
     */
    for (count = 0; count < 64; count++) {
        np = rAddDuplicateName(table, "colour", "many", 0);
        tnotnull(np);
    }
    teqi(rGetHashLength(table), 65);

    rFreeHash(table);
}


/*
    The convenience constructors. Each owns the memory it allocates, so the hash must free it.
 */
static void addNameVariants()
{
    RHash *table;
    RName *np;
    cchar *value;

    table = rAllocHash(0, 0);
    tnotnull(table);

    np = rAddNameSubstring(table, "keyboard", 3, "valueXXXX", 5);
    tnotnull(np);
    value = rLookupName(table, "key");
    tmatch(value, "value");
    value = rLookupName(table, "keyboard");
    tnull(value);

    np = rAddFmtName(table, "port", 0, "%d", 8080);
    tnotnull(np);
    value = rLookupName(table, "port");
    tmatch(value, "8080");

    np = rAddFmtName(table, "greeting", R_STATIC_NAME, "%s %s", "hello", "world");
    tnotnull(np);
    value = rLookupName(table, "greeting");
    tmatch(value, "hello world");

    np = rAddIntName(table, "count", 42);
    tnotnull(np);
    value = rLookupName(table, "count");
    tmatch(value, "42");

    np = rAddIntName(table, "negative", -7);
    tnotnull(np);
    value = rLookupName(table, "negative");
    tmatch(value, "-7");

    rFreeHash(table);
}


/*
    rIncName keeps the counter in the value pointer itself and returns the value from before the
    increment, so the first call on an absent name reports zero.
 */
static void incrementNames()
{
    RHash *table;
    ssize prior;

    table = rAllocHash(0, R_STATIC_NAME);

    prior = rIncName(table, "hits", 1);
    teqz(prior, 0);
    prior = rIncName(table, "hits", 1);
    teqz(prior, 1);
    prior = rIncName(table, "hits", 5);
    teqz(prior, 2);
    prior = rIncName(table, "hits", 0);
    teqz(prior, 7);

    /*
        A negative increment counts down, and a second counter is independent of the first.
     */
    prior = rIncName(table, "down", -3);
    teqz(prior, 0);
    prior = rIncName(table, "down", 0);
    teqz(prior, -3);
    prior = rIncName(table, "hits", 0);
    teqz(prior, 7);

    teqi(rGetHashLength(table), 2);
    rFreeHash(table);
}

/*
    A clone is independent: changing one table must not be visible in the other.
 */
static void cloneHash()
{
    RHash *table, *clone;
    cchar *value;
    int   rc;

    table = rAllocHash(0, R_DYNAMIC_NAME | R_DYNAMIC_VALUE);
    rAddName(table, sclone("one"), sclone("first"), 0);
    rAddName(table, sclone("two"), sclone("second"), 0);

    clone = rCloneHash(table);
    tnotnull(clone);
    teqi(rGetHashLength(clone), 2);
    value = rLookupName(clone, "one");
    tmatch(value, "first");
    value = rLookupName(clone, "two");
    tmatch(value, "second");

    /*
        The values are copies, not shared pointers.
     */
    ttrue(rLookupName(clone, "one") != rLookupName(table, "one"));

    rc = rRemoveName(clone, "one");
    teqi(rc, 0);
    value = rLookupName(table, "one");
    tmatch(value, "first");

    rFreeHash(clone);
    rFreeHash(table);

    clone = rCloneHash(NULL);
    tnull(clone);
}


/*
    A caseless table matches regardless of case but preserves the name as it was given.
 */
static void caselessHash()
{
    RHash *table;
    RName *np;
    cchar *value;

    table = rAllocHash(0, R_HASH_CASELESS | R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(table, "Content-Type", "text/html", 0);

    value = rLookupName(table, "content-type");
    tmatch(value, "text/html");
    value = rLookupName(table, "CONTENT-TYPE");
    tmatch(value, "text/html");

    np = rLookupNameEntry(table, "content-TYPE");
    tnotnull(np);
    tmatch(np->name, "Content-Type");

    rFreeHash(table);
}


/*
    rHashToBuf and rHashToString render "name="value"" pairs with a caller-supplied separator.
 */
static void hashToString()
{
    RHash *table;
    RBuf  *buf;
    char  *str;
    cchar *text;

    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(table, "one", "1", 0);

    buf = rHashToBuf(table, ",");
    tnotnull(buf);
    text = rBufToString(buf);
    tmatch(text, "one=\"1\"");
    rFreeBuf(buf);

    /*
        The default separator is a comma, and the trailing separator is trimmed.
     */
    rAddName(table, "two", "2", 0);
    str = rHashToString(table, NULL);
    tnotnull(str);
    ttrue(scontains(str, "one=\"1\"") != 0);
    ttrue(scontains(str, "two=\"2\"") != 0);
    tfalse(sends(str, ",") != 0);
    rFree(str);

    str = rHashToString(table, " | ");
    tnotnull(str);
    ttrue(scontains(str, " | ") != 0);
    tfalse(sends(str, " | ") != 0);
    rFree(str);

    /*
        An empty table renders as an empty string, not a stray separator.
     */
    rFreeHash(table);
    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    str = rHashToString(table, ",");
    tmatch(str, "");
    rFree(str);
    rFreeHash(table);
}


/*
    rHashToJsonBuf is the serializer. Numbers and booleans are emitted bare, everything else is
    quoted with JSON escaping, and a null table is an empty object rather than a crash.
 */
static void hashToJson()
{
    RHash *table;
    RBuf  *buf;
    char  *json;
    cchar *text;

    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(table, "name", "value", 0);

    json = rHashToJson(table, 0);
    tnotnull(json);
    tmatch(json, "{\"name\":\"value\"}");
    rFree(json);

    /*
        Numbers and booleans are not quoted.
     */
    rFreeHash(table);
    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(table, "count", "42", 0);
    json = rHashToJson(table, 0);
    tmatch(json, "{\"count\":42}");
    rFree(json);

    rFreeHash(table);
    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(table, "ratio", "1.5", 0);
    json = rHashToJson(table, 0);
    tmatch(json, "{\"ratio\":1.5}");
    rFree(json);

    rFreeHash(table);
    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(table, "on", "true", 0);
    json = rHashToJson(table, 0);
    tmatch(json, "{\"on\":true}");
    rFree(json);

    rFreeHash(table);
    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(table, "off", "false", 0);
    json = rHashToJson(table, 0);
    tmatch(json, "{\"off\":false}");
    rFree(json);

    /*
        Every escape the serializer knows. A value that came from outside the process is exactly
        the value that must not be able to break out of its quotes.
     */
    rFreeHash(table);
    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(table, "quote", "say \"hi\"", 0);
    json = rHashToJson(table, 0);
    tmatch(json, "{\"quote\":\"say \\\"hi\\\"\"}");
    rFree(json);

    rFreeHash(table);
    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(table, "path", "a\\b", 0);
    json = rHashToJson(table, 0);
    tmatch(json, "{\"path\":\"a\\\\b\"}");
    rFree(json);

    rFreeHash(table);
    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(table, "ws", "a\b\f\n\r\tb", 0);
    json = rHashToJson(table, 0);
    tmatch(json, "{\"ws\":\"a\\b\\f\\n\\r\\tb\"}");
    rFree(json);

    /*
        A control character with no short escape becomes a \u sequence.
     */
    rFreeHash(table);
    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(table, "ctrl", "a\001b", 0);
    json = rHashToJson(table, 0);
    tmatch(json, "{\"ctrl\":\"a\\u0001b\"}");
    rFree(json);

    /*
        Pretty output is indented and newline separated.
     */
    rFreeHash(table);
    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(table, "name", "value", 0);
    json = rHashToJson(table, 1);
    tnotnull(json);
    ttrue(scontains(json, "\n") != 0);
    ttrue(scontains(json, "    \"name\": \"value\"") != 0);
    rFree(json);

    /*
        An empty table and a null table both render as an empty object.
     */
    rFreeHash(table);
    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    json = rHashToJson(table, 0);
    tmatch(json, "{}");
    rFree(json);
    rFreeHash(table);

    json = rHashToJson(NULL, 0);
    tmatch(json, "{}");
    rFree(json);

    /*
        rHashToJsonBuf appends to a caller-supplied buffer and refuses a null one.
     */
    buf = rAllocBuf(256);
    rPutStringToBuf(buf, "prefix:");
    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(table, "k", "v", 0);
    rHashToJsonBuf(table, buf, 0);
    text = rBufToString(buf);
    tmatch(text, "prefix:{\"k\":\"v\"}");
    rFreeBuf(buf);
    rFreeHash(table);

    buf = rHashToJsonBuf(table, NULL, 0);
    tnull(buf);
}


/*
    Growth. The bucket table starts at the first entry of hashSizes and the name array grows by
    half again, so both only rehash once the table is well past its initial size. Every one of
    those paths was uncovered: no test had put more than a handful of entries in a table.
 */
static void hashGrowth()
{
    RHash *table;
    RName *np;
    char  key[32];
    cchar *value;
    int   i, count;

    //  Past the first two bucket sizes, which forces growBuckets and growNames more than once
    table = rAllocHash(0, 0);
    tnotnull(table);
    for (i = 0; i < 200; i++) {
        sfmtbuf(key, sizeof(key), "key-%d", i);
        //  Bound to a local: tnotnull expands its argument twice
        np = rAddName(table, key, sitos(i), R_TEMPORAL_NAME | R_DYNAMIC_VALUE);
        tnotnull(np);
    }
    teqi(rGetHashLength(table), 200);

    //  Every entry survived the rehash and is still reachable by name
    for (i = 0; i < 200; i++) {
        sfmtbuf(key, sizeof(key), "key-%d", i);
        value = rLookupName(table, key);
        tnotnull(value);
        teqi(atoi(value), i);
    }

    //  ... and by iteration, exactly once each
    count = 0;
    for (np = rGetNextName(table, NULL); np; np = rGetNextName(table, np)) {
        count++;
    }
    teqi(count, 200);
    rFreeHash(table);

    /*
        Duplicates grow the same structures by a different route: rAddDuplicateName never
        replaces, so the length rises with every call and the bucket chain for one key gets long.
     */
    table = rAllocHash(0, R_STATIC_NAME | R_DYNAMIC_VALUE);
    for (i = 0; i < 150; i++) {
        np = rAddDuplicateName(table, "same", sitos(i), 0);
        tnotnull(np);
    }
    teqi(rGetHashLength(table), 150);

    count = 0;
    for (np = rGetNextName(table, NULL); np; np = rGetNextName(table, np)) {
        if (smatch(np->name, "same")) {
            count++;
        }
    }
    teqi(count, 150);
    rFreeHash(table);

    /*
        A name or value flag left at zero is inherited from the table rather than defaulting to
        static, so a table declared temporal clones the key it is given.
     */
    table = rAllocHash(0, R_TEMPORAL_NAME | R_TEMPORAL_VALUE);
    scopy(key, sizeof(key), "borrowed");
    np = rAddName(table, key, "held", 0);
    tnotnull(np);
    ttrue(np->name != key);
    scopy(key, sizeof(key), "overwritten");
    value = rLookupName(table, "borrowed");
    tmatch(value, "held");
    rFreeHash(table);
}


/*
    A budgeted allocator: unlimited while the budget is negative, otherwise it fails once the
    budget reaches zero.
 */
static int allocBudget = -1;
static int failCalls;

static void *budgetAlloc(size_t size)
{
    if (allocBudget == 0) {
        return NULL;
    }
    if (allocBudget > 0) {
        allocBudget--;
    }
    return malloc(size);
}

static void *budgetRealloc(void *ptr, size_t size)
{
    if (allocBudget == 0) {
        return NULL;
    }
    if (allocBudget > 0) {
        allocBudget--;
    }
    return realloc(ptr, size);
}

static void budgetFree(void *ptr)
{
    free(ptr);
}

static void failHandler(int cause, size_t size)
{
    failCalls++;
}

/*
    Allocation failures are reported, and a hash that fails to grow keeps every entry reachable
 */
static void hashAllocFailures(void)
{
    RHash *hash;
    RName *np;
    char  name[32];
    int   added, i, point, rc;

    rSetAllocator(budgetAlloc, budgetRealloc, budgetFree);
    rSetMemHandler(failHandler);

    //  The header, the buckets and the names are separate allocations and any can fail
    allocBudget = 0;
    failCalls = 0;
    hash = rAllocHash(0, 0);
    tnull(hash);
    teqi(failCalls, 1);
    allocBudget = 1;
    hash = rAllocHash(16, 0);
    tnull(hash);
    allocBudget = 2;
    hash = rAllocHash(16, 0);
    tnull(hash);

    //  An empty hash cannot take its first entry without buckets
    allocBudget = -1;
    hash = rAllocHash(0, 0);
    tnotnull(hash);
    allocBudget = 0;
    np = rAddName(hash, "first", "1", 0);
    tnull(np);
    teqi((int) hash->length, 0);

    /*
        Fill to each growth point in turn, fail the growth, and check that every entry added
        so far is still found. The bucket and name arrays grow at different points so both
        failure paths are reached.
     */
    allocBudget = -1;
    added = 0;
    for (point = 0; point < 4; point++) {
        while (hash->length < hash->numBuckets && hash->free >= 0) {
            SFMT(name, "name-%d", added);
            np = rAddName(hash, name, name, R_TEMPORAL_NAME | R_TEMPORAL_VALUE);
            tnotnull(np);
            added++;
        }
        allocBudget = 0;
        SFMT(name, "name-%d", added);
        np = rAddName(hash, name, name, R_TEMPORAL_NAME | R_TEMPORAL_VALUE);
        tnull(np);
        teqi((int) hash->length, added);
        for (i = 0; i < added; i++) {
            SFMT(name, "name-%d", i);
            tmatch((char*) rLookupName(hash, name), name);
        }
        //  With memory available again the deferred entry goes in
        allocBudget = -1;
        SFMT(name, "name-%d", added);
        np = rAddName(hash, name, name, R_TEMPORAL_NAME | R_TEMPORAL_VALUE);
        tnotnull(np);
        added++;
    }
    rc = (int) hash->length;
    teqi(rc, added);
    rFreeHash(hash);

    rSetMemHandler(NULL);
    rSetAllocator(NULL, NULL, NULL);
}

int main(void)
{
    rInit(0, 0);
    createTable();
    isTableEmpty();
    inserAndRemoveHash();
    hashScale();
    iterateHash();

    duplicateNames();
    hashGrowth();
    addNameVariants();
    incrementNames();
    cloneHash();
    caselessHash();
    hashToString();
    hashToJson();
    hashAllocFailures();

    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
