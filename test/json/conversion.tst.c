/*
    conversion.c.tst - Unit tests for JSON conversion functions

    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonConvertTest()
{
    char    *result;
    char    buf[256];
    cchar   *str;

    // Test jsonConvert with simple JSON string
    result = jsonConvert("\"Hello World\"");
    tmatch(result, "\"Hello World\"");
    rFree(result);

    // Test jsonConvert with printf formatting of JSON
    result = jsonConvert("\"%s has %d points\"", "Alice", 100);
    tmatch(result, "\"Alice has 100 points\"");
    rFree(result);

    // Test jsonConvert with JSON object
    result = jsonConvert("{\"name\": \"%s\", \"age\": %d}", "Bob", 30);
    ttrue(result != NULL);
    ttrue(scontains(result, "\"name\""));
    ttrue(scontains(result, "\"Bob\""));
    ttrue(scontains(result, "\"age\""));
    ttrue(scontains(result, "30"));
    rFree(result);

    // Test jsonConvert with JSON array
    result = jsonConvert("[%d, %d, %d]", 1, 2, 3);
    tmatch(result, "[1,2,3]");
    rFree(result);

    // Test jsonConvertBuf with JSON string
    str = jsonConvertBuf(buf, sizeof(buf), "\"Temperature: %d°C\"", 25);
    ttrue(str == buf);
    tmatch(buf, "\"Temperature: 25°C\"");

    // Test JFMT macro with JSON string
    str = JFMT(buf, "\"Count: %d\"", 42);
    tmatch(buf, "\"Count: 42\"");

    // Test JSON macro with valid JSON string
    str = JSON(buf, "\"Simple text\"");
    tmatch(buf, "\"Simple text\"");

    // Test with empty JSON string
    result = jsonConvert("\"\"");
    tmatch(result, "\"\"");
    rFree(result);

    // Test with NULL (should handle gracefully)
    result = jsonConvert(NULL);
    ttrue(result == NULL);

    // Test with invalid JSON (should return NULL)
    result = jsonConvert("invalid json");
    ttrue(result == NULL);
}

static void jsonParseFmtTest()
{
    Json    *obj;
    char    *name = "Bob";
    int     age = 30;

    // Test jsonParseFmt with simple object
    obj = jsonParseFmt("{name: '%s', age: %d}", name, age);
    ttrue(obj != 0);
    tmatch(jsonGet(obj, 0, "name", 0), "Bob");
    tmatch(jsonGet(obj, 0, "age", 0), "30");
    jsonFree(obj);

    // Test with array
    obj = jsonParseFmt("[%d, %d, %d]", 1, 2, 3);
    ttrue(obj != 0);
    tmatch(jsonGet(obj, 0, "[0]", 0), "1");
    tmatch(jsonGet(obj, 0, "[1]", 0), "2");
    tmatch(jsonGet(obj, 0, "[2]", 0), "3");
    tmatch(jsonGet(obj, 0, "[2]", 0), "3");
    jsonFree(obj);

    // Test with nested structure
    obj = jsonParseFmt("{\
        user: {\
            name: '%s',\
            age: %d,\
            active: %s\
        }\
    }", "Charlie", 25, "true");
    ttrue(obj != 0);
    tmatch(jsonGet(obj, 0, "user.name", 0), "Charlie");
    tmatch(jsonGet(obj, 0, "user.age", 0), "25");
    tmatch(jsonGet(obj, 0, "user.active", 0), "true");
    tmatch(jsonGet(obj, 0, "user.active", 0), "true");
    jsonFree(obj);

    // Test with boolean and null values
    obj = jsonParseFmt("{\
        enabled: %s,\
        disabled: %s,\
        value: %s\
    }", "true", "false", "null");
    ttrue(obj != 0);
    tmatch(jsonGet(obj, 0, "enabled", 0), "true");
    tmatch(jsonGet(obj, 0, "disabled", 0), "false");
    ttrue(jsonGet(obj, 0, "value", 0) == NULL);
    jsonFree(obj);
}

static void jsonStringTest()
{
    Json    *obj;
    cchar   *result;

    obj = parse("{\
        name: 'Test Object',\
        values: [1, 2, 3],\
        nested: {\
            flag: true\
        }\
    }");

    // Test jsonString with default flags
    result = jsonString(obj, 0);
    ttrue(result != 0);
    ttrue(scontains(result, "name"));
    ttrue(scontains(result, "Test Object"));

    // Test jsonString with JSON format
    result = jsonString(obj, JSON_JSON);
    ttrue(result != 0);
    ttrue(scontains(result, "\"name\""));
    ttrue(scontains(result, "\"Test Object\""));

    // Test jsonString with compact format
    result = jsonString(obj, JSON_ONE_LINE);
    ttrue(result != 0);
    // Should be on one line (no newlines except in string values)
    char *newline = strchr(result, '\n');
    ttrue(newline == 0 || *(newline-1) == '\\'); // Only escaped newlines

    // Test jsonString with multiline format
    result = jsonString(obj, JSON_MULTILINE);
    ttrue(result != 0);
    ttrue(scontains(result, "\n")); // Should contain newlines

    jsonFree(obj);
}

static void jsonPrintTest()
{
    Json    *obj;

    obj = parse("{test: 'print functionality'}");
    // Omit this test as it generates unwanted output.
    // jsonPrint(obj);
    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonConvertTest();
    jsonParseFmtTest();
    jsonStringTest();
    jsonPrintTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
*/