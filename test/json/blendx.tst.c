/*
    blendx.c.tst - Unit tests for JSON blend edge cases

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void nullLog(cchar *type, cchar *source, cchar *msg)
{
}

/*
    Blend src into dest and compare the rendered result
 */
static void blendKeys(cchar *destText, cchar *dkey, cchar *srcText, cchar *skey, cchar *result, int flags)
{
    Json    *dest, *src;
    char    *s;
    int     rc;

    dest = parse(destText);
    src = parse(srcText);
    rc = jsonBlend(dest, 0, dkey, src, 0, skey, flags);
    ttrue(rc == 0);

    s = jsonToString(dest, 0, 0, 0);
    tmatch(s, result);
    rFree(s);
    jsonFree(dest);
    jsonFree(src);
}

static void jsonBlendKeyTest()
{
    //  A destination key that does not exist is created
    blendKeys("{}", "user", "{name: 'John'}", 0, "{user:{name:'John'}}", 0);

    //  A destination key that exists is used as the blend root
    blendKeys("{user: {age: 42}}", "user", "{name: 'John'}", 0, "{user:{age:42,name:'John'}}", 0);

    //  A source key selects the sub-tree to blend
    blendKeys("{}", 0, "{outer: {name: 'John'}}", "outer", "{name:'John'}", 0);

    //  A source key that does not exist blends nothing
    blendKeys("{a: 1}", 0, "{outer: {name: 'John'}}", "missing", "{a:1}", 0);
}

static void jsonBlendTypeTest()
{
    Json    *dest, *src;
    char    *s;
    int     rc;

    //  A scalar destination is converted to an object
    blendKeys("'scalar'", 0, "{name: 'John'}", 0, "{name:'John'}", 0);

    //  A string property overwritten by an array releases the string value
    blendKeys("{a: 'str'}", 0, "{a: [1, 2]}", 0, "{a:[1,2]}", 0);

    //  An object property overwritten by a shorter array
    blendKeys("{a: [1, 2, 3]}", 0, "{a: [9]}", 0, "{a:[9]}", 0);

    //  An allocated string property overwritten by an array releases the value
    dest = parse("{}");
    ttrue(jsonSet(dest, 0, "a", "allocated", 0) > 0);
    src = parse("{a: [1, 2]}");
    rc = jsonBlend(dest, 0, 0, src, 0, 0, 0);
    ttrue(rc == 0);
    s = jsonToString(dest, 0, 0, 0);
    tmatch(s, "{a:[1,2]}");
    rFree(s);
    jsonFree(dest);
    jsonFree(src);

    //  Arrays with JSON_CCREATE keep the destination
    blendKeys("[1, 2]", 0, "[3]", 0, "[1,2]", JSON_CCREATE);

    //  Arrays with JSON_REPLACE remove matching elements
    blendKeys("[1, 2, 3]", 0, "[2]", 0, "[1,3]", JSON_REPLACE);

    //  Undefined source values remove the destination property
    blendKeys("{a: 1, b: 2}", 0, "{a: undefined}", 0, "{b:2}", JSON_REMOVE_UNDEF);

    //  Mismatched container types cannot be appended or replaced
    dest = parse("[1, 2]");
    src = parse("{a: 1}");
    rc = jsonBlend(dest, 0, 0, src, 0, 0, JSON_APPEND);
    ttrue(rc < 0);
    rc = jsonBlend(dest, 0, 0, src, 0, 0, JSON_REPLACE);
    ttrue(rc < 0);
    s = jsonToString(dest, 0, 0, 0);
    tmatch(s, "[1,2]");
    rFree(s);
    jsonFree(dest);
    jsonFree(src);
}

static void jsonBlendSelfTest()
{
    Json    *obj;
    char    *s;
    int     rc;

    //  Blending an object into itself must operate on a copy of the source
    obj = parse("{a: {b: 1}}");
    rc = jsonBlend(obj, 0, "copy", obj, 0, "a", 0);
    ttrue(rc == 0);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{a:{b:1},copy:{b:1}}");
    rFree(s);
    jsonFree(obj);

    //  Blend flags must not be applied as parse flags to the source copy
    obj = parse("{a: {b: 1}}");
    rc = jsonBlend(obj, 0, "copy", obj, 0, "a", JSON_COMBINE);
    ttrue(rc == 0);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{a:{b:1},copy:{b:1}}");
    rFree(s);
    jsonFree(obj);
}

static void jsonBlendNullTest()
{
    Json    *src;
    char    *s;

    //  A null destination is rejected rather than dereferenced
    src = parse("{a: 1}");
    ttrue(jsonBlend(0, 0, 0, src, 0, 0, 0) < 0);

    //  A null source blends nothing
    s = jsonToString(src, 0, 0, 0);
    ttrue(jsonBlend(src, 0, 0, 0, 0, 0, 0) == 0);
    tmatch(s, "{a:1}");
    rFree(s);
    jsonFree(src);
}

/*
    A container in an object position with no key used to parse into a nameless
    property, which jsonBlend then dereferenced under JSON_COMBINE. Issue 15.
 */
static void jsonBlendNullNameTest()
{
    Json     *dest, *src;
    JsonNode *node;
    char     *s;

    //  The document that produced the crash no longer parses
    ttrue(parseFail("{{}}"));
    ttrue(parseFail("{[1]}"));
    ttrue(parseFail("{a: 1, {}}"));

    //  A container in an array position, or with a key, is still fine
    ttrue(parseSuccess("[{}]"));
    ttrue(parseSuccess("[[1],[2]]"));
    ttrue(parseSuccess("{a: {b: 1}}"));
    ttrue(parseSuccess("{a: [{b: 1}]}"));

    //  A null name reaching the blend through the node API is tolerated
    dest = parse("{a: 1}");
    src = parse("{b: 2}");
    node = jsonGetNode(src, 0, "b");
    ttrue(node != 0);
    node->name = 0;
    node->allocatedName = 0;
    ttrue(jsonBlend(dest, 0, 0, src, 0, 0, JSON_COMBINE) == 0);
    s = jsonToString(dest, 0, 0, 0);
    ttrue(s != 0);
    rFree(s);
    jsonFree(dest);
    jsonFree(src);
}

