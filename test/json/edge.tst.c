/*
    edge.c.tst - Unit tests for JSON edge cases and error conditions

    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonParseEdgeCasesTest()
{
    Json    *obj;
    char    *error;

    // Test with very large numbers
    obj = parse("{big: 9223372036854775807}");
    ttrue(obj != 0);
    ttrue(jsonGetNum(obj, 0, "big", 0) == 9223372036854775807LL);
    jsonFree(obj);

    // Test with negative numbers
    obj = parse("{neg: -42, negFloat: -3.14}");
    ttrue(obj != 0);
    ttrue(jsonGetInt(obj, 0, "neg", 0) == -42);
    ttrue(jsonGetDouble(obj, 0, "negFloat", 0) == -3.14);
    jsonFree(obj);

    // Test with zero
    obj = parse("{zero: 0, zeroFloat: 0.0}");
    ttrue(obj != 0);
    ttrue(jsonGetInt(obj, 0, "zero", -1) == 0);
    ttrue(jsonGetDouble(obj, 0, "zeroFloat", -1.0) == 0.0);
    jsonFree(obj);

    // Test deeply nested structures
    obj = parse("{\
        a: { b: { c: { d: { e: { f: { g: { h: { i: { j: 'deep' } } } } } } } } } \
    }");
    ttrue(obj != 0);
    checkValue(obj, "a.b.c.d.e.f.g.h.i.j", "deep");
    jsonFree(obj);

    // Test large arrays
    obj = parse("[\
        1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,\
        21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,\
        41,42,43,44,45,46,47,48,49,50\
    ]");
    ttrue(obj != 0);
    checkValue(obj, "[0]", "1");
    checkValue(obj, "[49]", "50");
    jsonFree(obj);

    // Test empty structures
    obj = parse("{}");
    ttrue(obj != 0);
    ttrue(jsonGet(obj, 0, "anything", 0) == 0);
    jsonFree(obj);

    obj = parse("[]");
    ttrue(obj != 0);
    ttrue(jsonGet(obj, 0, "[0]", 0) == 0);
    jsonFree(obj);

    // Test malformed JSON variations
    obj = jsonParseString("{key: 'unclosed string", &error, 0);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    obj = jsonParseString("{key: value}", &error, 0); // unquoted string value (valid in JSON6)
    ttrue(obj != 0);
    ttrue(error == 0);
    checkValue(obj, "key", "value");
    jsonFree(obj);

    obj = jsonParseString("{key: value with spaces}", &error, 0); // unquoted string with spaces (should fail)
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    obj = jsonParseString("{,}", &error, 0); // comma without key acceptable
    ttrue(obj);
    ttrue(error == 0);
    rFree(error);

    obj = jsonParseString("[,]", &error, 0); // comma in array 
    ttrue(obj);
    ttrue(error == 0);
    ttrue(jsonGetLength(obj, 0, NULL) == 0);
    rFree(error);

    obj = jsonParseString("{key: 'mixed quotes\"}", &error, 0);
    ttrue(obj == 0);
    ttrue(error);
    rFree(error);
}

static void jsonStrictModeTest()
{
    Json    *obj;
    char    *error;

    // Test JSON5 features that should fail in strict mode
    obj = jsonParseString("{unquoted: 'value'}", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0); // Should fail with unquoted key
    ttrue(error != 0);
    rFree(error);

    obj = jsonParseString("{\"key\": 'single quotes'}", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0); // Should fail with single quotes in strict mode
    ttrue(error != 0);
    rFree(error);

    obj = jsonParseString("/* comment */ {\"key\": \"value\"}", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0); // Should fail with comments in strict mode
    ttrue(error != 0);
    rFree(error);

    // Test valid strict JSON
    obj = jsonParseString("{\"key\": \"value\", \"number\": 42}", &error, JSON_STRICT_PARSE);
    ttrue(obj != 0); // Should succeed
    ttrue(error == 0);
    checkValue(obj, "key", "value");
    checkValue(obj, "number", "42");
    jsonFree(obj);
}

