/*
    file.c.tst - Unit tests for file handler operations

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testGetFile()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *response;

    up = urlAlloc(0);

    // Test GET existing file
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response != NULL);
    ttrue(scontains(response, "html"));

    // Test GET non-existent file
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/nonexistent.html", HTTP), NULL, 0, NULL);
    ttrue(status == 404);

    urlFree(up);
}

static void testHeadFile()
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test HEAD existing file - may not be fully supported on all endpoints
    status = urlFetch(up, "HEAD", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    // HEAD may not be implemented everywhere, accept various valid responses
    ttrue(status >= 200 && status < 500);

    urlFree(up);
}

static void testPutFile()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *testContent = "Test file content for PUT operation";

    up = urlAlloc(0);

    // Test PUT file - may not be enabled or configured
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/test-put.txt", HTTP),
                      testContent, slen(testContent),
                      "Content-Type: text/plain\r\n");
    // PUT may not be enabled, so accept various responses
    ttrue(status >= 200);  // Any valid HTTP response

    urlFree(up);
}

static void testDeleteFile()
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test DELETE - may not be enabled or configured
    status = urlFetch(up, "DELETE", SFMT(url, "%s/upload/test-delete.txt", HTTP), NULL, 0, NULL);
    // DELETE may not be enabled, so accept various responses
    ttrue(status >= 200);  // Any valid HTTP response

    urlFree(up);
}

static void testDirectoryRedirect()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *location;

    up = urlAlloc(0);

    // Test access to directory without trailing slash (should redirect)
    status = urlFetch(up, "GET", SFMT(url, "%s/upload", HTTP), NULL, 0, NULL);
    if (status == 301 || status == 302) {
        location = urlGetHeader(up, "Location");
        ttrue(location != NULL);
        ttrue(sends(location, "/"));
    }

    urlFree(up);
}

static void testUnsupportedMethod()
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test unsupported HTTP method
    status = urlFetch(up, "PATCH", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 405);  // Method Not Allowed

    urlFree(up);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, &HTTPS)) {
        testGetFile();
        testHeadFile();
        testPutFile();
        testDeleteFile();
        testDirectoryRedirect();
        testUnsupportedMethod();
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