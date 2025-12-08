

/*
    sig.c.tst - Unit tests for signature validation

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

#define JTEST(json, line, key, value)      jtest(json, line, key, value)
#define INVOKE(up, test, status, fmt, ...) invoke(up, test, status, __LINE__, fmt, ## __VA_ARGS__)

/************************************ Code ************************************/

static Json *invoke(Url *up, cchar *test, int status, int line, cchar *fmt, ...)
{
    va_list ap;
    char    url[128];
    Json    *json, *response;
    char    *body, *data;

    va_start(ap, fmt);
    data = sfmtv(fmt, ap);
    va_end(ap);

    if ((json = jsonParse(data, 0)) == 0) {
        tfail("Invalid JSON body");
        return 0;
    }
    body = jsonToString(json, 0, 0, JSON_JSON);
    jsonFree(json);

    urlClose(up);
    response = urlJson(up, "POST", SFMT(url, "%s/test/sig/controller/%s", HTTP, test), body, 0, NULL);
    rFree(data);

    if (urlGetStatus(up) != status) {
        rError("web", "Error at line %d, status %d (expected %d): %s", line, urlGetStatus(up), status,
               urlGetResponse(up));
        jsonFree(response);
        response = 0;
        urlClose(up);
        ttrue(urlGetStatus(up) == status);
    }
    return response;
}

static void jtest(Json *json, int line, cchar *key, cchar *value)
{
    char *result;

    result = jsonToString(json, 0, key, 0);
    if (value) {
        ttrue(result);
        if (smatch(strim(result, "'\"", R_TRIM_BOTH), value)) {
            ttrue(1);
        } else {
            rError("web", "Error at line %d, key %s=%s (expected %s)", line, key, result, value);
        }
    } else {
        ttrue(!result);
        if (result) {
            rError("web", "Error at line %d, key %s=%s (expected %s)", line, key, result, value);
        }
    }
    rFree(result);
}

static void test_0(Url *up)
{
    Json *response;

    response = INVOKE(up, "test_0", 200, NULL);
    ttrue(response && response->count == 0);
    jsonFree(response);

    response = INVOKE(up, "test_0", 400, "\"Unexpected data\"");
    jsonFree(response);

    response = INVOKE(up, "test_0?query=42", 200, "");
    jsonFree(response);
}

static void test_1(Url *up)
{
    Json *response;

    // Simple echo of primitive body
    response = INVOKE(up, "test_1", 200, "\"Hello World\"");
    ttrue(smatch(jsonString(response, JSON_JSON), "\"Hello World\""));
    jsonFree(response);
}

static void test_2(Url *up)
{
    Json *response;
    char body[160];

    response = INVOKE(up, "test_2", 200,
                      SFMT(body, SDEF({
        email: "test@test.com",
        name: "Test User",
        zip: 12345,
        age: 30
    }))
                      );
    JTEST(response, __LINE__, "name", "Test User");
    JTEST(response, __LINE__, "email", "test@test.com");
    JTEST(response, __LINE__, "zip", "12345");
    //  Age is discarded
    JTEST(response, __LINE__, "age", NULL);
    jsonFree(response);
}

static void test_2a(Url *up)
{
    Json *response;
    char body[160];

    //  Test missing required field
    response = INVOKE(up, "test_2", 400,
                      SFMT(body, SDEF({
        name: "Test User",
        zip: 12345,
        age: 30
    }))
                      );
    jsonFree(response);
}

static void test_3(Url *up)
{
    Json *response;

    //  Test nested array of objects
    response = INVOKE(up, "test_3", 200, "{ users: [ 'user1@test.com', 'user2@test.com' ] }");
    JTEST(response, __LINE__, "users", "['user1@test.com','user2@test.com']");
    jsonFree(response);
}

static void test_4(Url *up)
{
    Json *response;

    //  Test top-level array of strings
    response = INVOKE(up, "test_4", 200, "['red', 'green', 'blue']");
    JTEST(response, __LINE__, NULL, "['red','green','blue']");
    jsonFree(response);
}

static void test_5(Url *up)
{
    Json *response;

    //  Test default and omitted response
    response = INVOKE(up, "test_5", 200, NULL);
    JTEST(response, __LINE__, "color", "red");
    jsonFree(response);
}

static void test_6(Url *up)
{
    Json *response;
    char body[160];

    //  Test nested objects
    response = INVOKE(up, "test_6", 200, SFMT(body, SDEF({
        name: "Test User",
        address: {
            street: "123 Main St",
            zip: "12345"
        }
    })));
    JTEST(response, __LINE__, "name", "Test User");
    JTEST(response, __LINE__, "address.street", "123 Main St");
    JTEST(response, __LINE__, "address.zip", "12345");
    jsonFree(response);

    // Should fail
    response = INVOKE(up, "test_6", 400, SFMT(body, SDEF({
        name: "Test User",
        address: {
            mainStreet: "123 Main St",
        }
    })));
}

static void test_7(Url *up)
{
    Json *response;

    //  Test strict signatures with missing request fields
    response = INVOKE(up, "test_7", 400, "{}");
    jsonFree(response);

    //  Test strict signatures with missing response fields
    response = INVOKE(up, "test_7", 400, SDEF({
        name: "Test User",
    }));
    jsonFree(response);
}

static void test_8(Url *up)
{
    Json *response;

    //  Test strict signatures without fields definition -- should pass
    response = INVOKE(up, "test_8", 200, SDEF({
        name: "Test User",
        any: "Any field",
    }));
    JTEST(response, __LINE__, "name", "Test User");
    JTEST(response, __LINE__, "any", "Any field");
    JTEST(response, __LINE__, "missing", NULL);
    jsonFree(response);
}

/*
    Test without a content/type so web->vars won't get created
 */
static void test_9(Url *up)
{
    char url[128];
    int  status;

    urlClose(up);
    // Test an empty request
    status = urlFetch(up, "POST", SFMT(url, "%s/test/sig/controller/test_0", HTTP), NULL, (size_t) -1, NULL);
    ttrue(status == 200);

    // Test an empty object
    status = urlFetch(up, "POST", SFMT(url, "%s/test/sig/controller/test_8", HTTP), NULL, (size_t) -1, NULL);
    ttrue(status == 200);

    // Test an empty array
    urlClose(up);
    status = urlFetch(up, "POST", SFMT(url, "%s/test/sig/controller/test_4", HTTP), NULL, (size_t) -1, NULL);
    ttrue(status == 400);
}

static void fiberMain(void *data)
{
    Url *up;

    if (setup(&HTTP, &HTTPS)) {
        // up = urlAlloc(URL_SHOW_REQ_HEADERS | URL_SHOW_RESP_BODY | URL_SHOW_RESP_HEADERS);
        up = urlAlloc(0);
        test_0(up);
        test_1(up);
        test_2(up);
        test_2a(up);
        test_3(up);
        test_4(up);
        test_5(up);
        test_6(up);
        test_7(up);
        test_8(up);
        test_9(up);
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
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
