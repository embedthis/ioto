/*
    types.c.tst - Unit tests for JSON types

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/
static void jsonTypes()
{
    Json    *obj;
    char    *s;

    obj = parse("{ value: 'hello' }");
    checkJson(obj, "{value:'hello'}", 0);
    jsonFree(obj);

    obj = parse("{ value: 'hello' }");
    checkJson(obj, "{value:'hello'}", 0);
    jsonFree(obj);

    obj = parse("{ value: true }");
    checkJson(obj, "{value:true}", 0);
    jsonFree(obj);

    obj = parse("{ value: false }");
    checkJson(obj, "{value:false}", 0);
    jsonFree(obj);

    obj = parse("{ value: 42 }");
    checkJson(obj, "{value:42}", 0);
    jsonFree(obj);

    obj = parse("{ value: 3.14159 }");
    checkJson(obj, "{value:3.14159}", 0);
    jsonFree(obj);

    obj = parse("{ value: null }");
    checkJson(obj, "{value:null}", 0);
    jsonFree(obj);

    obj = parse("{ value: undefined }");
    checkJson(obj, "{value:undefined}", 0);
    jsonFree(obj);

    obj = parse("{ value: /abc/ }");
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{value:/abc/}");
    jsonFree(obj);
    rFree(s);

    /*
        Special values as strings
    */
    obj = parse("{ value: 'undefined' }");
    checkJson(obj, "{value:'undefined'}", 0);
    jsonFree(obj);

    obj = parse("{ value: 'null' }");
    checkJson(obj, "{value:'null'}", 0);
    jsonFree(obj);

    obj = parse("{ value: '1.2' }");
    checkJson(obj, "{value:'1.2'}", 0);
    jsonFree(obj);

    obj = parse("{ value: 'true' }");
    checkJson(obj, "{value:'true'}", 0);
    jsonFree(obj);

    obj = parse("{ value: 'false' }");
    checkJson(obj, "{value:'false'}", 0);
    jsonFree(obj);
}


/*
    The value type must be sleuthed from the whole token, regardless of which
    delimiter terminated it.
 */
static void jsonSleuthTypes()
{
    Json    *obj;
    char    *s;

    //  A non-numeric trailing character makes the value a string
    obj = parse("{a: 12x}");
    checkType(obj, "a", JSON_STRING);
    checkValue(obj, "a", "12x");
    s = jsonToString(obj, 0, 0, JSON_JSON);
    tmatch(s, "{\"a\":\"12x\"}");
    rFree(s);
    jsonFree(obj);

    //  The terminating delimiter must not change the result
    obj = parse("{a: 12x }");
    checkType(obj, "a", JSON_STRING);
    jsonFree(obj);

    obj = parse("{a: 12x, b: 12x}");
    checkType(obj, "a", JSON_STRING);
    checkType(obj, "b", JSON_STRING);
    jsonFree(obj);

    obj = parse("[12x]");
    checkType(obj, "[0]", JSON_STRING);
    jsonFree(obj);

    //  A keyword prefix is not a keyword
    obj = parse("{a: nul, b: tru, c: fals, d: undefine}");
    checkType(obj, "a", JSON_STRING);
    checkType(obj, "b", JSON_STRING);
    checkType(obj, "c", JSON_STRING);
    checkType(obj, "d", JSON_STRING);
    jsonFree(obj);

    obj = parse("{a: nul}");
    checkType(obj, "a", JSON_STRING);
    jsonFree(obj);

    //  Nor is a keyword with a suffix
    obj = parse("{a: nullish, b: truely}");
    checkType(obj, "a", JSON_STRING);
    checkType(obj, "b", JSON_STRING);
    jsonFree(obj);

    //  The exact keywords remain primitives with any terminator
    obj = parse("{a: true, b: false, c: null, d: undefined}");
    checkType(obj, "a", JSON_PRIMITIVE);
    checkType(obj, "b", JSON_PRIMITIVE);
    checkType(obj, "c", JSON_PRIMITIVE);
    checkType(obj, "d", JSON_PRIMITIVE);
    jsonFree(obj);

    obj = parse("{a: true}");
    checkType(obj, "a", JSON_PRIMITIVE);
    jsonFree(obj);

    obj = parse("[true,false,null]");
    checkType(obj, "[0]", JSON_PRIMITIVE);
    checkType(obj, "[1]", JSON_PRIMITIVE);
    checkType(obj, "[2]", JSON_PRIMITIVE);
    jsonFree(obj);

    //  Numbers remain primitives with any terminator
    obj = parse("{a: 42, b: -1.5, c: 1e5}");
    checkType(obj, "a", JSON_PRIMITIVE);
    checkType(obj, "b", JSON_PRIMITIVE);
    checkType(obj, "c", JSON_PRIMITIVE);
    jsonFree(obj);

    obj = parse("{a: 42}");
    checkType(obj, "a", JSON_PRIMITIVE);
    s = jsonToString(obj, 0, 0, JSON_JSON);
    tmatch(s, "{\"a\":42}");
    rFree(s);
    jsonFree(obj);
}

