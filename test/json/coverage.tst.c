/*
    coverage.c.tst - Additional unit tests for JSON library for code coverage

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#include "test.h"

/************************************ Code ************************************/

static void testParseNumbers()
{
    Json *obj;

    // Numbers are stored verbatim and convert through both accessors (issue 30)
    obj = parse("{ a: 1e5, b: 1.2E-3, c: -5.2e+2, d: 0x10, e: 2.5e3, f: 1.5, g: -2, h: 9007199254740993 }");
    ttrue(obj != 0);
    checkValue(obj, "a", "1e5");
    checkValue(obj, "b", "1.2E-3");
    checkValue(obj, "c", "-5.2e+2");
    ttrue(jsonGetDouble(obj, 0, "a", 0) == 100000.0);
    ttrue(jsonGetDouble(obj, 0, "b", 0) == 0.0012);
    ttrue(jsonGetNum(obj, 0, "a", 0) == 100000);
    ttrue(jsonGetNum(obj, 0, "c", 0) == -520);
    ttrue(jsonGetNum(obj, 0, "d", 0) == 16);
    ttrue(jsonGetNum(obj, 0, "e", 0) == 2500);
    ttrue(jsonGetNum(obj, 0, "f", 0) == 1);
    ttrue(jsonGetNum(obj, 0, "g", 0) == -2);
    // A plain integer must not lose precision through the double path
    ttrue(jsonGetNum(obj, 0, "h", 0) == 9007199254740993LL);
    ttrue(jsonGetNum(obj, 0, "missing", 7) == 7);
    ttrue(jsonGetInt(obj, 0, "a", 0) == 100000);
    ttrue(jsonGetInt(obj, 0, "d", 0) == 16);
    jsonFree(obj);

    // An exponent beyond int64 saturates rather than overflowing
    obj = parse("{ big: 1e30, neg: -1e30 }");
    ttrue(jsonGetNum(obj, 0, "big", 0) == MAXINT64);
    ttrue(jsonGetNum(obj, 0, "neg", 0) == -MAXINT64 - 1);
    jsonFree(obj);

    // In relaxed mode a malformed number is an unquoted string. Strict mode rejects it: see types.tst.c
    obj = parse("{ a: 1.2.3, b: 1e, c: --1 }");
    ttrue(obj != 0);
    ttrue(jsonGetType(obj, 0, "a") == JSON_STRING);
    ttrue(jsonGetType(obj, 0, "b") == JSON_STRING);
    ttrue(jsonGetType(obj, 0, "c") == JSON_STRING);
    jsonFree(obj);

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

    /*
        A \u escape decodes to UTF-8 for every code point, not just the ASCII range (issue 29).
        Two-byte, three-byte and a surrogate pair, in a value, in a key, and in the middle of
        other text, since the decoder shifts the string in place.
     */
    obj = parse("{ latin: \"\\u00e9\", cjk: \"\\u4e2d\", emoji: \"\\uD83D\\uDCA9\", mixed: \"a\\u00e9b\\u4e2dc\", \"\\u4e2d\": 1 }");
    ttrue(obj != 0);
    checkValue(obj, "latin", "é");
    checkValue(obj, "cjk", "中");
    checkValue(obj, "emoji", "💩");
    checkValue(obj, "mixed", "aéb中c");
    checkValue(obj, "中", "1");
    ttrue(slen(jsonGet(obj, 0, "latin", 0)) == 2);
    ttrue(slen(jsonGet(obj, 0, "cjk", 0)) == 3);
    ttrue(slen(jsonGet(obj, 0, "emoji", 0)) == 4);
    // The decoded text is plain UTF-8 on the way out
    s = jsonToString(obj, 0, "latin", JSON_JSON);
    tmatch(s, "\"é\"");
    rFree(s);
    jsonFree(obj);

    // A surrogate that does not form a pair cannot be encoded and is an error
    ttrue(!quiet("{ \"value\": \"\\uD83D\" }"));
    ttrue(!quiet("{ \"value\": \"\\uDCA9\" }"));
    ttrue(!quiet("{ \"value\": \"\\uD83D\\u0041\" }"));
    ttrue(!quiet("{ \"value\": \"\\uD83D\\uD83D\" }"));

    // Invalid unicode
    ttrue(!quiet("{ \"value\": \"\\uDEFG\" }"));
    ttrue(!quiet("{ \"value\": \"\\u123\" }"));

    // Invalid escape sequence
    ttrue(!quiet("{ \"value\": \"\\q\" }"));

    // An embedded NUL (\u0000) is not supported: the tree is NUL-terminated text
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

    // An empty key parses and serializes, but no path can address it: the empty term is rejected (issue 25)
    obj = parse("{ \"\": 42 }");
    ttrue(obj != 0);
    checkJson(obj, "{'':42}", 0);
    ttrue(jsonGet(obj, 0, "", 0) == 0 || jsonGetId(obj, 0, "") == 0);
    jsonFree(obj);

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

    // A colon with no value before the comma is an error in both modes, as it is before a brace (issue 31)
    obj = jsonParseString("{'a':,}", &error, 0);
    ttrue(obj == 0);
    ttrue(scontains(error, "Missing property value"));
    rFree(error);

    obj = jsonParseString("{\"a\":,}", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(scontains(error, "Missing property value"));
    rFree(error);

    obj = jsonParseString("{'a':}", &error, 0);
    ttrue(obj == 0);
    ttrue(scontains(error, "Missing property value"));
    rFree(error);

    obj = jsonParseString("{'a':, 'b': 1}", &error, 0);
    ttrue(obj == 0);
    rFree(error);

    // A missing colon is a strict-mode error
    obj = jsonParseString("{\"a\" \"b\"}", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    rFree(error);

    obj = jsonParseString("['a' 'b']", &error, 0);
    ttrue(obj == 0);
    ttrue(scontains(error, "JSON Parse Error"));
    rFree(error);
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

    // jsonSet on an array with an index beyond its length fills the gap with nulls (issue 17)
    obj = parse("[]");
    rc = jsonSet(obj, 0, "[2]", "value", 0);
    ttrue(rc > 0);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "[null,null,'value']");
    rFree(s);
    jsonFree(obj);

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

    obj = jsonParse(
        "{ str: 'string', nil: null, obj: {a:1}, arr: [1,2], nested: {x:{y:2}}, eobj: {}, earr: [] }", 0);

    // Test template with null value
    text = jsonTemplate(obj, "Value is ${nil}", 0);
    tmatch(text, "Value is ");
    rFree(text);

    text = jsonTemplate(obj, "V=${obj}", 0);
    tmatch(text, "V={\"a\":1}");
    rFree(text);

    text = jsonTemplate(obj, "A=${arr}", 0);
    tmatch(text, "A=[1,2]");
    rFree(text);

    text = jsonTemplate(obj, "N=${nested}", 0);
    tmatch(text, "N={\"x\":{\"y\":2}}");
    rFree(text);

    text = jsonTemplate(obj, "E=${eobj} F=${earr}", 0);
    tmatch(text, "E={} F=[]");
    rFree(text);

    // Test empty token with keep=1
    text = jsonTemplate(obj, "Empty: ${}", 1);
    tmatch(text, "Empty: ${}");
    rFree(text);

    jsonFree(obj);
}

/*
    Modifying the tree inside ITERATE_JSON is unsupported by design (see json.h), so there is
    no iterator-stability test here.
 */

static void jsonCoverage()
{
    testParseNumbers();
    testParseStrings();
    testParseStructure();
    testApiEdgeCases();
    testTemplateCoverage();
}

int main(void)
{
    rInit(0, 0);
    jsonCoverage();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
