/*
    coverage.c.tst - Additional unit tests for JSON library for code coverage

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#include "test.h"

/************************************ Code ************************************/

static void testParseNumbers()
{
#if FUTURE
    Json *obj;
    // Scientific notation
    obj = parse("{ \"value\": 1e5 }");
    ttrue(obj != 0);
    checkValue(obj, "value", "100000");
    jsonFree(obj);

    obj = parse("{ \"value\": 1.2E-3 }");
    ttrue(obj != 0);
    // Note: this check depends on floating point to string formatting.
    ttrue(stod(jsonGet(obj, 0, "value", 0)) == 0.0012);
    jsonFree(obj);

    obj = parse("{ \"value\": -5.2e+2 }");
    ttrue(obj != 0);
    checkValue(obj, "value", "-520");
    jsonFree(obj);

    // Invalid numbers
    ttrue(!quiet("{ \"value\": 1.2.3 }"));
    ttrue(!quiet("{ \"value\": 1e }"));
    ttrue(!quiet("{ \"value\": --1 }"));
#endif
    // A leading plus is not standard JSON
    ttrue(!quiet("{ \"value\": +1 }"));
}

static void testParseStrings()
{
    Json *obj;
    char *s;

    // All valid escape sequences
    obj = parse("{ \"value\": \"\\\"\\\\\\/\\b\\f\\n\\r\\t\" }");
    ttrue(obj != 0);
    s = jsonToString(obj, 0, 0, JSON_JSON);
    tmatch(s, "{\"value\":\"\\\"\\\\/\\b\\f\\n\\r\\t\"}");
    rFree(s);
    jsonFree(obj);

    // Unicode escape sequences
    obj = parse("{ \"value\": \"\\u0041\\u0042\\u0043\" }"); // "ABC"
    ttrue(obj != 0);
    checkValue(obj, "value", "ABC");
    jsonFree(obj);

#if FUTURE
    // Unicode surrogate pairs (e.g. Pile of Poo emoji 💩)
    obj = parse("{ \"value\": \"\\uD83D\\uDCA9\" }");
    ttrue(obj != 0);
    // This check assumes the test source file is UTF-8 encoded
    checkValue(obj, "value", "💩");
    jsonFree(obj);
#endif

    // Invalid unicode
    ttrue(!quiet("{ \"value\": \"\\uDEFG\" }"));
    ttrue(!quiet("{ \"value\": \"\\u123\" }"));

    // Invalid escape sequence
    ttrue(!quiet("{ \"value\": \"\\q\" }"));

#if FUTURE
    // String with null byte - we don't support this
    obj = parse("{ \"value\": \"hello\\u0000world\" }");
    ttrue(obj != 0);
    cchar *val = jsonGet(obj, 0, "value", 0);
    ttrue(slen(val) == 11);
    ttrue(val[5] == '\0');
    ttrue(memcmp(val, "hello\0world", 11) == 0);
    jsonFree(obj);
#endif
}

static void testParseStructure()
{
    Json *obj;
    char *error;

    // Trailing commas (non-standard but often supported by relaxed parsers)
    obj = parse("{ \"a\": 1, \"b\": 2, }");
    ttrue(obj != 0);
    checkJson(obj, "{a:1,b:2}", 0);
    jsonFree(obj);

    obj = parse("[ 1, 2, 3, ]");
    ttrue(obj != 0);
    checkJson(obj, "[1,2,3]", 0);
    jsonFree(obj);

#if FUTURE
    // Empty key - not supported
    obj = parse("{ \"\": 42 }");
    ttrue(obj != 0);
    checkValue(obj, "\"\"", "42");
    jsonFree(obj);
#endif

    // Deeply nested structure (tests for recursion limits)
    obj = parse("[[[[[[[[[[[]]]]]]]]]]]");
    ttrue(obj != 0);
    checkJson(obj, "[[[[[[[[[[[]]]]]]]]]]]", 0);
    jsonFree(obj);

    // More malformed JSON error reporting
    obj = jsonParseString("{'a':1, 'b'", &error, 0);
    ttrue(obj == 0);
    ttrue(scontains(error, "JSON Parse Error"));
    rFree(error);

#if FUTURE
    obj = jsonParseString("{'a':,}", &error, 0);
    ttrue(obj == 0);
    ttrue(scontains(error, "JSON Parse Error"));
    rFree(error);

    obj = jsonParseString("{'a' 'b'}", &error, 0);
    ttrue(obj == 0);
    ttrue(scontains(error, "JSON Parse Error"));
    rFree(error);

    obj = jsonParseString("['a' 'b']", &error, 0);
    ttrue(obj == 0);
    ttrue(scontains(error, "JSON Parse Error"));
    rFree(error);
#endif
}

