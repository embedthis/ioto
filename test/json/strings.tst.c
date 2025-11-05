/*
    strings.c.tst - Unit tests for JSON strings

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonStrings()
{
    Json    *obj;

    obj = parse("{multiline: 'one\ntwo\nthree'}");
    checkJson(obj, "{multiline:'one\ntwo\nthree'}", 0);
    jsonFree(obj);

    obj = parse("{ 1234: 42 }");
    checkJson(obj, "{1234:42}", 0);
    jsonFree(obj);

    obj = parse("{ 'abc\\u0001def': 42 }");
    checkJson(obj, "{'abc\\u0001def':42}", 0);
    jsonFree(obj);

    obj = parse("{'one\ttwo': 'three\tfour'}");
    checkJson(obj, "{'one\ttwo':'three\tfour'}", 0);
    jsonFree(obj);

    obj = parse("{'one\n': 'two\r'}");
    checkJson(obj, "{'one\n':'two\r'}", 0);
    jsonFree(obj);

    obj = parse("{'one\\u0001': 'two'}");
    checkJson(obj, "{'one\\u0001':'two'}", 0);
    jsonFree(obj);
}


static void jsonWhite()
{
    Json *obj;

    obj = parse("{value:'hello'}");
    checkJson(obj, "{value:'hello'}", 0);
    jsonFree(obj);

    obj = parse("     {value:'hello'}");
    checkJson(obj, "{value:'hello'}", 0);
    jsonFree(obj);

    obj = parse("{value:'hello'}       ");
    checkJson(obj, "{value:'hello'}", 0);
    jsonFree(obj);

    obj = parse("{     value:'hello'}");
    checkJson(obj, "{value:'hello'}", 0);
    jsonFree(obj);

    obj = parse("{value      :'hello'}");
    checkJson(obj, "{value:'hello'}", 0);
    jsonFree(obj);

    obj = parse("{value:        'hello'}");
    checkJson(obj, "{value:'hello'}", 0);
    jsonFree(obj);

    obj = parse("{value:'hello'        }");
    checkJson(obj, "{value:'hello'}", 0);
    jsonFree(obj);
}


static void jsonToStringTest()
{
    Json    *obj;
    char        *s;

    s = jsonToString(0, 0, 0, 0);
    ttrue(s == 0);

    obj = parse("{}");
    s = jsonToString(obj, 0, 0, 0);
    checkJson(obj, "{}", 0);
    jsonFree(obj);
    rFree(s);

    obj = parse("[]");
    checkJson(obj, "[]", 0);
    jsonFree(obj);

    obj = parse("42");
    checkJson(obj, "42", 0);
    jsonFree(obj);

    obj = parse("{age:42}");
    checkJson(obj, "{age:42}", 0);
    jsonFree(obj);

    //  Simple string
    obj = parse("{value:'abc'}");
    tmatch(jsonGet(obj, 0, "value", 0), "abc");
    checkJson(obj, "{value:'abc'}", 0);
    checkValue(obj, "value", "abc");
    jsonFree(obj);

    //  Embedded quotes
    obj = parse("{value:'\"abc\"'}");
    tmatch(jsonGet(obj, 0, "value", 0), "\"abc\"");
    checkJson(obj, "{value:'\"abc\"'}", 0);
    checkValue(obj, "value", "\"abc\"");
    jsonFree(obj);

    //  Embedded backslash
    //  This will embed a single "\" in the actual propery value.
    obj = parse("{value:'ab\\\\c'}");
    checkJson(obj, "{value:'ab\\\\c'}", 0);
    checkValue(obj, "value", "ab\\c");
    jsonFree(obj);

    //  Backslash at the end
    s = "{value:\"abc\\\\\"}";
    obj = parse(s);
    checkJson(obj, "{value:'abc\\\\'}", 0);
    checkValue(obj, "value", "abc\\");
    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonStrings();
    jsonWhite();
    jsonToStringTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