static void jsonMemoryLimitsTest()
{
    Json    *obj;
    char    largeJson[10000];
    int     i;

    // Build a large JSON string to test memory handling
    strcpy(largeJson, "{\"items\": [");
    for (i = 0; i < 100; i++) {
        char item[100];
        snprintf(item, sizeof(item), "%s{\"id\": %d, \"name\": \"item_%d\"}",
                (i > 0) ? "," : "", i, i);
        strcat(largeJson, item);
    }
    strcat(largeJson, "]}");

    obj = parse(largeJson);
    ttrue(obj != 0);
    checkValue(obj, "items[0].id", "0");
    checkValue(obj, "items[99].name", "item_99");
    jsonFree(obj);
}

static void jsonSpecialCharactersTest()
{
    Json    *obj;
    char    *result;

    // Test unicode characters
    obj = parse("{\"unicode\": \"\\u0041\\u0042\\u0043\"}"); // ABC
    ttrue(obj != 0);
    checkValue(obj, "unicode", "ABC");
    jsonFree(obj);

    // Test control characters
    obj = parse("{\"controls\": \"\\b\\f\\n\\r\\t\"}");
    ttrue(obj != 0);
    result = jsonToString(obj, 0, 0, JSON_ENCODE);
    ttrue(scontains(result, "\\b"));
    ttrue(scontains(result, "\\f"));
    ttrue(scontains(result, "\\n"));
    ttrue(scontains(result, "\\r"));
    ttrue(scontains(result, "\\t"));
    jsonFree(obj);
    rFree(result);

    // Test backslashes and quotes
    obj = parse("{\"path\": \"C:\\\\Program Files\\\\\", \"quote\": \"Say \\\"Hello\\\"\"}");
    ttrue(obj != 0);
    checkValue(obj, "path", "C:\\Program Files\\");
    checkValue(obj, "quote", "Say \"Hello\"");
    jsonFree(obj);
}

static void jsonNullAndUndefinedTest()
{
    Json    *obj;
    cchar   *value;

    obj = parse("{\"nullValue\": null, \"undefinedValue\": undefined}");
    ttrue(obj != 0);

    value = jsonGet(obj, 0, "nullValue", "default");
    tmatch(value, "default"); // jsonGet returns default value for null JSON values

    value = jsonGet(obj, 0, "undefinedValue", "default");
    tmatch(value, "undefined");

    // Test type checking
    ttrue(jsonGetType(obj, 0, "nullValue") == JSON_PRIMITIVE);
    ttrue(jsonGetType(obj, 0, "undefinedValue") == JSON_PRIMITIVE);

    jsonFree(obj);
}

static void jsonErrorRecoveryTest()
{
    Json    *obj;
    int     rc;

    // Test operations on NULL objects
    obj = NULL;
    tmatch(jsonGet(obj, 0, "key", "default"), "default"); // Should return default value for NULL object
    ttrue(jsonGetBool(obj, 0, "key", true) == 1); // Should return default
    ttrue(jsonGetInt(obj, 0, "key", 42) == 42); // Should return default

    // Test invalid operations
    obj = parse("{\"test\": \"value\"}");
    ttrue(obj != 0);

    // Try to get non-existent nested paths
    ttrue(jsonGet(obj, 0, "nonexistent.deeply.nested", 0) == 0);
    ttrue(jsonGetInt(obj, 0, "nonexistent.path", 999) == 999);

    // Try to set on invalid base
    rc = jsonSet(obj, 99999, "key", "value", 0); // Invalid node ID
    ttrue(rc < 0);

    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonParseEdgeCasesTest();
    jsonStrictModeTest();
    jsonMemoryLimitsTest();
    jsonSpecialCharactersTest();
    jsonNullAndUndefinedTest();
    jsonErrorRecoveryTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
*/