/*
    Growing the destination node array reallocates it, invalidating every
    JsonNode pointer into that tree. Issue 21.
 */
static void jsonBlendReallocTest()
{
    Json *dest, *src;
    RBuf *buf;
    char *s;
    int  i, n;

    //  Well past ME_JSON_INC so the realloc actually moves the block
    n = 200;
    buf = rAllocBuf(0);
    rPutCharToBuf(buf, '[');
    for (i = 0; i < n; i++) {
        rPutStringToBuf(buf, i ? ",1" : "1");
    }
    rPutCharToBuf(buf, ']');

    //  A larger array over a smaller one forces insertNodes to grow
    dest = parse("[1]");
    src = parse(rGetBufStart(buf));
    ttrue(jsonBlend(dest, 0, 0, src, 0, 0, 0) == 0);
    ttrue(jsonGetLength(dest, 0, 0) == n);
    checkValue(dest, "[0]", "1");
    jsonFree(dest);
    jsonFree(src);

    //  And the append path over the same growth
    dest = parse("[1]");
    src = parse(rGetBufStart(buf));
    ttrue(jsonBlend(dest, 0, 0, src, 0, 0, JSON_APPEND) == 0);
    ttrue(jsonGetLength(dest, 0, 0) == n + 1);
    jsonFree(dest);
    jsonFree(src);

    //  A nested array grown under an object property
    dest = parse("{a: [1]}");
    src = jsonParseFmt("{a: %s}", rGetBufStart(buf));
    ttrue(src != 0);
    ttrue(jsonBlend(dest, 0, 0, src, 0, 0, 0) == 0);
    ttrue(jsonGetLength(dest, 0, "a") == n);
    s = jsonToString(dest, 0, "a[0]", 0);
    tmatch(s, "1");
    rFree(s);
    jsonFree(dest);
    jsonFree(src);

    rFreeBuf(buf);
}

static void jsonBlendLockedTest()
{
    Json        *dest, *src;
    RLogHandler prior;
    char        *s;
    int         rc;

    dest = parse("{a: 1}");
    src = parse("{b: 2}");
    jsonLock(dest);

    prior = rSetLogHandler(nullLog);
    rc = jsonBlend(dest, 0, 0, src, 0, 0, 0);
    rSetLogHandler(prior);
    ttrue(rc < 0);

    s = jsonToString(dest, 0, 0, 0);
    tmatch(s, "{a:1}");
    rFree(s);

    jsonUnlock(dest);
    jsonFree(dest);
    jsonFree(src);
}

static void jsonBlendRecursionTest()
{
    Json        *dest, *src;
    RLogHandler prior;
    RBuf        *buf;
    int         i, rc;

    //  Build a source nested deeper than the blend recursion limit
    buf = rAllocBuf(0);
    for (i = 0; i < 40; i++) {
        rPutStringToBuf(buf, "{a:");
    }
    rPutStringToBuf(buf, "1");
    for (i = 0; i < 40; i++) {
        rPutCharToBuf(buf, '}');
    }
    src = parse(rGetBufStart(buf));
    ttrue(src != 0);
    rFreeBuf(buf);

    dest = parse("{}");
    prior = rSetLogHandler(nullLog);
    rc = jsonBlend(dest, 0, 0, src, 0, 0, 0);
    rSetLogHandler(prior);
    ttrue(rc < 0);

    jsonFree(dest);
    jsonFree(src);
}

/*
    Keep input before another property so exceeding the node limit would overwrite live data.
 */
static Json *makeBigResponse(int count)
{
    RBuf *buf;
    Json *json;
    int  i;

    buf = rAllocBuf((size_t) count * 2 + 128);
    rPutStringToBuf(buf, "{output:[{type:'function_call',name:'n',call_id:'c',arguments:[");
    for (i = 0; i < count; i++) {
        rPutStringToBuf(buf, i ? ",0" : "0");
    }
    rPutStringToBuf(buf, "]}]}");
    json = jsonParse(rBufToString(buf), 0);
    rFreeBuf(buf);
    return json;
}

static void blendPastCap(int flags)
{
    Json *request, *response;
    int  rc, before;

    request = parse("{input:[{role:'user',content:'hi'}],tools:[{type:'function',name:'t'}],model:'m'}");
    response = makeBigResponse(ME_JSON_MAX_NODES - 60);
    ttrue(request && response);
    before = request->count;

    rc = jsonBlend(request, 0, "input[$]", response, 0, "output[0]", flags);
    ttrue(rc < 0);
    ttrue(request->count <= request->size);
    ttrue(request->count >= before);
    checkValue(request, "model", "m");
    jsonFree(request);
    jsonFree(response);
}

static void jsonBlendNodeCapTest()
{
    blendPastCap(0);
    blendPastCap(JSON_APPEND);
}

int main(void)
{
    rInit(0, 0);
    jsonBlendKeyTest();
    jsonBlendTypeTest();
    jsonBlendSelfTest();
    jsonBlendNullTest();
    jsonBlendNullNameTest();
    jsonBlendReallocTest();
    jsonBlendLockedTest();
    jsonBlendRecursionTest();
    jsonBlendNodeCapTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
