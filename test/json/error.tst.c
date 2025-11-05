/*
    error.c.tst - Unit tests for JSON error handling

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonErrorTest()
{
    // Legacy tests - these should fail
    ttrue(parseFail("}"));
    ttrue(parseFail("]"));
}

static void jsonBoundaryErrorTest()
{
    // Empty and whitespace - these should succeed per parser note
    ttrue(parseSuccess(""));
    ttrue(parseSuccess("   "));
    ttrue(parseSuccess("\n\t  \n"));

    // Unmatched brackets and braces
    ttrue(parseFail("{"));
    ttrue(parseFail("}"));
    ttrue(parseFail("["));
    ttrue(parseFail("]"));
    ttrue(parseFail("{]"));
    ttrue(parseFail("[}"));
    ttrue(parseFail("{{}"));
    ttrue(parseFail("{[}]"));

    // Invalid object syntax
    ttrue(parseSuccess("{,}")); // Multiple commas tolerated
    ttrue(parseFail("{:}"));
    ttrue(parseFail("{\"key\"}"));
    ttrue(parseFail("{\"key\":}"));
    ttrue(parseSuccess("{\"key\": ,}")); // Trailing comma tolerated
    ttrue(parseSuccess("{key: value, ,}")); // Multiple commas tolerated
    ttrue(parseFail("{\"a\": 1 \"b\": 2}"));
    ttrue(parseFail("{\"a\": 1, \"b\":}"));

    // Invalid array syntax
    ttrue(parseSuccess("[,]")); // Multiple commas tolerated
    ttrue(parseSuccess("[1,,2]")); // Multiple commas tolerated
    ttrue(parseFail("[1 2]"));
    ttrue(parseSuccess("[1,]")); // Trailing comma is allowed in JSON5

    // Invalid strings
    ttrue(parseFail("\"unterminated string"));
    ttrue(parseFail("\"invalid \\x escape\""));
    ttrue(parseFail("\"\\u12G4\""));
    ttrue(parseFail("\"\\u123\""));
    ttrue(parseSuccess("'single quotes'")); // Single quotes are valid in JSON5
    ttrue(parseSuccess("\"line\nbreak\"")); // Multiline strings are valid in JSON5

#if FUTURE
    // The parser does not validate numbers
    ttrue(parseFail("01"));
    ttrue(parseFail("1."));
    ttrue(parseFail(".5"));
    ttrue(parseFail("1.2.3"));
    ttrue(parseFail("1e"));
    ttrue(parseFail("1e+"));
    ttrue(parseFail("1e1.2"));
    ttrue(parseFail("++1"));
    ttrue(parseFail("--1"));
    ttrue(parseFail("1-"));
#endif

    // Invalid keywords at the top level are interpreted as unquoted strings
    ttrue(parseSuccess("True"));
    ttrue(parseSuccess("False"));
    ttrue(parseSuccess("NULL"));
    ttrue(parseSuccess("nil"));
    ttrue(parseSuccess("none"));
    ttrue(parseSuccess("truee"));
    ttrue(parseSuccess("falsee"));
    ttrue(parseSuccess("nul"));

    // Invalid keywords as property values are treated as unquoted strings
    ttrue(parseSuccess("{\"key\": True}"));
    ttrue(parseSuccess("{\"key\": False}"));
    ttrue(parseSuccess("{\"key\": NULL}"));
    ttrue(parseSuccess("{\"key\": nil}"));
    ttrue(parseSuccess("{\"key\": none}"));
    ttrue(parseSuccess("{\"key\": truee}"));
    ttrue(parseSuccess("{\"key\": falsee}"));
    ttrue(parseSuccess("{\"key\": nul}"));
    ttrue(parseSuccess("[True, False, NULL]"));
    ttrue(parseSuccess("[nil, none, truee]"));

    // Control characters
    ttrue(parseFail("{\x01}"));
    ttrue(parseFail("{\x1F}"));
    ttrue(parseFail("\"test\x00\""));

    // Deeply nested structures (potential stack overflow)
    char *deep = rAlloc(10000);
    strcpy(deep, "[");
    for (int i = 1; i < 1000; i++) {
        strcat(deep, "[");
    }
    strcat(deep, "1");
    for (int i = 0; i < 999; i++) {
        strcat(deep, "]");
    }
    // Missing one closing bracket
    ttrue(parseFail(deep));
    rFree(deep);

    // Unicode issues
    ttrue(parseFail("\xFF\xFE"));
    ttrue(parseFail("\xEF\xBB"));

    // Mixed delimiters
    ttrue(parseFail("[1,2,3}"));
    ttrue(parseFail("{\"a\":1,\"b\":2]"));

    // Invalid escape sequences
    ttrue(parseFail("\"\\z\""));
    ttrue(parseFail("\"\\x\""));

    // Trailing content
    ttrue(parseFail("{}extra"));
    ttrue(parseFail("[]more"));
    ttrue(parseFail("true false"));
}


int main(void)
{
    rInit(0, 0);
    jsonErrorTest();
    jsonBoundaryErrorTest();
    rTerm();
    return 0;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
