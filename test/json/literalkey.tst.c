/*
    literalkey.tst.c - Unit tests for jsonSetKey/jsonGetKey literal (unparsed) property keys

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonLiteralKeyTest()
{
    Json    *obj;
    char    *s;
    int     rc;

    obj = parse("{}");
    rc = jsonSetKey(obj, 0, "a.b", "mallory", 0);
    ttrue(rc > 0);
    ttrue(jsonGetId(obj, 0, "a") < 0);
    tmatch(jsonGetKey(obj, 0, "a.b", 0), "mallory");
    jsonFree(obj);

    obj = parse("{}");
    jsonSetKey(obj, 0, "a.b", "mallory", 0);
    ttrue(jsonGet(obj, 0, "a.b", 0) == 0);
    jsonFree(obj);

    obj = parse("{ user: 'alice', other: 'unchanged' }");
    rc = jsonSetKey(obj, 0, "user", "mallory", 0);
    ttrue(rc > 0);
    tmatch(jsonGet(obj, 0, "user", 0), "mallory");
    tmatch(jsonGet(obj, 0, "other", 0), "unchanged");
    jsonFree(obj);

    obj = parse("{ cfg: { a: 1, b: 2, c: 3 }, other: 'unchanged' }");
    rc = jsonSetKey(obj, 0, "cfg", "pwn", 0);
    ttrue(rc > 0);
    tmatch(jsonGet(obj, 0, "cfg", 0), "pwn");
    tmatch(jsonGet(obj, 0, "other", 0), "unchanged");
    jsonFree(obj);

    obj = parse("{}");
    rc = jsonSetKey(obj, 0, "a[99999]", "1", 0);
    ttrue(rc > 0);
    ttrue(jsonGetLength(obj, 0, 0) == 1);
    tmatch(jsonGetKey(obj, 0, "a[99999]", 0), "1");
    jsonFree(obj);

    obj = parse("{}");
    jsonSetKey(obj, 0, "a.b", "value", 0);
    s = jsonToString(obj, 0, 0, 0);
    jsonFree(obj);

    obj = jsonParseString(s, NULL, 0);
    rFree(s);
    ttrue(obj != NULL);
    tmatch(jsonGetKey(obj, 0, "a.b", 0), "value");
    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonLiteralKeyTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
