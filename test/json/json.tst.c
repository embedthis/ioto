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
    char    *text, *s;

    text = "{ key: 'one\ntwo'}";
    obj = parse(text);
    ttrue(obj != 0);
    s = jsonToString(obj, 0, 0, JSON_JSON);
    tmatch(s, "{\"key\":\"one\\ntwo\"}");
    jsonFree(obj);
    rFree(s);
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
