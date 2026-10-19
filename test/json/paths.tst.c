/*
    paths.c.tst - Unit tests for property path resolution

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonArrayPathTest()
{
    Json    *obj;

    obj = parse("{words: ['one', 'two', 'three'], pairs: [[1,2],[3,4]]}");

    //  A non-numeric array term matches an element by value
    checkValue(obj, "words.two", "two");
    checkValue(obj, "words[three]", "three");
    tmatch(jsonGet(obj, 0, "words.four", "missing"), "missing");

    //  Numeric terms index the array
    checkValue(obj, "pairs[1][0]", "3");

    //  An index beyond the element count is not found even though the node
    //  count of the array is larger
    tmatch(jsonGet(obj, 0, "pairs[2]", "missing"), "missing");
    tmatch(jsonGet(obj, 0, "pairs[3]", "missing"), "missing");
    tmatch(jsonGet(obj, 0, "pairs[-1]", "missing"), "missing");

    jsonFree(obj);
}

static void jsonPathSyntaxTest()
{
    Json    *obj;

    obj = parse("{name: 'John', user: {key: 'value'}}");

    //  Leading and trailing separators are ignored
    checkValue(obj, ".name", "John");
    checkValue(obj, "name.", "John");
    checkValue(obj, "user.key", "value");

    //  Quoted terms have their quotes stripped
    checkValue(obj, "user['key']", "value");
    checkValue(obj, "user[\"key\"]", "value");

    //  Dereferencing a scalar is not possible
    tmatch(jsonGet(obj, 0, "name.sub", "missing"), "missing");
    tmatch(jsonGet(obj, 0, "name[0]", "missing"), "missing");

    jsonFree(obj);
}

static void jsonSetPathTest()
{
    Json    *obj;
    char    *s;
    int     rc;

    //  Set on an empty tree must create the root node
    obj = jsonAlloc();
    ttrue(obj->count == 0);
    rc = jsonSet(obj, 0, "a.b", "1", 0);
    ttrue(rc > 0);
    checkValue(obj, "a.b", "1");
    jsonFree(obj);

    //  An intervening scalar is converted to a container
    obj = parse("{a: 'scalar'}");
    rc = jsonSet(obj, 0, "a.b", "1", 0);
    ttrue(rc > 0);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{a:{b:1}}");
    rFree(s);
    jsonFree(obj);

    //  The '$' term appends a new array element
    obj = parse("{list: [1]}");
    rc = jsonSet(obj, 0, "list[$]", "2", 0);
    ttrue(rc > 0);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{list:[1,2]}");
    rFree(s);

    //  And can append a container
    rc = jsonSet(obj, 0, "list[$]", 0, JSON_OBJECT);
    ttrue(rc > 0);
    rc = jsonSet(obj, 0, "list[2].name", "third", 0);
    ttrue(rc > 0);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{list:[1,2,{name:'third'}]}");
    rFree(s);
    jsonFree(obj);
}

/*
    Whatever key jsonSet accepted must read back through jsonGet. Issues 17, 18, 19.
 */
static void jsonSetGetAgreeTest()
{
    Json  *json;
    cchar *keys[] = {
        "a", "a.b", "x.y.z", "arr[0]", "arr[5]", "b.c[1]", "a[", "a]", "a..b",
        "a.", ".a", "[0]", "a[0][1]", "a.b.", "'q'", "a['q']", "a[\"q\"]", 0
    };
    cchar *rejects[] = { "[]", "", ".", "..", "[", "]", 0 };
    int   i, rc;

    for (i = 0; keys[i]; i++) {
        json = parse("{a: 1, b: {c: [1,2]}, arr: ['one','two']}");
        rc = jsonSet(json, 0, keys[i], "sentinel", JSON_STRING);
        if (rc < 0) {
            tfail("jsonSet rejected key: %s", keys[i]);
        } else if (!smatch(jsonGet(json, 0, keys[i], 0), "sentinel")) {
            tfail("set/get disagree for key: %s", keys[i]);
        } else {
            ttrue(rc >= 0);
        }
        jsonFree(json);
    }
    //  A key that names no property is rejected, as the empty key already was
    for (i = 0; rejects[i]; i++) {
        json = parse("{a: 1}");
        if (jsonSet(json, 0, rejects[i], "sentinel", JSON_STRING) >= 0) {
            tfail("jsonSet accepted a key that names nothing: \"%s\"", rejects[i]);
        } else {
            ttrue(1);
        }
        jsonFree(json);
    }
}

/*
    An array index past the end fills the gap so the value lands where asked.
 */
