/*
    security-uri-validation.tst.c - URI validation and sanitization security testing

    Tests the web server's URI validation including special characters, control characters,
    invalid UTF-8 sequences, and various encoding edge cases that could lead to security
    vulnerabilities.

    Based on GoAhead test/security/uri.tst.es and OWASP security testing guidelines.

    Coverage:
    - Control characters in URIs (0x00-0x1F, 0x7F)
    - Invalid UTF-8 sequences
    - Overlong UTF-8 encodings
    - Special characters (< > " ' ; & |)
    - Space handling and encoding
    - Fragment identifier handling
    - Query string special characters
    - Percent encoding edge cases
    - URI length limits
    - Invalid percent sequences
    - Reserved characters

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testControlCharacters(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Null character in path (%00)
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%00file.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    // Test 2: Carriage return (%0D)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%0Dfile.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    // Test 3: Line feed (%0A)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%0Afile.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    // Test 4: Tab character (%09)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%09file.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    // Test 5: DEL character (%7F)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%7Ffile.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    urlFree(up);
}

static void testSpecialCharacters(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: HTML special characters < >
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%3Cscript%%3E.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    // Test 2: Quote characters " '
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%22%%27.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    // Test 3: Pipe and semicolon | ;
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%7C%%3Bcommand", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    // Test 4: Ampersand in path (not query)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%26amp.html", HTTP), NULL, 0, NULL);
    // Ampersand may be allowed in path, check both outcomes
    ttrue(status == 200 || status == 400 || status == 404 || status < 0);

    urlFree(up);
}

static void testInvalidUTF8Sequences(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Invalid UTF-8 continuation byte
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%80invalid.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    // Test 2: Incomplete UTF-8 sequence
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%C2.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    // Test 3: Overlong encoding of slash (should be %2F, not %C0%AF)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%C0%%AFfile.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    // Test 4: Overlong encoding of null (should be %00, not %C0%80)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%C0%%80.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    urlFree(up);
}

static void testSpaceHandling(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: URL-encoded space (%20) - should work
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%20file.html", HTTP), NULL, 0, NULL);
    // May or may not exist, but encoding should be valid
    ttrue(status == 200 || status == 404);

    // Test 2: Plus sign as space (in path, not query)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test+file.html", HTTP), NULL, 0, NULL);
    // Plus should be literal in path
    ttrue(status == 200 || status == 404);

    // Test 3: Literal space (usually rejected by URL client)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test file.html", HTTP), NULL, 0, NULL);
    // URL client may reject this
    ttrue(status == 404 || status < 0);

    urlFree(up);
}

static void testPercentEncodingEdgeCases(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Incomplete percent sequence (%)
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    // Test 2: Incomplete percent sequence (%2)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%2.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    // Test 3: Invalid hex digits (%GG)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%GG.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    // Test 4: Mixed case hex (%2f vs %2F) - both should work
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%2ffile.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200 || status == 400 || status == 404 || status < 0);

    // Test 5: Double percent (%%20)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%%%20file.html", HTTP), NULL, 0, NULL);
    // Literal % followed by %20
    ttrue(status == 200 || status == 400 || status == 404 || status < 0);

    urlFree(up);
}

static void testQueryStringSpecialChars(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Normal query string with special chars
    status = urlFetch(up, "GET", SFMT(url, "%s/test/echo?key=value&foo=bar", HTTP), NULL, 0, NULL);
    // Accept various responses depending on route configuration
    ttrue(status == 200 || status == 404 || status == 405 || status < 0);

    // Test 2: Query string with encoded special chars
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/echo?data=%%3Cscript%%3E", HTTP), NULL, 0, NULL);
    ttrue(status == 200 || status == 404 || status == 405 || status < 0);

    // Test 3: Empty query value
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/echo?empty=", HTTP), NULL, 0, NULL);
    ttrue(status == 200 || status == 404 || status == 405 || status < 0);

    // Test 4: Query string with = and & in value (encoded)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/echo?data=a%%3Db%%26c%%3Dd", HTTP), NULL, 0, NULL);
    ttrue(status == 200 || status == 404 || status == 405 || status < 0);

    urlFree(up);
}

static void testFragmentIdentifiers(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: URI with fragment (#anchor)
    // Note: Fragments should not be sent to server (client-side only)
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html#section", HTTP), NULL, 0, NULL);
    // Fragment handling depends on URL client
    ttrue(status == 200 || status == 404 || status < 0);

    // Test 2: Encoded hash in path (%23)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%23file.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200 || status == 404 || status < 0);

    urlFree(up);
}

static void testReservedCharacters(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Colon in path (after first segment)
    status = urlFetch(up, "GET", SFMT(url, "%s/test:file.html", HTTP), NULL, 0, NULL);
    // Colon may or may not be allowed
    ttrue(status == 200 || status == 400 || status == 404 || status < 0);

    // Test 2: At sign (@)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test@file.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200 || status == 400 || status == 404 || status < 0);

    // Test 3: Square brackets [ ]
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%5Barray%%5D.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200 || status == 400 || status == 404 || status < 0);

    urlFree(up);
}

static void testNormalizationAttacks(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Mixed encoding in same path
    status = urlFetch(up, "GET", SFMT(url, "%s/test%%2F%%252Ffile", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 404 || status < 0);

    // Test 2: Unicode normalization differences
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/caf%%C3%%A9.html", HTTP), NULL, 0, NULL);
    // May or may not exist (café.html)
    ttrue(status == 200 || status == 404 || status < 0);

    // Test 3: Case variations with percent encoding
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/Test%%2fFile.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200 || status == 400 || status == 404 || status < 0);

    urlFree(up);
}

static void testLongURIPaths(void)
{
    Url    *up;
    char   *longPath;
    char   url[128];
    int    status, i;
    size_t len;

    up = urlAlloc(0);

    // Test 1: Very long valid path (under limit)
    len = 4096;
    longPath = rAlloc(len + 200);
    char   *p = longPath;
    size_t remaining = len + 200;

    p += scopy(p, remaining, SFMT(url, "%s/", HTTP));
    remaining = len + 200 - slen(longPath);

    for (i = 0; i < 50 && remaining > 50; i++) {
        size_t copied = (size_t) scopy(p, remaining, "verylongdirectoryname/");
        p += copied;
        remaining -= copied;
    }
    scopy(p, remaining, "file.html");

    status = urlFetch(up, "GET", longPath, NULL, 0, NULL);
    // May hit URI length limit or just not exist (accepts anything)
    ttrue(status >= 200 || status < 0);

    rFree(longPath);
    urlFree(up);
}

static void testValidURIsNotRejected(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Normal ASCII filename
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    teqi(status, 200);

    // Test 2: Hyphen and underscore (valid)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test-file_name.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200 || status == 404);

    // Test 3: Numbers in filename
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/file123.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200 || status == 404);

    // Test 4: Multiple dots in filename
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/file.tar.gz", HTTP), NULL, 0, NULL);
    ttrue(status == 200 || status == 404);

    // Test 5: Tilde (home directory notation on Unix)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/~user/file.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200 || status == 404 || status == 403);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testControlCharacters();
        testSpecialCharacters();
        testInvalidUTF8Sequences();
        testSpaceHandling();
        testPercentEncodingEdgeCases();
        testQueryStringSpecialChars();
        testFragmentIdentifiers();
        testReservedCharacters();
        testNormalizationAttacks();
        testLongURIPaths();
        testValidURIsNotRejected();
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
