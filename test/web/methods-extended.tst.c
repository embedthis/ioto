/*
    methods-extended.tst.c - Extended HTTP method testing

    Tests additional HTTP method scenarios beyond the basic methods test.
    Covers case-insensitivity, OPTIONS verb, and method-specific behaviors.

    Based on Appweb test/basic/methods.tst.ts

    Coverage:
    - Case-insensitive method names
    - OPTIONS method with Allow header
    - HEAD method with no body
    - PUT file creation and updates
    - DELETE file removal
    - Method restrictions per route

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testCaselessMethods(void)
{
    Url  *up;
    char url[128];

    up = urlAlloc(0);

    // HTTP methods should be case-insensitive
    teqi(urlFetch(up, "GeT", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL), 200);

    urlClose(up);
    teqi(urlFetch(up, "get", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL), 200);

    urlClose(up);
    teqi(urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL), 200);

    urlFree(up);
}

static void testHEADMethod(void)
{
    Url   *up;
    char  url[128];
    cchar *clHeader, *response;
    int   status, contentLength;

    up = urlAlloc(0);

    // HEAD should return headers but no body (use trace route which allows HEAD)
    status = urlFetch(up, "HEAD", SFMT(url, "%s/trace/index.html", HTTP), NULL, 0, NULL);
    teqi(status, 200);

    // Should have Content-Length header
    clHeader = urlGetHeader(up, "Content-Length");
    contentLength = stoi(clHeader);
    tinfo("HEAD Content-Length header: '%s', parsed: %d", clHeader ? clHeader : "NULL", contentLength);
    tgti(contentLength, 0);

    // But no actual body (HEAD should not return body)
    response = urlGetResponse(up);
    ttrue(response == NULL || slen(response) == 0);

    urlFree(up);
}

static void testOPTIONSMethod(void)
{
    Url   *up;
    char  url[128];
    cchar *allow;
    int   status;

    up = urlAlloc(0);

    // OPTIONS on trace route (TRACE enabled)
    teqi(urlFetch(up, "OPTIONS", SFMT(url, "%s/trace/index.html", HTTP), NULL, 0, NULL), 200);

    allow = urlGetHeader(up, "Access-Control-Allow-Methods");
    if (allow) {
        // Should include standard methods
        tcontains(allow, "GET");
        tcontains(allow, "OPTIONS");
        // May include TRACE if enabled on that route
    }

    // OPTIONS on upload route - should be 405 (not in allowed methods list)
    urlClose(up);
    status = urlFetch(up, "OPTIONS", SFMT(url, "%s/upload/", HTTP), NULL, 0, NULL);
    teqi(status, 405);  // OPTIONS not allowed on /upload/ route

    urlFree(up);
}

static void testPUTMethodCreate(void)
{
    Url  *up;
    char url[128], data[128];
    int  pid, status;

    up = urlAlloc(0);

    pid = getpid();
    SFMT(data, "Test data from PID %d", pid);

    // PUT to create new file should return 201
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/put-test-%d.txt", HTTP, pid),
                      data, slen(data), "Content-Type: text/plain\r\n");
    teqi(status, 201);

    // Cleanup - delete the created file
    urlClose(up);
    status = urlFetch(up, "DELETE", url, NULL, 0, NULL);
    ttrue(status == 200 || status == 204);  // Keep ttrue for OR condition

    urlFree(up);
}

static void testPUTMethodUpdate(void)
{
    Url  *up;
    char url[128], data1[128], data2[128];
    int  pid, status;

    up = urlAlloc(0);

    pid = getpid();
    SFMT(data1, "Initial data %d", pid);
    SFMT(data2, "Updated data %d", pid);

    // Create file
    urlFetch(up, "PUT", SFMT(url, "%s/upload/put-update-%d.txt", HTTP, pid),
             data1, slen(data1), "Content-Type: text/plain\r\n");

    // Update file - should return 204 (No Content)
    urlClose(up);
    status = urlFetch(up, "PUT", url, data2, slen(data2), "Content-Type: text/plain\r\n");
    teqi(status, 204);

    // Cleanup - delete the file
    urlClose(up);
    status = urlFetch(up, "DELETE", url, NULL, 0, NULL);
    ttrue(status == 200 || status == 204);

    urlFree(up);
}

static void testDELETEMethod(void)
{
    Url  *up;
    char url[128], data[64];
    int  pid, status;

    up = urlAlloc(0);

    pid = getpid();
    SFMT(data, "Test data %d", pid);

    // Create file first
    urlFetch(up, "PUT", SFMT(url, "%s/upload/delete-test-%d.txt", HTTP, pid),
             data, slen(data), "Content-Type: text/plain\r\n");

    // DELETE should succeed with 204
    urlClose(up);
    status = urlFetch(up, "DELETE", url, NULL, 0, NULL);
    teqi(status, 204);

    // Verify file is gone
    urlClose(up);
    status = urlFetch(up, "GET", url, NULL, 0, NULL);
    teqi(status, 404);

    // DELETE non-existent file should return 404
    status = urlFetch(up, "DELETE", SFMT(url, "%s/upload/nonexistent-%d.txt", HTTP, pid), NULL, 0, NULL);
    teqi(status, 404);

    urlFree(up);
}

static void testPOSTMethod(void)
{
    Url   *up;
    Json  *json;
    char  url[128];
    cchar *body;

    up = urlAlloc(0);

    // POST with form data
    cchar *formData = "name=test&value=123";
    json = urlJson(up, "POST", SFMT(url, "%s/test/show", HTTP),
                   formData, slen(formData), "Content-Type: application/x-www-form-urlencoded\r\n");
    tnotnull(json);
    body = jsonGet(json, 0, "body", NULL);
    tnotnull(body);
    tcontains(body, "name=test");
    jsonFree(json);

    // POST with JSON
    urlClose(up);
    cchar *jsonData = "{\"test\":\"value\"}";
    json = urlJson(up, "POST", url, jsonData, slen(jsonData), "Content-Type: application/json\r\n");
    tnotnull(json);
    body = jsonGet(json, 0, "body", NULL);
    tnotnull(body);
    tcontains(body, "test");
    jsonFree(json);

    urlFree(up);
}

static void testTRACEDisabled(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // TRACE should be disabled by default on most routes
    status = urlFetch(up, "TRACE", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    teqi(status, 405);  // Method Not Allowed

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testCaselessMethods();
        testHEADMethod();
        testOPTIONSMethod();
        testPUTMethodCreate();
        testPUTMethodUpdate();
        testDELETEMethod();
        testPOSTMethod();
        testTRACEDisabled();
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
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
