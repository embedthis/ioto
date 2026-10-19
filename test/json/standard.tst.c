/*
    standard.c.tst - Unit tests for strict JSON standard compliance

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonStrictBoundaryTest()
{
    Json    *obj;
    char    *error;

    // Empty and whitespace - should fail in strict mode
    obj = jsonParseString("", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    obj = jsonParseString("   ", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    obj = jsonParseString("\n\t  \n", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    // JSON5/6 features that should fail in strict mode

    // Unquoted object keys
    obj = jsonParseString("{key: \"value\"}", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    // Single quotes
    obj = jsonParseString("{'key': 'value'}", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    obj = jsonParseString("{\"key\": 'single quotes'}", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    // Trailing commas
    obj = jsonParseString("[1,2,3,]", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    obj = jsonParseString("{\"a\": 1, \"b\": 2,}", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    // Multiple commas
    obj = jsonParseString("[1,,2]", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    obj = jsonParseString("{\"a\": 1,, \"b\": 2}", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    // Comments
    obj = jsonParseString("// comment\n{\"key\": \"value\"}", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    obj = jsonParseString("/* comment */ {\"key\": \"value\"}", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    // Unquoted string values
    obj = jsonParseString("{\"key\": value}", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    // Invalid keywords as strings
    obj = jsonParseString("True", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    obj = jsonParseString("False", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    obj = jsonParseString("NULL", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    // Undefined primitive (JSON6 feature)
    obj = jsonParseString("{\"key\": undefined}", &error, JSON_STRICT_PARSE);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);
}

static void jsonStrictValidTest()
{
    Json    *obj;
    char    *error;

    // Valid strict JSON should succeed
    obj = jsonParseString("{\"key\": \"value\"}", &error, JSON_STRICT_PARSE);
    ttrue(obj != 0);
    ttrue(error == 0);
    checkValue(obj, "key", "value");
    jsonFree(obj);

    obj = jsonParseString("[1, 2, 3]", &error, JSON_STRICT_PARSE);
    ttrue(obj != 0);
    ttrue(error == 0);
    checkValue(obj, "[0]", "1");
    checkValue(obj, "[2]", "3");
    jsonFree(obj);

    obj = jsonParseString("{\"string\": \"hello\", \"number\": 42, \"boolean\": true, \"null\": null}", &error, JSON_STRICT_PARSE);
    ttrue(obj != 0);
    ttrue(error == 0);
    checkValue(obj, "string", "hello");
    checkValue(obj, "number", "42");
    checkValue(obj, "boolean", "true");
    jsonFree(obj);

    // Test nested structures
    obj = jsonParseString("{\"object\": {\"nested\": true}, \"array\": [1, 2, {\"inner\": \"value\"}]}", &error, JSON_STRICT_PARSE);
    ttrue(obj != 0);
    ttrue(error == 0);
    checkValue(obj, "object.nested", "true");
    checkValue(obj, "array[2].inner", "value");
    jsonFree(obj);
}

static void jsonStrictOutputTest()
{
    Json    *obj;
    char    *result;
    char    *error;

    // Parse JSON5 in relaxed mode, then output in strict JSON format
    obj = jsonParseString("{key: 'value', array: [1,2,3,]}", &error, 0);
    ttrue(obj != 0);

    // Output should be strict JSON, not JSON5/6
    result = jsonToString(obj, 0, 0, JSON_JSON);
    ttrue(result != 0);

    // Result should have quoted keys and double quotes for strings
    ttrue(scontains(result, "\"key\""));
    ttrue(scontains(result, "\"value\""));
    // Should not contain trailing comma
    ttrue(!scontains(result, ",]"));
    rFree(result);
    jsonFree(obj);

    // Test jsonPrint* functions produce strict JSON
    obj = jsonParseString("{unquoted: 'single', /* comment */ \"normal\": true}", &error, 0);
    ttrue(obj != 0);

    result = jsonToString(obj, 0, 0, JSON_JSON);
    ttrue(result != 0);

    // Should be strict JSON output
    ttrue(scontains(result, "\"unquoted\""));
    ttrue(scontains(result, "\"single\""));
    ttrue(scontains(result, "\"normal\""));
    // Should not contain comments
    ttrue(!scontains(result, "/*"));
    ttrue(!scontains(result, "comment"));

    rFree(result);
    jsonFree(obj);
}

/*
    Every document valid per RFC 7159 must parse under JSON_STRICT_PARSE, and every
    malformed one must not. Empty containers and arrays of non-primitive elements were
    both rejected before issue 13.
 */
static void jsonStrictShapeTest()
{
    Json  *obj;
    char  *error;
    cchar *accept[] = {
        "{}", "[]", "{\"a\":{}}", "{\"a\":[]}", "[[]]", "[{}]",
        "[\"x\"]", "[\"x\",\"y\"]", "[1,\"x\"]", "[\"x\",1]",
        "[{\"a\":\"b\"}]", "[[1],[2]]", "{\"a\":[{\"b\":[]}]}",
        "{\"a\":1}", "{\"a\":\"x\"}", "{\"a\":true}", "{\"a\":null}",
        "\"x\"", "1", "[1,2,3]", 0
    };
    cchar *reject[] = {
        "[1,]", "{\"a\":}", "{\"a\":1,}", "[,]", "{,}", "[1,,2]",
        "[\"x\" \"y\"]", "[1 2]", "[\"a\":1]", "{\"a\" \"b\"}",
        "[/ab/]", "{a:1}", "{'a':1}", "{\"a\":undefined}", 0
    };
    int   i;

    for (i = 0; accept[i]; i++) {
        error = 0;
        obj = jsonParseString(accept[i], &error, JSON_STRICT_PARSE);
        if (obj == 0) {
            tfail("Strict parse rejected valid JSON: %s (%s)", accept[i], error ? error : "");
        } else {
            ttrue(obj != 0);
            jsonFree(obj);
        }
        rFree(error);
    }
    for (i = 0; reject[i]; i++) {
        error = 0;
        obj = jsonParseString(reject[i], &error, JSON_STRICT_PARSE);
        if (obj != 0) {
            tfail("Strict parse accepted invalid JSON: %s", reject[i]);
            jsonFree(obj);
        } else {
            ttrue(obj == 0);
        }
        rFree(error);
    }
}

int main(void)
{
    rInit(0, 0);
    jsonStrictBoundaryTest();
    jsonStrictValidTest();
    jsonStrictOutputTest();
    jsonStrictShapeTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */