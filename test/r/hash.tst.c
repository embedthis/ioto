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


int main(void)
{
    rInit(0, 0);
    createTable();
    isTableEmpty();
    inserAndRemoveHash();
    hashScale();
    iterateHash();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
