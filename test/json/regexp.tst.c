/*
    regexp.c.tst - Unit tests for JSON regexp primitives

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonRegExp()
{
    Json    *obj;
    char    *text, *s;

    text = "{ pattern: /abc/}";
    obj = parse(text);
    ttrue(obj != 0);
    tmatch(jsonGet(obj, 0, "pattern", 0), "abc");
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{pattern:/abc/}");
    jsonFree(obj);
    rFree(s);

    text = "{ pattern: /^(\\w+ \\d+ \\d+:\\d+:\\d+) (\\w+) (\\w+)\\[(\\d)+\\]: (.*)$/ }";
    obj = parse(text);
    ttrue(obj != 0);
    tmatch(jsonGet(obj, 0, "pattern", 0), "^(\\w+ \\d+ \\d+:\\d+:\\d+) (\\w+) (\\w+)\\[(\\d)+\\]: (.*)$");
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{pattern:/^(\\w+ \\d+ \\d+:\\d+:\\d+) (\\w+) (\\w+)\\[(\\d)+\\]: (.*)$/}");
    jsonFree(obj);
    rFree(s);
}


int main(void)
{
    rInit(0, 0);
    jsonRegExp();
    rTerm();
    return 0;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
