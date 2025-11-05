/*
    strict.c.tst - Unit tests for JSON strict mode

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonStrict()
{
    Json    *obj;

    obj = parse("{ key: { one: 1, two: 2}, key2: 42 }");
    ttrue(obj != 0);
    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonStrict();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
