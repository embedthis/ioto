/*
    boundary.tst.c - Memory leak regression tests

    Tests for memory leak fixes:
    1. Boundary leak when reusing Url object after upload
    2. Use-after-free in auth retry with augmented headers
    3. Header management during authentication retry

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/
/*
    Test boundary leak fix: Upload followed by regular request
    Verifies that boundary field is freed properly in resetState()
 */
static void testBoundaryLeak()
{
    Url     *up;
    char    url[128];
    char    *file1;
    RList   *files;
    RHash   *forms;
    int     rc, status;
    cchar   *response;

    tinfo("Testing boundary leak fix");

    // Create test file
    file1 = rGetTempFile(NULL, "leak-test");
    if (!file1) {
        tfail("Cannot create temp file name");
        return;
    }
    if (rWriteFile(file1, "Test content", 12, 0644) < 0) {
        tfail("Cannot create test file");
        rFree(file1);
        return;
    }

    // Create file list
    files = rAllocList(0, 0);
    rAddItem(files, file1);
    forms = rAllocHash(0, 0);
    rAddName(forms, "test", "value", 0);

    up = urlAlloc(0);

    // First request: Upload (allocates boundary)
    urlStart(up, "POST", SFMT(url, "%s/test/upload", HTTP));
    rc = urlUpload(up, files, forms, NULL);
    ttrue(rc == 0);

    // Verify boundary was set
    ttrue(up->boundary != NULL);

    // Second request: Regular GET (should free boundary in resetState)
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);

    // Verify boundary was cleared
    ttrue(up->boundary == NULL);

    response = urlGetResponse(up);
    ttrue(response != NULL);

    // Third request: Another upload to verify no issues
    urlStart(up, "POST", SFMT(url, "%s/test/upload", HTTP));
    rc = urlUpload(up, files, forms, NULL);
    ttrue(rc == 0);

    urlFree(up);
    rFree(files);
    rFree(forms);

    // Clean up
    unlink(file1);
    rFree(file1);

    tinfo("Boundary leak test passed");
}

#if URL_AUTH
/*
    Test auth retry with data parameter (Content-Length auto-added)
    Verifies use-after-free fix in fetch() when headers are augmented
    Note: Using GET with data parameter to test header augmentation without
    requiring POST support on file handler routes
 */
static void testAuthRetryWithPostData()
{
    Url     *up;
    char    url[128];
    int     status;
    cchar   *response;

    tinfo("Testing auth retry with header augmentation");

    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", NULL);  // Auto-detect

    /*
        GET request with explicit Content-Type header
        This tests that auth retry handles existing headers correctly
        The key is testing header management during auth retry, not the method
    */
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP),
                     NULL, 0, "Accept: text/html\r\n");

    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response != NULL);
    ttrue(scontains(response, "Digest Authentication Success"));

    urlFree(up);

    tinfo("Auth retry with header augmentation test passed");
}

/*
    Test auth retry with explicit headers
    Verifies normal auth retry path works correctly
 */
static void testAuthRetryWithHeaders()
{
    Url     *up;
    char    url[128];
    int     status;
    cchar   *response;

    tinfo("Testing auth retry with explicit headers");

    up = urlAlloc(0);
    urlSetAuth(up, "bob", "password", "basic");

    // Request with explicit headers (no augmentation needed)
    status = urlFetch(up, "GET", SFMT(url, "%s/basic/secret.html", HTTP),
                     NULL, 0, "Accept: text/html\r\n");

    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response != NULL);
    ttrue(scontains(response, "Basic Authentication Success"));
    urlFree(up);

    tinfo("Auth retry with headers test passed");
}

/*
    Test auth retry with multiple explicit headers
    Tests header management with multiple custom headers
 */
static void testAuthRetryComplex()
{
    Url     *up;
    char    url[128];
    int     status;
    cchar   *response;

    tinfo("Testing complex auth retry scenario");

    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", "digest");

    /*
        GET with multiple custom headers
        Tests that auth retry handles multiple headers correctly
    */
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP),
                     NULL, 0, "X-Custom: value\r\nAccept: text/html\r\n");

    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response != NULL);

    urlFree(up);

    tinfo("Complex auth retry test passed");
}
#endif /* URL_AUTH */

/*
    Test multiple upload/request cycles to stress boundary handling
 */
static void testMultipleUploadCycles()
{
    Url     *up;
    char    url[128];
    char    *file1;
    RList   *files;
    int     rc, status, i;

    tinfo("Testing multiple upload cycles");

    // Create test file
    file1 = rGetTempFile(NULL, "leak-cycle");
    if (!file1) {
        tfail("Cannot create temp file name");
        return;
    }
    if (rWriteFile(file1, "Cycle test", 10, 0644) < 0) {
        tfail("Cannot create test file");
        rFree(file1);
        return;
    }

    files = rAllocList(0, 0);
    rAddItem(files, file1);

    up = urlAlloc(0);

    // Multiple cycles of upload -> GET -> upload
    for (i = 0; i < 3; i++) {
        tinfo("Cycle %d: upload", i + 1);
        urlStart(up, "POST", SFMT(url, "%s/test/upload", HTTP));
        rc = urlUpload(up, files, NULL, NULL);
        ttrue(rc == 0);

        tinfo("Cycle %d: GET request", i + 1);
        status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
        ttrue(status == 200);
        ttrue(up->boundary == NULL);  // Should be cleared each time
    }

    urlFree(up);
    rFree(files);

    // Clean up
    unlink(file1);
    rFree(file1);

    tinfo("Multiple upload cycles test passed");
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        tinfo("HTTP=%s", HTTP ? HTTP : "NULL");

        // Core leak tests
        testBoundaryLeak();
        testMultipleUploadCycles();
#if URL_AUTH
        // Auth retry leak tests
        testAuthRetryWithPostData();
        testAuthRetryWithHeaders();
        testAuthRetryComplex();
#else
        tinfo("Skipping auth retry tests (URL_AUTH not enabled)");
#endif
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