static void jsonSparseArrayTest()
{
    Json *json;
    char *s;

    json = parse("{arr: ['one','two']}");
    ttrue(jsonSet(json, 0, "arr[5]", "V", JSON_STRING) >= 0);
    checkValue(json, "arr[5]", "V");
    checkValue(json, "arr[0]", "one");
    checkValue(json, "arr[1]", "two");
    ttrue(jsonGetLength(json, 0, "arr") == 6);
    s = jsonToString(json, 0, "arr", JSON_JSON);
    tmatch(s, "[\"one\",\"two\",null,null,null,\"V\"]");
    rFree(s);
    jsonFree(json);

    //  An index that cannot be filled is rejected rather than appended, so that
    //  jsonGet -- which can never resolve an index past the element count -- agrees
    json = parse("{arr: ['one','two']}");
    ttrue(jsonSet(json, 0, "arr[99999999]", "V", JSON_STRING) < 0);
    ttrue(jsonSet(json, 0, "arr[044444444444444444444444444]", "V", JSON_STRING) < 0);
    ttrue(jsonGetLength(json, 0, "arr") == 2);
    jsonFree(json);

    //  A large gap is filled in one insert, not one per element. The per-element
    //  loop was quadratic -- arr[99999] took 5.1 seconds -- and could stop part way
    //  on hitting the node limit while still reporting success.
    json = parse("{arr: []}");
    ttrue(jsonSet(json, 0, "arr[10000]", "V", JSON_STRING) >= 0);
    ttrue(jsonGetLength(json, 0, "arr") == 10001);
    checkValue(json, "arr[10000]", "V");
    //  jsonGet returns the caller's default for a null primitive, so check the type
    checkType(json, "arr[0]", JSON_PRIMITIVE);
    jsonFree(json);

    //  A gap that cannot be filled leaves the array untouched
    json = parse("{arr: ['one','two']}");
    ttrue(jsonSet(json, 0, "arr[99999]", "V", JSON_STRING) < 0);
    ttrue(jsonGetLength(json, 0, "arr") == 2);
    jsonFree(json);

    //  The next slot is still a plain append
    json = parse("{arr: ['one','two']}");
    ttrue(jsonSet(json, 0, "arr[2]", "V", JSON_STRING) >= 0);
    ttrue(jsonGetLength(json, 0, "arr") == 3);
    jsonFree(json);
}

/*
    A quoted term inside brackets is a property name, not an index.
 */
static void jsonQuotedSelectorTest()
{
    Json *json;
    char *s;

    //  Converting a scalar through a quoted selector makes an object, as a.q does
    json = parse("{a: 1}");
    ttrue(jsonSet(json, 0, "a['q']", "V", JSON_STRING) >= 0);
    s = jsonToString(json, 0, 0, 0);
    tmatch(s, "{a:{q:'V'}}");
    rFree(s);
    checkValue(json, "a['q']", "V");
    checkValue(json, "a.q", "V");
    jsonFree(json);

    json = parse("{a: 1}");
    ttrue(jsonSet(json, 0, "a[\"q\"]", "V", JSON_STRING) >= 0);
    s = jsonToString(json, 0, 0, 0);
    tmatch(s, "{a:{q:'V'}}");
    rFree(s);
    jsonFree(json);

    //  An unquoted term inside brackets is still an index
    json = parse("{a: 1}");
    ttrue(jsonSet(json, 0, "a[0]", "V", JSON_STRING) >= 0);
    s = jsonToString(json, 0, 0, 0);
    tmatch(s, "{a:['V']}");
    rFree(s);
    jsonFree(json);

    //  Reading a quoted selector is unchanged
    json = parse("{a: {q: 'X'}}");
    checkValue(json, "a['q']", "X");
    jsonFree(json);

    //  An empty term is rejected on write, as it already is on read.
    json = parse("{a: 1}");
    ttrue(jsonSet(json, 0, "obj['']", "S", JSON_STRING) < 0);
    ttrue(jsonSet(json, 0, "a[\"\"]", "S", JSON_STRING) < 0);
    jsonFree(json);

    //  A lone quote is not a matched pair.
    json = parse("{a: 1}");
    ttrue(jsonSet(json, 0, "]bd.']bd.2", "S", JSON_STRING) >= 0);
    checkValue(json, "]bd.']bd.2", "S");
    jsonFree(json);
}

/*
    A non-numeric bracket term on an array is a lookup by value, which names no
    slot to create.
 */
