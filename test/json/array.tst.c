
/*
    array.c.tst - Unit tests for arrays

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonArrayTest()
{
    Json    *array;
    cchar   *value;
    int     rc;

    // Indexing
    array = parse("['one', 'two', 'three']");
    value = jsonGet(array, 0, "0", 0);
    tmatch(value, "one");
    value = jsonGet(array, 0, "1", 0);
    tmatch(value, "two");
    value = jsonGet(array, 0, "2", 0);
    tmatch(value, "three");
    value = jsonGet(array, 0, "3", 0);
    ttrue(value == 0);

    // Set
    array = parse("['one', 'two', 'three']");
    rc = jsonSet(array, 0, "1", "TWO", 0);
    ttrue(rc > 0);
    value = jsonGet(array, 0, "1", 0);
    tmatch(value, "TWO");
    tmatch(jsonString(array, JSON_ONE_LINE), "['one','TWO','three']");


    /*
        Add element to array
     */
    rc = jsonSet(array, 0, "[$]", "four", 0);
    ttrue(rc > 0);
    value = jsonGet(array, 0, "3", 0);
    tmatch(value, "four");
    tmatch(jsonString(array, JSON_ONE_LINE), "['one','TWO','three','four']");

    jsonFree(array);
}

int main(void)
{
    rInit(0, 0);
    jsonArrayTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
