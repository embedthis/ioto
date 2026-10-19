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


/*
    A regular expression has no JSON representation.
 */
static void jsonRegExpDialect()
{
    Json *obj;
    char *s;

    obj = parse("{re: /ab+c/, n: 1}");

    //  JSON output renders the pattern as a string, so the result is valid JSON
    s = jsonToString(obj, 0, 0, JSON_JSON);
    tmatch(s, "{\"re\":\"ab+c\",\"n\":1}");
    ttrue(jsonParse(s, JSON_STRICT_PARSE) != 0);
    rFree(s);

    //  JSON5 and the default dialect keep /pattern/
    s = jsonToString(obj, 0, 0, JSON_JSON5);
    tmatch(s, "{re:/ab+c/,n:1}");
    rFree(s);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{re:/ab+c/,n:1}");
    rFree(s);
    jsonFree(obj);

    //  Strict mode rejects a regexp on input, as it rejects comments and single quotes
    ttrue(jsonParse("{\"re\": /ab+c/}", JSON_STRICT_PARSE) == 0);
    ttrue(jsonParse("[/x/]", JSON_STRICT_PARSE) == 0);

    //  An unterminated regexp swallowing structure no longer yields two top level values
    obj = parse("[1,,/][1,,/]");
    ttrue(obj != 0);
    s = jsonToString(obj, 0, 0, JSON_JSON);
    ttrue(jsonParse(s, JSON_STRICT_PARSE) != 0);
    rFree(s);
    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonRegExpDialect();
    jsonRegExp();
    rTerm();
    return 0;
}


/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
