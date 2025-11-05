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

int main(void)
{
    rInit(0, 0);
    jsonStrictBoundaryTest();
    jsonStrictValidTest();
    jsonStrictOutputTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */