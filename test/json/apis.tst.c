/*
    apis.c.tst - Unit tests for the typed setter and buffer APIs

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

/*
    Call the format setters via an unattributed pointer so a null format can be
    passed without tripping the compiler format checks.
 */
typedef int (*SetFmtProc)(Json *json, int nid, cchar *key, cchar *fmt, ...);

static SetFmtProc setFmt = jsonSetFmt;
static SetFmtProc setJsonFmt = jsonSetJsonFmt;

static void jsonTypedSetTest()
{
    Json    *obj;
    cchar   *nofmt;
    int     rc;

    obj = parse("{}");

    rc = jsonSetString(obj, 0, "name", "Alice");
    ttrue(rc > 0);
    checkValue(obj, "name", "Alice");
    checkType(obj, "name", JSON_STRING);

    rc = jsonSetNumber(obj, 0, "count", 42);
    ttrue(rc > 0);
    checkValue(obj, "count", "42");
    checkType(obj, "count", JSON_PRIMITIVE);
    ttrue(jsonGetNum(obj, 0, "count", 0) == 42);

    rc = jsonSetNumber(obj, 0, "negative", -7);
    ttrue(rc > 0);
    checkValue(obj, "negative", "-7");

    rc = jsonSetBool(obj, 0, "enable", 1);
    ttrue(rc > 0);
    checkValue(obj, "enable", "true");
    ttrue(jsonGetBool(obj, 0, "enable", 0) == 1);

    rc = jsonSetBool(obj, 0, "disable", 0);
    ttrue(rc > 0);
    checkValue(obj, "disable", "false");
    ttrue(jsonGetBool(obj, 0, "disable", 1) == 0);

    rc = jsonSetDouble(obj, 0, "ratio", 3.5);
    ttrue(rc > 0);
    checkType(obj, "ratio", JSON_PRIMITIVE);
    ttrue(jsonGetDouble(obj, 0, "ratio", 0) == 3.5);

    //  jsonSetFmt sleuths the value type
    rc = jsonSetFmt(obj, 0, "greeting", "%s-%d", "hello", 3);
    ttrue(rc > 0);
    checkValue(obj, "greeting", "hello-3");
    checkType(obj, "greeting", JSON_STRING);

    rc = jsonSetFmt(obj, 0, "port", "%d", 8080);
    ttrue(rc > 0);
    checkValue(obj, "port", "8080");
    checkType(obj, "port", JSON_PRIMITIVE);

    //  A null format is tolerated
    nofmt = 0;
    rc = setFmt(obj, 0, "unused", nofmt);
    ttrue(rc == 0);
    ttrue(jsonGetId(obj, 0, "unused") < 0);

    //  Nested paths
    rc = jsonSetString(obj, 0, "user.address.city", "Boston");
    ttrue(rc > 0);
    checkValue(obj, "user.address.city", "Boston");

    jsonFree(obj);
}

static void jsonSetJsonFmtTest()
{
    Json    *obj;
    cchar   *nofmt;
    int     rc;

    obj = parse("{}");

    rc = jsonSetJsonFmt(obj, 0, "config", "{a:1,b:[2,3],c:{d:'deep'}}");
    ttrue(rc >= 0);
    checkValue(obj, "config.a", "1");
    checkValue(obj, "config.b[1]", "3");
    checkValue(obj, "config.c.d", "deep");

    //  Overwrite an existing property
    rc = jsonSetJsonFmt(obj, 0, "config", "{a:%d}", 99);
    ttrue(rc >= 0);
    checkValue(obj, "config.a", "99");

    //  Unparsable json is rejected
    rc = jsonSetJsonFmt(obj, 0, "bad", "%s", "{a:");
    ttrue(rc < 0);
    ttrue(jsonGetId(obj, 0, "bad") < 0);

    //  A null format is rejected
    nofmt = 0;
    rc = setJsonFmt(obj, 0, "none", nofmt);
    ttrue(rc < 0);

    jsonFree(obj);
}

