
/*
    fuzz.c.tst - Unit tests for JSON fuzzing

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonFuzz()
{
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
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
