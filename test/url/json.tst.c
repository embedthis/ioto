/*
    json.tst.c - Unit tests for the JSON convenience APIs

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

/*
    urlGetJson allocates its own Url and returns the parsed response
 */
static void testGetJson()
{
    Json    *json;
    char    url[128];

    json = urlGetJson(SFMT(url, "%s/test/show", HTTP), NULL);
    ttrue(json != NULL);
    if (json) {
        tmatch(jsonGet(json, 0, "method", 0), "GET");
        tmatch(jsonGet(json, 0, "url", 0), "/test/show");
        jsonFree(json);
    }
}

/*
    urlGetJson with caller supplied headers
 */
static void testGetJsonWithHeaders()
{
    Json    *json;
    char    url[128];

    json = urlGetJson(SFMT(url, "%s/test/show", HTTP), "X-Test-Header: %s\r\n", "json-value");
    ttrue(json != NULL);
    if (json) {
        tmatch(jsonGet(json, 0, "headers.X-Test-Header", 0), "json-value");
        jsonFree(json);
    }
}

/*
    A response that is not JSON must not be returned as a document
 */
static void testGetJsonNotJson()
{
    Json    *json;
    char    url[128];

    json = urlGetJson(SFMT(url, "%s/index.html", HTTP), NULL);
    ttrue(json == NULL);
}

/*
    A failed request must return NULL, not an empty document
 */
static void testGetJsonBadStatus()
{
    Json    *json;
    char    url[128];

    json = urlGetJson(SFMT(url, "%s/missing-document.json", HTTP), NULL);
    ttrue(json == NULL);
}

static void testGetJsonBadHost()
{
    Json    *json;

    json = urlGetJson("http://localhost:1/test/show", NULL);
    ttrue(json == NULL);
}

/*
    urlPostJson posts a body and parses the response
 */
static void testPostJson()
{
    Json    *json;
    cchar   *body;
    char    url[128];

    body = "{\"name\":\"test\"}";
    json = urlPostJson(SFMT(url, "%s/test/show", HTTP), body, slen(body), NULL);
    ttrue(json != NULL);
    if (json) {
        tmatch(jsonGet(json, 0, "method", 0), "POST");
        tmatch(jsonGet(json, 0, "body", 0), body);
        tmatch(jsonGet(json, 0, "headers.Content-Type", 0), "application/json");
        jsonFree(json);
    }
}

/*
    urlJson with a caller supplied Url object reuses the connection
 */
static void testJsonWithUrl()
{
    Json    *json;
    Url     *up;
    char    url[128];

    up = urlAlloc(0);

    json = urlJson(up, "GET", SFMT(url, "%s/test/show", HTTP), NULL, 0, NULL);
    ttrue(json != NULL);
    if (json) {
        tmatch(jsonGet(json, 0, "method", 0), "GET");
        jsonFree(json);
    }
    //  Second request on the same object must reuse the connection
    json = urlJson(up, "POST", SFMT(url, "%s/test/show", HTTP), "abc", 3, NULL);
    ttrue(json != NULL);
    if (json) {
        tmatch(jsonGet(json, 0, "method", 0), "POST");
        tmatch(jsonGet(json, 0, "body", 0), "abc");
        jsonFree(json);
    }
    ttrue(urlGetStatus(up) == 200);
    urlFree(up);
}

/*
    urlJson with a NULL Url allocates a temporary one
 */
static void testJsonWithoutUrl()
{
    Json    *json;
    char    url[128];

    json = urlJson(NULL, "GET", SFMT(url, "%s/test/show", HTTP), NULL, 0, NULL);
    ttrue(json != NULL);
    if (json) {
        tmatch(jsonGet(json, 0, "method", 0), "GET");
        jsonFree(json);
    }
}

/*
    urlJson must report a bad status as a NULL document
 */
static void testJsonBadStatus()
{
    Json    *json;
    Url     *up;
    char    url[128];

    up = urlAlloc(0);
    json = urlJson(up, "GET", SFMT(url, "%s/missing-document.json", HTTP), NULL, 0, NULL);
    ttrue(json == NULL);
    ttrue(urlGetStatus(up) == 404);
    urlFree(up);
}

/*
    urlGetJsonResponse parses the buffered response of a completed request
 */
static void testGetJsonResponse()
{
    Json    *json;
    Url     *up;
    char    url[128];

    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", SFMT(url, "%s/test/show", HTTP), NULL, 0, NULL) == 200);

    json = urlGetJsonResponse(up);
    ttrue(json != NULL);
    if (json) {
        tmatch(jsonGet(json, 0, "url", 0), "/test/show");
        jsonFree(json);
    }
    urlFree(up);
}

/*
    urlGetJsonResponse on a non-JSON body must fail and record an error
 */
static void testGetJsonResponseNotJson()
{
    Json    *json;
    Url     *up;
    char    url[128];

    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL) == 200);

    json = urlGetJsonResponse(up);
    ttrue(json == NULL);
    ttrue(urlGetError(up) != NULL);
    tcontains(urlGetError(up), "Cannot parse json");
    urlFree(up);
}

/*
    Null tolerance
 */
static void testJsonNullArgs()
{
    ttrue(urlGetJson(NULL, NULL) == NULL);
    ttrue(urlPostJson(NULL, "abc", 3, NULL) == NULL);
    ttrue(urlGetJsonResponse(NULL) == NULL);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testGetJson();
        testGetJsonWithHeaders();
        testGetJsonNotJson();
        testGetJsonBadStatus();
        testGetJsonBadHost();
        testPostJson();
        testJsonWithUrl();
        testJsonWithoutUrl();
        testJsonBadStatus();
        testGetJsonResponse();
        testGetJsonResponseNotJson();
        testJsonNullArgs();
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
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
