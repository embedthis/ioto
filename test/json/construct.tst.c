/*
    construct.c.tst - Unit tests for JSON constructor

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonEmpty()
{
    Json    *obj;

    obj = parse(0);
    ttrue(obj);
    checkJson(obj, "", 0);
    jsonFree(obj);

    obj = parse("");
    checkJson(obj, "", 0);
    jsonFree(obj);
}


static void jsonConstruct()
{
    Json    *obj;

    obj = parse("{}");
    ttrue(obj != 0);
    checkType(obj, 0,  JSON_OBJECT);
    checkJson(obj, "{}", 0);
    jsonFree(obj);

    obj = parse("[]");
    ttrue(obj != 0);
    checkType(obj, 0,  JSON_ARRAY);
    checkJson(obj, "[]", 0);
    jsonFree(obj);

    obj = parse("{a:42}");
    ttrue(obj != 0);
    checkType(obj, 0, JSON_OBJECT);
    checkValue(obj, "a", "42");
    checkJson(obj, "{a:42}", 0);
    jsonFree(obj);

    obj = parse("[1,2,3]");
    ttrue(obj != 0);
    checkType(obj, 0, JSON_ARRAY);
    checkJson(obj, "[1,2,3]", 0);
    jsonFree(obj);

    /*
        Strings must be objects {} or arrays []
     */
    obj = parse("1");
    ttrue(obj != 0);
    checkType(obj, 0, JSON_PRIMITIVE);
    checkJson(obj, "1", 0);
    jsonFree(obj);

    obj = parse("true");
    ttrue(obj != 0);
    checkType(obj, 0, JSON_PRIMITIVE);
    checkJson(obj, "true", 0);
    jsonFree(obj);

    obj = parse("false");
    ttrue(obj != 0);
    checkType(obj, 0, JSON_PRIMITIVE);
    checkJson(obj, "false", 0);
    jsonFree(obj);

    obj = parse("null");
    ttrue(obj != 0);
    checkType(obj, 0, JSON_PRIMITIVE);
    checkJson(obj, "null", 0);
    jsonFree(obj);

    obj = parse("undefined");
    ttrue(obj != 0);
    checkType(obj, 0, JSON_PRIMITIVE);
    checkJson(obj, "undefined", 0);
    jsonFree(obj);

    obj = parse("{ value: /pattern/ }");
    ttrue(obj != 0);
    checkType(obj, "value", JSON_REGEXP);
    checkValue(obj, "value", "pattern");
    checkJson(obj, "{value:/pattern/}", 0);
    jsonFree(obj);

    obj = parse("'hello world'");
    ttrue(obj != 0);
    checkType(obj, 0, JSON_STRING);
    checkJson(obj, "'hello world'", 0);
    jsonFree(obj);

    obj = parse("5.42");
    ttrue(obj != 0);
    checkType(obj, 0, JSON_PRIMITIVE);
    checkJson(obj, "5.42", 0);
    jsonFree(obj);

    obj = parse("{ 'user': { 'name': 'john', age: 42 }}");
    ttrue(obj != 0);
    checkType(obj, 0, JSON_OBJECT);
    checkJson(obj, "{user:{name:'john',age:42}}", 0);
    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonEmpty();
    jsonConstruct();
    rTerm();
    return 0;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