static void jsonNodeValueTest()
{
    Json        *obj;
    JsonNode    *node;
    char        *s;

    obj = parse("{test: 'initial'}");
    node = jsonGetNode(obj, 0, "test");
    ttrue(node != 0);

    //  Replace an allocated value with a cloned value
    jsonSetNodeValue(node, "second", JSON_STRING, 0);
    checkValue(obj, "test", "second");

    //  Replace again to release the prior allocation
    jsonSetNodeValue(node, "third", JSON_STRING, 0);
    checkValue(obj, "test", "third");

    //  Transfer ownership of the value to the node
    jsonSetNodeValue(node, sclone("owned"), JSON_STRING, JSON_PASS_VALUE);
    checkValue(obj, "test", "owned");

    //  A null value renders as null
    jsonSetNodeValue(node, 0, JSON_STRING, JSON_PASS_VALUE);
    s = jsonToString(obj, 0, 0, JSON_JSON);
    tmatch(s, "{\"test\":null}");
    rFree(s);

    jsonFree(obj);
}

static void jsonPutToBufTest()
{
    Json    *obj;
    RBuf    *buf;
    int     rc;

    obj = parse("{name: 'John', items: [1, 2]}");

    buf = rAllocBuf(0);
    rc = jsonPutToBuf(buf, obj, 0, JSON_JSON);
    ttrue(rc > 0);
    tmatch(rGetBufStart(buf), "{\"name\":\"John\",\"items\":[1,2]}");
    rFreeBuf(buf);

    //  Render a sub-tree
    buf = rAllocBuf(0);
    rc = jsonPutToBuf(buf, obj, jsonGetId(obj, 0, "items"), JSON_JSON);
    ttrue(rc > 0);
    tmatch(rGetBufStart(buf), "[1,2]");
    rFreeBuf(buf);

    //  A null json object is tolerated
    buf = rAllocBuf(0);
    rc = jsonPutToBuf(buf, 0, 0, 0);
    ttrue(rc == 0);
    rFreeBuf(buf);

    //  A null buffer is an error
    rc = jsonPutToBuf(0, obj, 0, 0);
    ttrue(rc < 0);

    jsonFree(obj);
}

static void jsonGetRefTest()
{
    Json    *obj;
    cchar   *value;

    obj = parse("{name: 'John', age: 30}");

    value = jsonGetRef(obj, 0, "name", 0);
    tmatch(value, "John");
    ttrue(value == jsonGet(obj, 0, "name", 0));

    value = jsonGetRef(obj, 0, "missing", "fallback");
    tmatch(value, "fallback");

    jsonFree(obj);
}

static void jsonGetLengthTest()
{
    Json    *obj;

    obj = parse("{list: [10, 20, 30], nested: [[1,2],[3,4]], obj: {a:1, b:2}, empty: [], str: 'text'}");

    ttrue(jsonGetLength(obj, 0, "list") == 3);
    ttrue(jsonGetLength(obj, 0, "nested") == 2);
    ttrue(jsonGetLength(obj, 0, "obj") == 2);
    ttrue(jsonGetLength(obj, 0, "empty") == 0);
    ttrue(jsonGetLength(obj, 0, "str") == 0);
    ttrue(jsonGetLength(obj, 0, 0) == 5);

    //  A missing key cannot be measured
    ttrue(jsonGetLength(obj, 0, "missing") < 0);

    jsonFree(obj);
}

static void jsonGetErrorTest()
{
    Json    *obj;

    //  A freshly allocated object has no error
    obj = jsonAlloc();
    ttrue(jsonGetError(obj) == 0);
    ttrue(jsonParseText(obj, sclone("{name:"), 0) < 0);
    ttrue(jsonGetError(obj) != 0);
    ttrue(scontains(jsonGetError(obj), "JSON Parse Error") != 0);
    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonTypedSetTest();
    jsonSetJsonFmtTest();
    jsonNodeValueTest();
    jsonPutToBufTest();
    jsonGetRefTest();
    jsonGetLengthTest();
    jsonGetErrorTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
