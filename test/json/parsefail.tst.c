/*
    parsefail.c.tst - Unit tests for JSON parse failure paths

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonRegExpFailTest()
{
    Json    *obj;
    char    *error;

    //  A complete regular expression parses
    obj = jsonParseString("{re: /ab+c/}", &error, 0);
    ttrue(obj != 0);
    jsonFree(obj);

    //  An unterminated regular expression is an error
    obj = jsonParseString("{re: /abc}", &error, 0);
    ttrue(obj == 0);
    ttrue(scontains(error, "Incomplete regular expression") != 0);
    rFree(error);

    ttrue(parseFail("/unterminated"));

    //  An escaped slash does not terminate the expression
    obj = parse("{re: /a\\/b/}");
    ttrue(obj != 0);
    checkType(obj, "re", JSON_REGEXP);
    jsonFree(obj);
}

static void jsonUnicodeFailTest()
{
    Json    *obj;
    char    *error;

    //  A unicode escape truncated by the end of input
    obj = jsonParseString("\"\\u12", &error, 0);
    ttrue(obj == 0);
    ttrue(scontains(error, "Invalid unicode characters") != 0);
    rFree(error);

    //  Non hex characters in a unicode escape
    obj = jsonParseString("\"\\u12g4\"", &error, 0);
    ttrue(obj == 0);
    ttrue(scontains(error, "Unexpected hex characters") != 0);
    rFree(error);
}

static void jsonCommentTest()
{
    Json    *obj;
    char    *error;

    //  A multiline comment spanning several lines is skipped and the
    //  line number is tracked for later errors
    obj = parse("{\n/* one\n   two\n   three */\n a: 1\n}");
    ttrue(obj != 0);
    checkValue(obj, "a", "1");
    jsonFree(obj);

    //  An unterminated multiline comment names the line it started on
    obj = jsonParseString("{\n\n/* never closed\n a: 1\n}", &error, 0);
    ttrue(obj == 0);
    ttrue(scontains(error, "Cannot find end of comment") != 0);
    rFree(error);
}

static void jsonBraceFailTest()
{
    Json    *obj;
    char    *error;

    //  A container may not follow a value without a comma
    obj = jsonParseString("[1 [2]]", &error, 0);
    ttrue(obj == 0);
    ttrue(scontains(error, "Invalid brace/bracket") != 0);
    rFree(error);

    ttrue(parseFail("[[1] [2]]"));
    ttrue(parseFail("{a: 1 {}}"));
}

static void jsonNumberSleuthTest()
{
    Json    *obj;

    //  Values that look numeric but are not become strings
    obj = parse("{a: 1.2.3, b: 1e5, c: -12, d: 1-2, e: hello}");
    ttrue(obj != 0);
    checkType(obj, "a", JSON_STRING);
    checkType(obj, "b", JSON_PRIMITIVE);
    checkType(obj, "c", JSON_PRIMITIVE);
    checkType(obj, "d", JSON_STRING);
    checkType(obj, "e", JSON_STRING);
    checkValue(obj, "a", "1.2.3");
    jsonFree(obj);

    //  A non-numeric character disqualifies a numeric looking token
    obj = parse("{a: 12x }");
    ttrue(obj != 0);
    checkType(obj, "a", JSON_STRING);
    jsonFree(obj);

    //  A leading plus is not accepted
    ttrue(parseFail("{a: +5}"));

    //  jsonParse returns null for text it cannot parse
    ttrue(jsonParse("{a:", 0) == 0);
    ttrue(jsonParse("}", 0) == 0);

    //  A dereference in a primitive is an error
    ttrue(parseFail("{a: .5}"));
}

static void jsonMaxNodesTest()
{
    Json        *obj;
    RBuf        *buf;
    char        *error;
    int         i;

    //  Exceeding the node limit is reported rather than allocated
    buf = rAllocBuf(0);
    rPutCharToBuf(buf, '[');
    for (i = 0; i < 100001; i++) {
        rPutStringToBuf(buf, "1,");
    }
    rPutCharToBuf(buf, ']');

    obj = jsonParseString(rGetBufStart(buf), &error, 0);
    ttrue(obj == 0);
    ttrue(scontains(error, "Too many elements") != 0);
    rFree(error);
    rFreeBuf(buf);
}

int main(void)
{
    rInit(0, 0);
    jsonRegExpFailTest();
    jsonUnicodeFailTest();
    jsonCommentTest();
    jsonBraceFailTest();
    jsonNumberSleuthTest();
    jsonMaxNodesTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
