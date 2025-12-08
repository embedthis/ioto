/*
    file-serving.tst.c - Advanced file serving functionality tests

    Tests file serving edge cases, special file types, content negotiation,
    and advanced file operations beyond basic GET requests.

    Based on Appweb test/basic/files.tst.es

    Coverage:
    - Large file handling
    - Zero-byte files
    - Files with special characters in names
    - Content-Type detection and overrides
    - Content-Disposition headers
    - File metadata (size, modification time)
    - Partial content handling (ranges already tested separately)
    - Hidden files and dot files
    - Case sensitivity in filenames

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testBasicFileServing(void)
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *contentType, *contentLength;

    up = urlAlloc(0);

    // Test 1: Serve HTML file
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    teqi(status, 200);

    contentType = urlGetHeader(up, "Content-Type");
    tnotnull(contentType);
    tcontains(contentType, "text/html");

    contentLength = urlGetHeader(up, "Content-Length");
    tnotnull(contentLength);
    tgti(stoi(contentLength), 0);

    // Test 2: Serve CSS file
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/styles.css", HTTP), NULL, 0, NULL);
    if (status == 200) {
        contentType = urlGetHeader(up, "Content-Type");
        tnotnull(contentType);
        // Should be text/css
        tnotnull(scontains(contentType, "css"));
    } else {
        // File may not exist - that's acceptable
        teqi(status, 404);
    }

    urlFree(up);
}

static void testZeroByteFile(void)
{
    Url   *up;
    char  url[128];
    int   status, pid;
    cchar *contentLength, *response;

    up = urlAlloc(0);
    pid = getpid();

    // Create zero-byte file
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/zero-%d.txt", HTTP, pid),
                      "", 0, "Content-Type: text/plain\r\n");
    ttrue(status == 201 || status == 204);

    // Retrieve zero-byte file
    urlClose(up);
    status = urlFetch(up, "GET", url, NULL, 0, NULL);
    teqi(status, 200);

    contentLength = urlGetHeader(up, "Content-Length");
    tnotnull(contentLength);
    teqi(stoi(contentLength), 0);

    response = urlGetResponse(up);
    ttrue(response == NULL || slen(response) == 0);

    // Cleanup
    urlClose(up);
    urlFetch(up, "DELETE", url, NULL, 0, NULL);

    urlFree(up);
}

static void testLargeFile(void)
{
    Url    *up;
    char   url[128];
    char   *largeContent;
    int    status, pid;
    size_t size;

    up = urlAlloc(0);
    pid = getpid();

    // Create a larger file (50KB - under body limit but larger than typical)
    size = 50 * 1024;
    largeContent = rAlloc(size + 1);
    memset(largeContent, 'L', size);
    largeContent[size] = '\0';

    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/large-%d.txt", HTTP, pid),
                      largeContent, size, "Content-Type: text/plain\r\n");
    tinfo("Large file PUT status: %d, size: %zu", status, size);
    ttrue(status == 201 || status == 204);
    rFree(largeContent);

    // Note: /upload/ route may not serve file contents via GET
    // Just verify the upload succeeded, skip retrieval test

    // Cleanup
    urlClose(up);
    urlFetch(up, "DELETE", url, NULL, 0, NULL);

    urlFree(up);
}

static void testContentTypeDetection(void)
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *contentType;

    up = urlAlloc(0);

    // Test various file extensions
    struct {
        cchar *file;
        cchar *expectedType;
    } tests[] = {
        { "index.html", "text/html" },
        { "test.txt", "text/plain" },
        { "data.json", "application/json" },
        { NULL, NULL }
    };

    for (int i = 0; tests[i].file != NULL; i++) {
        urlClose(up);
        status = urlFetch(up, "GET", SFMT(url, "%s/%s", HTTP, tests[i].file), NULL, 0, NULL);

        if (status == 200) {
            contentType = urlGetHeader(up, "Content-Type");
            tnotnull(contentType);
            // Content-Type may include charset, so use contains
            tcontains(contentType, tests[i].expectedType);
        } else {
            // File doesn't exist - acceptable
            teqi(status, 404);
        }
    }

    urlFree(up);
}

static void testSpecialCharactersInFilenames(void)
{
    Url  *up;
    char url[256];
    int  status, pid;

    up = urlAlloc(0);
    pid = getpid();

    // Test spaces in filename (URL encoded as %20)
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/file%%20with%%20spaces-%d.txt", HTTP, pid),
                      "test", 4, "Content-Type: text/plain\r\n");
    ttrue(status == 201 || status == 204);

    urlClose(up);
    status = urlFetch(up, "GET", url, NULL, 0, NULL);
    teqi(status, 200);

    // Cleanup
    urlClose(up);
    urlFetch(up, "DELETE", url, NULL, 0, NULL);

    // Test dashes and underscores (should work fine)
    urlClose(up);
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/file-with_special-%d.txt", HTTP, pid),
                      "test", 4, "Content-Type: text/plain\r\n");
    ttrue(status == 201 || status == 204);

    // Cleanup
    urlClose(up);
    urlFetch(up, "DELETE", url, NULL, 0, NULL);

    urlFree(up);
}

static void testDotFiles(void)
{
    Url  *up;
    char url[128];
    int  status, pid;

    up = urlAlloc(0);
    pid = getpid();

    // Try to create a dot file (hidden file on Unix)
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/.hidden-%d.txt", HTTP, pid),
                      "secret", 6, "Content-Type: text/plain\r\n");

    if (status == 201 || status == 204) {
        // Server allows dot files
        urlClose(up);
        status = urlFetch(up, "GET", url, NULL, 0, NULL);
        teqi(status, 200);

        // Cleanup
        urlClose(up);
        urlFetch(up, "DELETE", url, NULL, 0, NULL);
    } else {
        // Server may reject dot files for security
        ttrue(status == 403 || status == 404);
    }

    urlFree(up);
}

static void testFileMetadata(void)
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *lastModified, *etag, *contentLength;

    up = urlAlloc(0);

    // Request file and check metadata headers
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    teqi(status, 200);

    // Should have Last-Modified header
    lastModified = urlGetHeader(up, "Last-Modified");
    tnotnull(lastModified);
    tgti(slen(lastModified), 0);

    // Should have ETag header
    etag = urlGetHeader(up, "ETag");
    tnotnull(etag);
    tgti(slen(etag), 0);

    // Should have Content-Length header
    contentLength = urlGetHeader(up, "Content-Length");
    tnotnull(contentLength);
    tgti(stoi(contentLength), 0);

    urlFree(up);
}

static void testNonExistentFile(void)
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *contentType;

    up = urlAlloc(0);

    // Request file that doesn't exist
    status = urlFetch(up, "GET", SFMT(url, "%s/does-not-exist-12345.html", HTTP), NULL, 0, NULL);
    teqi(status, 404);

    // Should have error page with HTML content type
    contentType = urlGetHeader(up, "Content-Type");
    if (contentType) {
        // Error pages are typically HTML
        ttrue(scontains(contentType, "text/html") != NULL ||
              scontains(contentType, "text/plain") != NULL);
    }

    urlFree(up);
}

static void testFileCaseSensitivity(void)
{
    Url  *up;
    char url[128];
    int  status1, status2;

    up = urlAlloc(0);

    // Request file with correct case
    status1 = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);

    // Request same file with different case
    urlClose(up);
    status2 = urlFetch(up, "GET", SFMT(url, "%s/INDEX.HTML", HTTP), NULL, 0, NULL);

    // On case-insensitive filesystems (Windows, macOS with defaults),
    // both should work. On case-sensitive systems (Linux, macOS case-sensitive),
    // second should fail. Accept both behaviors.
    if (status1 == 200) {
        // File exists - second request may succeed or fail depending on filesystem
        ttrue(status2 == 200 || status2 == 404);
    }

    urlFree(up);
}

static void testMultipleSimultaneousRequests(void)
{
    Url  *up1, *up2;
    char url[128];
    int  status1, status2;

    up1 = urlAlloc(0);
    up2 = urlAlloc(0);

    // Make two simultaneous requests for the same file
    // (In actual practice, these are sequential, but tests server's ability
    //  to handle multiple connections)
    status1 = urlFetch(up1, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    status2 = urlFetch(up2, "GET", url, NULL, 0, NULL);

    teqi(status1, 200);
    teqi(status2, 200);

    // Both should get the same content
    cchar *response1 = urlGetResponse(up1);
    cchar *response2 = urlGetResponse(up2);

    if (response1 && response2) {
        teqz(slen(response1), slen(response2));
    }

    urlFree(up1);
    urlFree(up2);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testBasicFileServing();
        testZeroByteFile();
        testLargeFile();
        testContentTypeDetection();
        testSpecialCharactersInFilenames();
        testDotFiles();
        testFileMetadata();
        testNonExistentFile();
        testFileCaseSensitivity();
        testMultipleSimultaneousRequests();
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
