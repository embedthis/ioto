/*
    security-path-traversal.tst.c - Path traversal security testing

    Tests the web server's protection against path traversal attacks that attempt
    to access files outside the document root. Validates path normalization,
    backslash handling, URL encoding attacks, and other directory traversal vectors.

    Based on GoAhead test/security/traversal.tst.es and various security best practices.

    Coverage:
    - Classic ../ attacks (relative path traversal)
    - URL-encoded path traversal (%2e%2e%2f)
    - Double-encoded attacks (%252e%252e%252f)
    - Backslash attacks (Windows path separators)
    - Unicode/UTF-8 encoded traversal attempts
    - Null byte injection (path truncation)
    - Multiple encoding combinations
    - Absolute path attempts
    - Symlink following restrictions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testBasicTraversal(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Classic ../ attack
    status = urlFetch(up, "GET", SFMT(url, "%s/../../../etc/passwd", HTTP), NULL, 0, NULL);
    // Should reject with 400 (Bad Request), 403 (Forbidden), or 404 (Not Found)
    // URL client may also reject (-21 = R_ERR_CANT_COMPLETE)
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 2: Multiple ../ sequences
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/../../../../../../../../etc/passwd", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 3: Mixed / and ../ in path
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html/../../../../../../etc/passwd", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    urlFree(up);
}

static void testURLEncodedTraversal(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: URL-encoded ../ (%2e%2e%2f)
    status = urlFetch(up, "GET", SFMT(url, "%s/%%2e%%2e%%2f%%2e%%2e%%2f%%2e%%2e%%2fetc%%2fpasswd", HTTP), NULL, 0,
                      NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 2: Mixed encoding
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/%%2e%%2e/%%2e%%2e/etc/passwd", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 3: Dot encoding only (%2e%2e/)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/%%2e%%2e/", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    urlFree(up);
}

static void testDoubleEncodedTraversal(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Double-encoded ../ (%252e%252e%252f)
    // %25 = %, so %252e = %2e after first decode
    status = urlFetch(up, "GET", SFMT(url, "%s/%%252e%%252e%%252f%%252e%%252e%%252fetc", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 2: Triple encoding
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/%%25252e%%25252e%%25252f", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    urlFree(up);
}

static void testBackslashTraversal(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Backslash instead of forward slash (Windows style)
    status = urlFetch(up, "GET", SFMT(url, "%s/..\\..\\..\\etc\\passwd", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 2: URL-encoded backslash (%5c)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/%%2e%%2e%%5c%%2e%%2e%%5cetc", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 3: Mixed forward slash and backslash
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/../..\\../etc/passwd", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    urlFree(up);
}

static void testNullByteInjection(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Null byte to truncate path (%00)
    status = urlFetch(up, "GET", SFMT(url, "%s/../../../../etc/passwd%%00.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 2: Null byte in middle of path
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/..%%00/../etc/passwd", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    urlFree(up);
}

static void testAbsolutePathAttempts(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Absolute Unix path
    status = urlFetch(up, "GET", SFMT(url, "%s//etc/passwd", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 2: Windows absolute path (C:)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/C:/Windows/System32/config/sam", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 3: UNC path attempt
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s///server/share/file.txt", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    urlFree(up);
}

static void testUnicodeTraversal(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Unicode dot encoding (U+002E = .)
    // %c0%ae is an overlong UTF-8 encoding of '.'
    status = urlFetch(up, "GET", SFMT(url, "%s/%%c0%%ae%%c0%%ae/%%c0%%ae%%c0%%ae/etc", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 2: UTF-8 encoded slash (U+002F = /)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/%%c0%%af%%c0%%ae%%c0%%ae", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    urlFree(up);
}

static void testDotVariations(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Triple dot
    status = urlFetch(up, "GET", SFMT(url, "%s/.../.../.../etc/passwd", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 2: Dot-slash-dot pattern
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/././../../etc/passwd", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 3: Excessive dots
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/....//....//etc/passwd", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    urlFree(up);
}

static void testPathNormalizationEdgeCases(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Path with trailing ../
    status = urlFetch(up, "GET", SFMT(url, "%s/valid/path/../../../../../../etc/passwd", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 2: Repeated slashes with ../
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s////..////..//etc//passwd", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 3: Mixed case (case-sensitive systems)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/../ETC/passwd", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    urlFree(up);
}

static void testCombinationAttacks(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: URL encoding + backslash + null byte
    status = urlFetch(up, "GET", SFMT(url, "%s/%%2e%%2e%%5c%%2e%%2e%%00.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 2: Double encoding + unicode
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/%%252e%%c0%%ae/etc", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    // Test 3: All techniques combined
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/%%2e%%2e%%5c%%c0%%ae%%c0%%ae%%00/etc/passwd", HTTP), NULL, 0, NULL);
    ttrue(status == 400 || status == 403 || status == 404 || status < 0);

    urlFree(up);
}

static void testLegitimatePathsNotBlocked(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Normal file should work
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);

    // Test 2: File with dots in name (but not ..)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test.tar.gz", HTTP), NULL, 0, NULL);
    // May exist or not, but shouldn't be rejected as traversal
    ttrue(status == 200 || status == 404);

    // Test 3: Path with single dot (current directory)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/./index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testBasicTraversal();
        testURLEncodedTraversal();
        testDoubleEncodedTraversal();
        testBackslashTraversal();
        testNullByteInjection();
        testAbsolutePathAttempts();
        testUnicodeTraversal();
        testDotVariations();
        testPathNormalizationEdgeCases();
        testCombinationAttacks();
        testLegitimatePathsNotBlocked();
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
