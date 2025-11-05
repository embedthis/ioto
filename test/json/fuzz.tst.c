
/*
    fuzz.c.tst - Unit tests for JSON fuzzing

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonFuzz()
{
    Json *obj;
    char *error;
    if ((obj = jsonParseString(",", &error, 0)) == 0) {
        fprintf(stderr, "Failed to parse: %s\n", error);
        rFree(error);
        return;
    }
    jsonFree(obj);
    /* All should fail */
    ttrue(parseFail("unquoted string"));
    ttrue(parseFail(":"));
    ttrue(parseFail(":::::"));
    ttrue(parseFail(","));
    ttrue(parseFail(",,,,,"));
    ttrue(parseFail("'"));
    ttrue(parseFail("\\a"));
    ttrue(parseFail("\\{"));
    ttrue(parseFail("{"));
    ttrue(parseFail("["));
    ttrue(parseFail("}"));
    ttrue(parseFail("]"));
    ttrue(parseFail("[[[[[[[[[["));
    ttrue(parseFail("{{{{{{{{{{"));
    ttrue(parseFail("@"));
    ttrue(parseFail("..."));
    ttrue(parseFail("\\"));
    ttrue(parseFail("        \\"));
    ttrue(parseFail("\x01"));

    /* Should pass */
    ttrue(parseSuccess("one-word"));
    ttrue(parseSuccess("'multiple quoted words'"));
    ttrue(parseSuccess("\"multiple quoted words\""));
    ttrue(parseSuccess(""));
    ttrue(parseSuccess(0));
    ttrue(parseSuccess("1234"));
    ttrue(parseSuccess("true"));
    ttrue(parseSuccess("false"));
    ttrue(parseSuccess("null"));
    ttrue(parseSuccess("        "));
}

int main(void)
{
    rInit(0, 0);
    jsonFuzz();
    rTerm();
    return 0;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