static void jsonValueLookupTest()
{
    Json *json;
    char *s;

    //  Reading by element value still works
    json = parse("{arr: ['one','two']}");
    checkValue(json, "arr[one]", "one");
    checkValue(json, "arr.two", "two");

    //  Writing through the same term is rejected rather than appending
    ttrue(jsonSet(json, 0, "arr[x]", "S", JSON_STRING) < 0);
    s = jsonToString(json, 0, "arr", 0);
    tmatch(s, "['one','two']");
    rFree(s);

    //  And when the term matches, the write would destroy the value that named
    //  it, so it is rejected too and the element is untouched.
    ttrue(jsonSet(json, 0, "arr[two]", "S", JSON_STRING) < 0);
    ttrue(jsonSet(json, 0, "arr.two", "S", JSON_STRING) < 0);
    ttrue(jsonSet(json, 0, "arr[two", "S", JSON_STRING) < 0);
    checkValue(json, "arr[two]", "two");
    s = jsonToString(json, 0, "arr", 0);
    tmatch(s, "['one','two']");
    rFree(s);

    //  The append term and an index are unaffected
    ttrue(jsonSet(json, 0, "arr[$]", "S", JSON_STRING) >= 0);
    ttrue(jsonSet(json, 0, "arr[0]", "T", JSON_STRING) >= 0);
    checkValue(json, "arr[0]", "T");
    jsonFree(json);
}

/*
    A scalar parent is converted to an object to hold a property.
 */
static void jsonScalarParentTest()
{
    Json  *json;
    char  *s;
    cchar *docs[] = { "n", "'str'", "123", "true", "null", 0 };
    int   i;

    for (i = 0; docs[i]; i++) {
        json = parse(docs[i]);
        ttrue(jsonSet(json, 0, "abc", "S", JSON_STRING) >= 0);
        checkValue(json, "abc", "S");
        s = jsonToString(json, 0, 0, 0);
        tmatch(s, "{abc:'S'}");
        rFree(s);
        jsonFree(json);
    }
    //  A nested scalar is converted the same way
    json = parse("{a: 1}");
    ttrue(jsonSet(json, 0, "a.b.c", "S", JSON_STRING) >= 0);
    checkValue(json, "a.b.c", "S");
    jsonFree(json);
}

/*
    A path write that changes a node's type must not strand its children as
    nameless siblings.
 */
static void jsonTypeChangeTest()
{
    Json *json, *again;
    char *s;

    //  A container leaf replaced by a value loses its whole subtree
    json = parse("{b: {c: [1, 2]}}");
    ttrue(jsonSet(json, 0, "b[c", "S", JSON_STRING) >= 0);
    s = jsonToString(json, 0, 0, 0);
    tmatch(s, "{b:{c:'S'}}");
    rFree(s);
    jsonFree(json);

    json = parse("{a: {x: 1, y: {z: 2}}, keep: 3}");
    ttrue(jsonSet(json, 0, "a", "flat", JSON_STRING) >= 0);
    s = jsonToString(json, 0, 0, 0);
    tmatch(s, "{a:'flat',keep:3}");
    rFree(s);
    ttrue(jsonGetLength(json, 0, 0) == 2);
    jsonFree(json);

    //  Dot syntax on an array is an index, as it is on the read path, not a
    //  coercion of the array to an object
    json = parse("{arr: ['one', 'two']}");
    ttrue(jsonSet(json, 0, "arr.1", "S", JSON_STRING) >= 0);
    checkValue(json, "arr[1]", "S");
    checkValue(json, "arr.0", "one");
    s = jsonToString(json, 0, 0, 0);
    tmatch(s, "{arr:['one','S']}");
    rFree(s);
    jsonFree(json);

    //  A sibling is preserved by an ordinary leaf update
    json = parse("{a: {b: 1, c: 2}}");
    ttrue(jsonSet(json, 0, "a.b", "S", JSON_STRING) >= 0);
    checkValue(json, "a.c", "2");
    jsonFree(json);

    //  No sequence of set calls yields a nameless property: whatever is produced
    //  must serialize to JSON the strict parser accepts
    json = parse("{arr: ['one', 'two'], b: {c: [1, 2]}}");
    jsonSet(json, 0, "arr.x.y", "S", JSON_STRING);
    jsonSet(json, 0, "b.c", "S", JSON_STRING);
    jsonSet(json, 0, "b", "S", JSON_STRING);
    s = jsonToString(json, 0, 0, JSON_JSON);
    again = jsonParse(s, JSON_STRICT_PARSE);
    ttrue(again != 0);
    jsonFree(again);
    rFree(s);
    jsonFree(json);
}

int main(void)
{
    rInit(0, 0);
    jsonArrayPathTest();
    jsonPathSyntaxTest();
    jsonSetPathTest();
    jsonSetGetAgreeTest();
    jsonSparseArrayTest();
    jsonQuotedSelectorTest();
    jsonValueLookupTest();
    jsonScalarParentTest();
    jsonTypeChangeTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
