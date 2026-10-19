/*
    render.c.tst - Unit tests for JSON serialization flags

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonDebugRenderTest()
{
    Json    *obj;
    char    *s;

    obj = parse("{name: 'John', list: [1, 2]}");

    //  Debug format prefixes each node with its node ID and last index
    s = jsonToString(obj, 0, 0, JSON_DEBUG);
    ttrue(scontains(s, "<0/") != 0);
    ttrue(scontains(s, "John") != 0);
    rFree(s);

    jsonFree(obj);
}

static void jsonBareRenderTest()
{
    Json    *obj;
    char    *s;

    obj = parse("{a: 1, b: 'two'}");
    s = jsonToString(obj, 0, 0, JSON_BARE);
    tmatch(s, "a:1,b:two");
    rFree(s);
    jsonFree(obj);

    obj = parse("[1, 'two']");
    s = jsonToString(obj, 0, 0, JSON_BARE);
    tmatch(s, "1,two");
    rFree(s);
    jsonFree(obj);
}

static void jsonUnknownTypeRenderTest()
{
    Json        *obj;
    JsonNode    *node;
    char        *s;

    //  A node with a type the renderer does not know is emitted as undefined
    obj = parse("{a: 1}");
    node = &obj->nodes[0];
    jsonSetNodeType(node, JSON_COMMENT);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "undefined");
    rFree(s);
    jsonFree(obj);
}

static void jsonEmptyNodeRenderTest()
{
    Json        *obj;
    JsonNode    *node;
    char        *s;

    //  Nodes with a zero type are skipped when rendering arrays
    obj = parse("[1, 2, 3]");
    node = jsonGetChildNode(obj, 0, 1);
    ttrue(node != 0);
    jsonSetNodeType(node, 0);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "[1,3]");
    rFree(s);
    jsonFree(obj);

    //  And when rendering objects
    obj = parse("{a: 1, b: 2, c: 3}");
    node = jsonGetNode(obj, 0, "b");
    ttrue(node != 0);
    jsonSetNodeType(node, 0);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{a:1,c:3}");
    rFree(s);
    jsonFree(obj);
}

static void jsonExpandKeyRenderTest()
{
    Json    *obj;
    char    *s;

    //  Key expansion is not yet implemented, so a ${token} key is rendered as is
    obj = parse("{'${name}': 'value', name: 'John'}");
    s = jsonToString(obj, 0, 0, JSON_EXPAND);
    ttrue(scontains(s, "'${name}':'value'") != 0);
    rFree(s);

    s = jsonToString(obj, 0, 0, 0);
    ttrue(scontains(s, "'${name}':'value'") != 0);
    rFree(s);
    jsonFree(obj);

    //  An unresolvable key reference is left alone
    obj = parse("{'${missing}': 'value'}");
    s = jsonToString(obj, 0, 0, JSON_EXPAND);
    ttrue(scontains(s, "${missing}") != 0);
    rFree(s);
    jsonFree(obj);

    //  Values are expanded with JSON_EXPAND
    obj = parse("{greeting: 'hello ${name}', name: 'John'}");
    s = jsonToString(obj, 0, 0, JSON_EXPAND);
    ttrue(scontains(s, "hello John") != 0);
    rFree(s);
    jsonFree(obj);
}

static void jsonRegExpRenderTest()
{
    Json    *obj;
    char    *s;

    obj = parse("{re: /ab+c/}");
    ttrue(obj != 0);
    checkType(obj, "re", JSON_REGEXP);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{re:/ab+c/}");
    rFree(s);
    jsonFree(obj);
}

static void jsonStringRenderTest()
{
    Json    *obj;
    cchar   *s;

    obj = parse("{name: 'John', list: [1, 2]}");

    //  A zero flag selects the human readable format
    s = jsonString(obj, 0);
    ttrue(s != 0);
    ttrue(scontains(s, "name") != 0);

    //  The prior value is released and replaced
    s = jsonString(obj, JSON_JSON);
    tmatch(s, "{\"name\":\"John\",\"list\":[1,2]}");

    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonDebugRenderTest();
    jsonBareRenderTest();
    jsonUnknownTypeRenderTest();
    jsonEmptyNodeRenderTest();
    jsonExpandKeyRenderTest();
    jsonRegExpRenderTest();
    jsonStringRenderTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
