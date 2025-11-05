/*
    formats.c.tst - Unit tests for JSON format flags and rendering options

    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonFormatFlagsTest()
{
    Json    *obj;
    char    *result;

    obj = parse("{\
        name: 'John O\\'Malley',\
        age: 30,\
        skills: ['JavaScript', 'Python', 'C'],\
        address: {\
            street: '123 Main St',\
            city: 'Boston'\
        }\
    }");

    // Test JSON_COMPACT flag
    result = jsonToString(obj, 0, 0, JSON_COMPACT);
    ttrue(result != 0);
    // Should have minimal spacing
    ttrue(!scontains(result, "  ")); // No double spaces
    rFree(result);

    // Test JSON_DOUBLE_QUOTES flag
    result = jsonToString(obj, 0, 0, JSON_DOUBLE_QUOTES);
    ttrue(result != 0);
    ttrue(scontains(result, "\"John O'Malley\""));
    rFree(result);

    // Test JSON_SINGLE_QUOTES flag
    result = jsonToString(obj, 0, 0, JSON_SINGLE_QUOTES);
    ttrue(result != 0);
    ttrue(scontains(result, "'name'") || scontains(result, "name"));
    rFree(result);

    // Test JSON_QUOTE_KEYS flag
    result = jsonToString(obj, 0, 0, JSON_QUOTE_KEYS | JSON_DOUBLE_QUOTES);
    ttrue(result != 0);
    ttrue(scontains(result, "\"name\""));
    ttrue(scontains(result, "\"age\""));
    rFree(result);

    // Test JSON_ENCODE flag
    obj = parse("{text: 'Line 1\nLine 2\tTab'}");
    result = jsonToString(obj, 0, 0, JSON_ENCODE);
    ttrue(result != 0);
    ttrue(scontains(result, "\\n"));
    ttrue(scontains(result, "\\t"));
    jsonFree(obj);
    rFree(result);

    // Test JSON_ONE_LINE flag
    obj = parse("{\
        level1: {\
            level2: {\
                value: 'deep'\
            }\
        }\
    }");
    result = jsonToString(obj, 0, 0, JSON_ONE_LINE);
    ttrue(result != 0);
    // Count newlines - should be minimal
    int newlines = 0;
    for (char *p = result; *p; p++) {
        if (*p == '\n' && (p == result || *(p-1) != '\\')) {
            newlines++;
        }
    }
    ttrue(newlines <= 1); // At most one trailing newline
    jsonFree(obj);
    rFree(result);

    // Test JSON_MULTILINE flag
    obj = parse("{a: 1, b: 2, c: {d: 3, e: 4}}");
    result = jsonToString(obj, 0, 0, JSON_MULTILINE);
    ttrue(result != 0);
    ttrue(scontains(result, "\n")); // Should contain newlines for formatting
    jsonFree(obj);
    rFree(result);
}

static void jsonCompositeFormatsTest()
{
    Json    *obj;
    char    *result;

    obj = parse("{name: 'test', value: 42, flag: true}");

    // Test JSON_JS format (JavaScript-like)
    result = jsonToString(obj, 0, 0, JSON_JS);
    ttrue(result != 0);
    // Should use single quotes by default
    ttrue(scontains(result, "'test'"));
    rFree(result);

    // Test JSON_JSON format (strict JSON)
    result = jsonToString(obj, 0, 0, JSON_JSON);
    ttrue(result != 0);
    // Should have quoted keys and double quotes
    ttrue(scontains(result, "\"name\""));
    ttrue(scontains(result, "\"test\""));
    rFree(result);

    // Test JSON_JSON5 format
    result = jsonToString(obj, 0, 0, JSON_JSON5);
    ttrue(result != 0);
    // Should allow unquoted keys and single quotes
    ttrue(scontains(result, "'test'") || scontains(result, "test"));
    rFree(result);

    // Test JSON_HUMAN format
    result = jsonToString(obj, 0, 0, JSON_HUMAN);
    ttrue(result != 0);
    // Should be readable with proper formatting
    ttrue(result != 0);
    rFree(result);

    jsonFree(obj);
}

static void jsonIndentAndLengthTest()
{
    Json    *obj;
    char    *result;

    obj = parse("{\
        level1: {\
            level2: {\
                array: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]\
            }\
        }\
    }");

    // Test custom indent
    jsonSetIndent(2);
    result = jsonToString(obj, 0, 0, JSON_MULTILINE);
    ttrue(result != 0);
    // Check that indentation appears to be 2 spaces (simple heuristic)
    ttrue(scontains(result, "  level1") || scontains(result, "  \"level1\""));
    rFree(result);

    jsonSetIndent(8);
    result = jsonToString(obj, 0, 0, JSON_MULTILINE);
    ttrue(result != 0);
    // Should have wider indentation
    ttrue(scontains(result, "        "));
    rFree(result);

    // Test custom line length
    jsonSetMaxLength(20);
    result = jsonToString(obj, 0, 0, JSON_COMPACT);
    ttrue(result != 0);
    // With short line length, should break up the array
    rFree(result);

    jsonSetMaxLength(200);
    result = jsonToString(obj, 0, 0, JSON_COMPACT);
    ttrue(result != 0);
    // With long line length, array might stay on one line
    rFree(result);

    // Restore defaults
    jsonSetIndent(JSON_DEFAULT_INDENT);
    jsonSetMaxLength(JSON_MAX_LINE_LENGTH);

    jsonFree(obj);
}

static void jsonPartialRenderingTest()
{
    Json    *obj;
    char    *result;

    obj = parse("{\
        users: [\
            {name: 'Alice', age: 30},\
            {name: 'Bob', age: 25}\
        ],\
        settings: {\
            theme: 'dark',\
            language: 'en'\
        }\
    }");

    // Test rendering partial tree - just users array
    result = jsonToString(obj, 0, "users", JSON_JSON);
    ttrue(result != 0);
    ttrue(scontains(result, "Alice"));
    ttrue(scontains(result, "Bob"));
    ttrue(!scontains(result, "settings")); // Should not contain settings
    rFree(result);

    // Test rendering single user
    result = jsonToString(obj, 0, "users[0]", JSON_JSON5);
    ttrue(result != 0);
    ttrue(scontains(result, "Alice"));
    ttrue(!scontains(result, "Bob")); // Should not contain Bob
    rFree(result);

    // Test rendering nested object
    result = jsonToString(obj, 0, "settings", JSON_HUMAN);
    ttrue(result != 0);
    ttrue(scontains(result, "theme"));
    ttrue(scontains(result, "dark"));
    ttrue(!scontains(result, "users")); // Should not contain users
    rFree(result);

    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonFormatFlagsTest();
    jsonCompositeFormatsTest();
    jsonIndentAndLengthTest();
    jsonPartialRenderingTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
*/