
/*
    comments.c.tst - Unit tests for JSON comments

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonComments()
{
    Json    *obj;

    obj = parse("/* Comment */");
    ttrue(obj);
    jsonFree(obj);

    obj = jsonParseString("/* Unclosed comment ", NULL, 0);
    ttrue(obj == 0);

    obj = parse("// Rest of line ");
    ttrue(obj);
    jsonFree(obj);

    obj = parse("/* Leading Comment */ {}");
    ttrue(obj);
    jsonFree(obj);

    obj = parse("{}/* Trailing Comment */");
    ttrue(obj != 0);
    jsonFree(obj);

    obj = parse("{/* Inside Comment */}");
    ttrue(obj != 0);
    jsonFree(obj);

    obj = parse("{/* Before name Comment */ color: 'red'}");
    ttrue(obj != 0);
    checkJson(obj, "{color:'red'}", 0);
    jsonFree(obj);

    obj = parse("{color: /* Before value */ 'red'}");
    ttrue(obj != 0);
    checkJson(obj, "{color:'red'}", 0);
    jsonFree(obj);

    obj = parse("{color: 'red' /* After value */}");
    ttrue(obj != 0);
    checkJson(obj, "{color:'red'}", 0);
    jsonFree(obj);
}


/*
    A comment abutting a value must terminate it, as white space does.
 */
static void jsonAbuttingComments()
{
    Json *obj;
    char *s;

    obj = parse("{a: 1//c\n}");
    checkValue(obj, "a", "1");
    checkType(obj, "a", JSON_PRIMITIVE);
    jsonFree(obj);

    obj = parse("{a: 1/*c*/}");
    checkValue(obj, "a", "1");
    jsonFree(obj);

    obj = parse("{a: 1 //c\n}");
    checkValue(obj, "a", "1");
    jsonFree(obj);

    obj = parse("{timeout: 30//seconds\n, retries: 3}");
    checkValue(obj, "timeout", "30");
    checkValue(obj, "retries", "3");
    jsonFree(obj);

    //  An unquoted string token is terminated the same way
    obj = parse("{a: hello//c\n}");
    checkValue(obj, "a", "hello");
    checkType(obj, "a", JSON_STRING);
    jsonFree(obj);

    //  A top level primitive followed by a comment
    obj = parse("false//c");
    s = jsonToString(obj, 0, 0, JSON_JSON);
    tmatch(s, "false");
    rFree(s);
    jsonFree(obj);

    //  A regular expression still parses; the '/' is not a comment there
    obj = parse("{re: /ab+c/}");
    checkValue(obj, "re", "ab+c");
    checkType(obj, "re", JSON_REGEXP);
    jsonFree(obj);

    //  The rendered JSON must survive a strict reparse
    obj = parse("{a: 1/*c*/, b: 2//d\n}");
    s = jsonToString(obj, 0, 0, JSON_JSON);
    tmatch(s, "{\"a\":1,\"b\":2}");
    rFree(s);
    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonComments();
    jsonAbuttingComments();
    rTerm();
    return 0;
}


/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
