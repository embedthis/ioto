/*
    sig-types.tst.c - Signature validation of primitive types, field dropping and defaults

    Complements sig.tst.c. These drive the signature engine over the /test/sig route, which
    echoes the validated request back as the validated response, so a single request exercises
    both the request and response validation paths.

    The sig action fakes a 'user' identity but never authenticates, so webCan() denies every
    non-public role. That is what makes the role-keyed "drop" cases drop.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

/*
    POST a JSON body to a signature method and check the status. Returns the parsed response
    body, or NULL. The caller frees it.
 */
static Json *invoke(Url *up, cchar *method, int status, cchar *body)
{
    Json *response;
    char url[160];

    urlClose(up);
    response = urlJson(up, "POST", SFMT(url, "%s/test/sig/controller/%s", HTTP, method), body,
                       body ? (size_t) -1 : 0, NULL);
    if (urlGetStatus(up) != status) {
        rError("web", "Method %s returned %d (expected %d): %s", method, urlGetStatus(up), status,
               urlGetResponse(up));
    }
    teqi(urlGetStatus(up), status);
    return response;
}

/*
    Check a response field against its expected unquoted value. strim() may return a pointer into
    the middle of the allocation, so the original pointer is the one that is freed.
 */
static void checkField(Json *json, cchar *key, cchar *expected)
{
    char *result;

    if (!json) {
        tfail("No response for key %s", key);
        return;
    }
    if ((result = jsonToString(json, 0, key, 0)) == 0) {
        tfail("Missing response key %s", key);
        return;
    }
    tmatch(strim(result, "'\"", R_TRIM_BOTH), expected);
    rFree(result);
}

/*
    Check that a field was dropped from the response.
 */
static void checkFieldAbsent(Json *json, cchar *key)
{
    char *result;

    result = json ? jsonToString(json, 0, key, 0) : 0;
    tnull(result);
    rFree(result);
}

/*
    Boolean, date and number field types are checked by validatePrimitive.
 */
static void testFieldTypes(Url *up)
{
    Json *response;
    response = invoke(up, "test_9", 200,
                      "{flag: true, when: '2025-01-01T00:00:00Z', count: 42, label: 'x'}");
    checkField(response, "flag", "true");
    checkField(response, "count", "42");
    checkField(response, "label", "x");
    jsonFree(response);

    //  'false' is equally valid, and the boolean test is case insensitive
    response = invoke(up, "test_9", 200, "{flag: false, when: '2024-02-29T23:59:59Z', count: -1, label: ''}");
    checkField(response, "flag", "false");
    jsonFree(response);

    //  A non-boolean in a boolean field is rejected
    jsonFree(invoke(up, "test_9", 400, "{flag: 'maybe', when: '2025-01-01T00:00:00Z', count: 1, label: 'x'}"));
    jsonFree(invoke(up, "test_9", 400, "{flag: 2, when: '2025-01-01T00:00:00Z', count: 1, label: 'x'}"));

    //  A malformed date is rejected
    jsonFree(invoke(up, "test_9", 400, "{flag: true, when: 'yesterday', count: 1, label: 'x'}"));
    jsonFree(invoke(up, "test_9", 400, "{flag: true, when: '2025-13-45T99:99:99Z', count: 1, label: 'x'}"));

    //  A non-numeric value in a number field is rejected
    jsonFree(invoke(up, "test_9", 400, "{flag: true, when: '2025-01-01T00:00:00Z', count: 'lots', label: 'x'}"));
}

/*
    A type name the validator does not recognise must fail closed rather than pass the value.
 */
static void testUnknownType(Url *up)
{
    jsonFree(invoke(up, "test_10", 400, "{thing: 'anything'}"));
}

/*
    'drop' given as a role name removes the field unless the requester holds that role.
 */
static void testDropByRole(Url *up)
{
    Json *response;
    response = invoke(up, "test_11", 200, "{name: 'visible', secret: 'hidden'}");
    checkField(response, "name", "visible");
    //  The requester does not hold 'admin', so the field is dropped from the response
    checkFieldAbsent(response, "secret");
    jsonFree(response);
}

/*
    'drop' given as an object is keyed by the validation tag, so drop.response applies here.
 */
static void testDropByTag(Url *up)
{
    Json *response;
    response = invoke(up, "test_12", 200, "{name: 'visible', secret: 'hidden'}");
    checkField(response, "name", "visible");
    checkFieldAbsent(response, "secret");
    jsonFree(response);
}

/*
    A required response field that is absent is supplied from its default.
 */
static void testResponseDefault(Url *up)
{
    Json *response;
    response = invoke(up, "test_13", 200, "{name: 'Test User'}");
    checkField(response, "name", "Test User");
    checkField(response, "color", "blue");
    jsonFree(response);

    //  An explicit value overrides the default
    response = invoke(up, "test_13", 200, "{name: 'Test User', color: 'green'}");
    checkField(response, "color", "green");
    jsonFree(response);
}

/*
    hasWild admits properties the signature does not declare. They have no field signature, so
    they are passed through unvalidated rather than rejected.
 */
static void testWildcardFields(Url *up)
{
    Json *response;

    response = invoke(up, "test_14", 200, "{name: 'Test User', extra: 'kept', another: 'also kept'}");
    checkField(response, "name", "Test User");
    checkField(response, "extra", "kept");
    checkField(response, "another", "also kept");
    jsonFree(response);

    //  A declared field on its own still validates
    response = invoke(up, "test_14", 200, "{name: 'Test User'}");
    checkField(response, "name", "Test User");
    jsonFree(response);

    //  Values of any type pass through, not just strings
    response = invoke(up, "test_14", 200,
                      "{name: 'n', num: 42, flag: true, obj: {a: 'b'}, arr: [1, 2]}");
    checkField(response, "num", "42");
    checkField(response, "flag", "true");
    checkField(response, "obj.a", "b");
    jsonFree(response);

    //  Without hasWild an undeclared field is rejected with a proper 400 under strict signatures
    jsonFree(invoke(up, "test_2", 400, "{email: 'a@b.com', undeclared: 'x'}"));
}

/*
    Under strict signatures a request with no signature at all, or a signature with no request
    block, must be refused rather than passed through unvalidated.
 */
static void testStrictMissingSignature(Url *up)
{
    //  No signature entry for this method name
    jsonFree(invoke(up, "no_such_method", 400, "{name: 'x'}"));

    //  A signature that declares only a response
    jsonFree(invoke(up, "test_15", 400, "{name: 'x'}"));
}

static void fiberMain(void *data)
{
    Url *up;

    if (setup(&HTTP, &HTTPS)) {
        up = urlAlloc(0);
        testFieldTypes(up);
        testUnknownType(up);
        testDropByRole(up);
        testDropByTag(up);
        testResponseDefault(up);
        testWildcardFields(up);
        testStrictMissingSignature(up);
        urlFree(up);
    }
    rFree(HTTP);
    rFree(HTTPS);
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