static void testApiEdgeCases()
{
    Json *obj;
    char *s;
    int rc;

    // jsonSet creating nested properties on an empty object
    obj = parse("{}");
    rc = jsonSet(obj, 0, "a.b.c", "value", 0);
    ttrue(rc > 0);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{a:{b:{c:'value'}}}");
    rFree(s);
    jsonFree(obj);

#if FUTURE
    // jsonSet on array with index beyond length, should fill with nulls
    obj = parse("[]");
    rc = jsonSet(obj, 0, "[2]", "value", 0);
    ttrue(rc > 0);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "[null,null,'value']");
    rFree(s);
    jsonFree(obj);
#endif

    // jsonRemove on non-existent nested path
    obj = parse("{ a: { d: 1 } }");
    rc = jsonRemove(obj, 0, "a.b.c");
    ttrue(rc < 0); // Should fail as path does not fully exist
    checkJson(obj, "{a:{d:1}}", 0);
    jsonFree(obj);
}

static void testTemplateCoverage()
{
    Json *obj;
    char *text;

    obj = jsonParse("{ str: 'string', nil: null, obj: {a:1}, arr: [1,2] }", 0);

    // Test template with null value
    text = jsonTemplate(obj, "Value is ${nil}", 0);
    tmatch(text, "Value is ");
    rFree(text);

#if FUTURE
    // Test template with object value. Should be stringified.
    text = jsonTemplate(obj, "Value is ${obj}", 0);
    tmatch(text, "Value is {a:1}");
    rFree(text);

    // Test template with array value. Should be stringified.
    text = jsonTemplate(obj, "Value is ${arr}", 0);
    tmatch(text, "Value is [1,2]");
    rFree(text);
#endif

    // Test empty token with keep=1
    text = jsonTemplate(obj, "Empty: ${}", 1);
    tmatch(text, "Empty: ${}");
    rFree(text);

    jsonFree(obj);
}

#if FUTURE
static void testIterateStability()
{
    Json     *json;
    JsonNode *child;
    RList    *keys = rAllocList(-1, 0);

    json = parse("{item: {first: 1, second: 2, third: 3}}");
    for (ITERATE_JSON_KEY(json, 0, "item", child, nid)) {
        rAddItem(keys, sclone(child->name));
        if (smatch(child->name, "second")) {
            // Add a new key. A stable iterator should not visit this new key in the current loop.
            jsonSet(json, 0, "item.fourth", "4", JSON_PRIMITIVE);
        }
    }
    // Check that we only iterated over the original keys
    ttrue(rGetListLength(keys) == 3);
    ttrue(rLookupStringItem(keys, "first") >= 0);
    ttrue(rLookupStringItem(keys, "second") >= 0);
    ttrue(rLookupStringItem(keys, "third") >= 0);
    ttrue(rLookupStringItem(keys, "fourth") < 0);

    // And that the new key is present in the object now
    checkValue(json, "item.fourth", "4");

    rFreeList(keys);
    jsonFree(json);
}
#endif


static void jsonCoverage() 
{
    testParseNumbers();
    testParseStrings();
    testParseStructure();
    testApiEdgeCases();
    testTemplateCoverage();
#if FUTURE
    testIterateStability();
#endif
}

int main(void)
{
    rInit(0, 0);
    jsonCoverage();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */ 