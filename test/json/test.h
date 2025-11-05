/*
    test.h - Unit tests helpers

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"
#include    "json.h"

/************************************ Code ************************************/

PUBLIC Json *parse(cchar *text)
{
    Json    *obj;
    char    *error;

    if ((obj = jsonParseString(text, &error, 0)) == 0) {
        printf("Cannot parse json: %s\nJSON: \n%s", error, text);
        rFree(error);
        return NULL;
    }
    return obj;
}

/*
    Parse should succeed
 */
PUBLIC bool parseSuccess(cchar *text)
{
    Json    *obj;
    char    *error;

    if ((obj = jsonParseString(text, &error, 0)) == 0) {
        rFree(error);
        return 0;
    }
    jsonFree(obj);
    return 1;
}

/* 
    Parse should fail
 */
PUBLIC bool parseFail(cchar *text)
{
    Json    *obj;
    char    *error;

    if ((obj = jsonParseString(text, &error, 0)) == 0) {
        fprintf(stderr, "Failed to parse: %s\n%s", text, error);
        rFree(error);
        return 1;
    }
    jsonFree(obj);
    return 0;
}

// LEGACY
PUBLIC bool quiet(cchar *text)
{
    Json    *obj;
    char    *error;

    if ((obj = jsonParseString(text, &error, 0)) == 0) {
        rFree(error);
        return 0;
    } else {
        jsonFree(obj);
    }
    return 1;
}

#define checkJson(json, value, flags) checkJsonInner(json, value, flags, __LINE__)


PUBLIC void checkJsonInner(Json *json, cchar *value, int flags, int line)
{
    char    *result;

    result = jsonToString(json, 0, 0, flags);
    if (smatch(result, value)) {
        tmatch(result, value);
    } else {
        tfail("Expected: %s, Received: %s, at line %d", value, result, line);
    }
    rFree(result);
}

#define checkValue(json, key, value) checkValueInner(json, key, value, __LINE__)

PUBLIC void checkValueInner(Json *json, cchar *key, cchar *value, int line)
{
    cchar   *result;

    result = jsonGet(json, 0, key, 0);
    if (smatch(result, value)) {
        tmatch(result, value);
    } else {
        tfail("Expected: %s, Received: %s, at line %d", value, result, line);
    }
}


PUBLIC void checkType(Json *json, cchar *key, int type)
{
    int     atype;

    atype = jsonGetType(json, 0, key);
    ttrue(atype == type);
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