/*
    "undefined" is a JSON5 primitive with no JSON representation. Issue 14.
 */
static void jsonUndefinedDialectTest()
{
    Json *obj;
    char *s;

    obj = parse("{a: undefined, b: 1}");

    //  JSON output must be valid JSON
    s = jsonToString(obj, 0, 0, JSON_JSON);
    tmatch(s, "{\"a\":null,\"b\":1}");
    rFree(s);

    //  JSON5 and JS keep it, where it is legal
    s = jsonToString(obj, 0, 0, JSON_JSON5);
    tmatch(s, "{a:undefined,b:1}");
    rFree(s);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{a:undefined,b:1}");
    rFree(s);
    jsonFree(obj);

    //  A bare undefined document, and the null it must not be confused with
    obj = parse("undefined");
    s = jsonToString(obj, 0, 0, JSON_JSON);
    tmatch(s, "null");
    rFree(s);
    jsonFree(obj);

    obj = parse("{a: null}");
    s = jsonToString(obj, 0, 0, JSON_JSON);
    tmatch(s, "{\"a\":null}");
    rFree(s);
    jsonFree(obj);

    //  A string that merely reads "undefined" is still a string
    obj = parse("{a: 'undefined'}");
    s = jsonToString(obj, 0, 0, JSON_JSON);
    tmatch(s, "{\"a\":\"undefined\"}");
    rFree(s);
    jsonFree(obj);
}

/*
    A token is a number only if it matches the RFC 7159 number grammar. Issue 10.
 */
static void jsonNumberGrammarTest()
{
    Json  *obj;
    char  *s, *text;
    cchar *numbers[] = {
        "0", "-0", "42", "-1", "1.5", "0.5", "1e5", "1e-3", "1e+5", "1E+10",
        "1.2E-3", "-5.2e+2", "123456789", "-0.0001", 0
    };
    cchar *strings[] = {
        "01", "007", "00", "1.", "1e", "1e+", "1-2", "12x", "1.2.3",
        "0x10", "Infinity", "NaN", "-Infinity", 0
    };
    int   i;

    for (i = 0; numbers[i]; i++) {
        text = sfmt("{a: %s}", numbers[i]);
        obj = parse(text);
        if (obj == 0) {
            tfail("Number failed to parse: %s", numbers[i]);
        } else {
            if (jsonGetType(obj, 0, "a") != JSON_PRIMITIVE) {
                tfail("Not typed as a number: %s", numbers[i]);
            } else {
                ttrue(jsonGetType(obj, 0, "a") == JSON_PRIMITIVE);
            }
            jsonFree(obj);
        }
        rFree(text);
    }
    for (i = 0; strings[i]; i++) {
        text = sfmt("{a: %s}", strings[i]);
        obj = parse(text);
        if (obj == 0) {
            tfail("Token failed to parse: %s", strings[i]);
        } else {
            if (jsonGetType(obj, 0, "a") != JSON_STRING) {
                tfail("Should not be typed as a number: %s", strings[i]);
            } else {
                ttrue(jsonGetType(obj, 0, "a") == JSON_STRING);
            }
            //  And the JSON rendering must survive a strict reparse
            s = jsonToString(obj, 0, 0, JSON_JSON);
            ttrue(s != 0);
            if (s) {
                Json *again = jsonParse(s, JSON_STRICT_PARSE);
                if (again == 0) {
                    tfail("Strict reparse rejected our own JSON output: %s", s);
                } else {
                    ttrue(again != 0);
                    jsonFree(again);
                }
                rFree(s);
            }
            jsonFree(obj);
        }
        rFree(text);
    }
}

int main(void)
{
    rInit(0, 0);
    jsonTypes();
    jsonSleuthTypes();
    jsonUndefinedDialectTest();
    jsonNumberGrammarTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
