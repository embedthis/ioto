/*
    methods.c.tst - Unit tests for HTTP methods (PUT, DELETE, etc.)

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

static void testPutMethod()
{
    Url     *up;
    char    url[128], *response;
    char    *data = "Hello World";
    int     status;

    up = urlAlloc(0);

    status = urlFetch(up, "PUT", SFMT(url, "%s/test/put", HTTP), data, 0, NULL);
    ttrue(status == 200);

    response = urlGetResponse(up);
    teqi((int) stoi(response), slen(data));
    urlFree(up);
}

static void testDeleteMethod()
{
    Url     *up;
    Json    *json;
    char    url[128];
    int     status;

    up = urlAlloc(0);

    status = urlFetch(up, "DELETE", SFMT(url, "%s/test/show", HTTP), 0, 0, 0);
    ttrue(status == 200);

    json = urlGetJsonResponse(up);
    tmatch(jsonGet(json, 0, "method", 0), "DELETE");
    jsonFree(json);

    urlFree(up);
}

static void testHeadMethod()
{
    Url     *up;
    cchar   *response;
    char    url[128];
    int     status;

    up = urlAlloc(0);

    status = urlFetch(up, "HEAD", SFMT(url, "%s/test/show", HTTP), 0, 0, 0);
    ttrue(status == 200);

    // HEAD requests should have headers but no body
    ttrue(urlGetHeader(up, "Content-Type") != 0);
    ttrue(urlGetHeader(up, "Content-Length") || urlGetHeader(up, "Transfer-Encoding"));

    response = urlGetResponse(up);
    ttrue(response == 0 || slen(response) == 0);

    urlFree(up);
}

static void testPatchMethod()
{
    Url     *up;
    Json    *json;
    char    url[128];
    char    *data = "{\"name\": \"updated\"}";
    int     status;

    up = urlAlloc(0);

    status = urlFetch(up, "PATCH", SFMT(url, "%s/test/show", HTTP),
                     data, 0, "Content-Type: application/json\r\n");
    ttrue(status == 200);

    json = urlGetJsonResponse(up);
    tmatch(jsonGet(json, 0, "method", 0), "PATCH");
    tmatch(jsonGet(json, 0, "form.name", 0), "updated");
    jsonFree(json);

    urlFree(up);
}

static void testOptionsMethod()
{
    Url     *up;
    cchar   *header;
    char    url[128];
    int     status;

    up = urlAlloc(0);

    status = urlFetch(up, "OPTIONS", SFMT(url, "%s/test/show", HTTP), 0, 0, 0);
    ttrue(status == 200);

    // OPTIONS should return allowed methods
    header = urlGetHeader(up, "Allow");
    if (header) {
        ttrue(scontains(header, "GET"));
        ttrue(scontains(header, "POST"));
    }

    urlFree(up);
}

static void testInvalidMethod()
{
    Url     *up;
    char    url[128];
    int     status;

    up = urlAlloc(0);

    // Test with invalid HTTP method
    status = urlFetch(up, "INVALID", SFMT(url, "%s/test/show", HTTP), 0, 0, 0);
    ttrue(status >= 400);  // Should return error status

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testPutMethod();
        testDeleteMethod();
        testHeadMethod();
        testPatchMethod();
        testOptionsMethod();
        testInvalidMethod();
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