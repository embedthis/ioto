/*
    put-large.tst.c - PUT file testing and edge cases

    Tests PUT-based file writes to /upload/ route. These PUTs are limited
    by the body size limit (100KB) rather than the multipart upload limit (20MB).
    Focuses on edge cases, security validation, and resource management.

    Note: For multipart upload testing, see upload.tst.c and upload-multipart.tst.c

    Coverage:
    - PUT at various sizes (under 100KB body limit)
    - Boundary condition testing (near limit)
    - Progressive PUT (buffering)
    - Filename sanitization and path traversal prevention
    - PUT cleanup and lifecycle
    - Concurrent PUTs
    - Empty file PUTs
    - Content-Length validation

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Helpers *********************************/

/*
    Get the size of a PUT file in ./site/upload/
    Returns file size or -1 on error
 */
static ssize getPutSize(cchar *filename)
{
    char path[256];

    SFMT(path, "./site/upload/%s", filename);
    return rGetFileSize(path);
}

/************************************ Code ************************************/

static void testPutLargeUnderLimit(void)
{
    Url    *up;
    char   url[128], filename[64], *largeData;
    int    status, pid;
    size_t putSize;
    ssize  fileSize;

    up = urlAlloc(0);
    pid = getpid();

    // PUT file under 100KB body limit (use 50KB for test)
    putSize = 50 * 1024;  // 50KB
    largeData = rAlloc(putSize + 1);
    memset(largeData, 'L', putSize);
    largeData[putSize] = '\0';

    SFMT(filename, "large-%d.dat", pid);
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/%s", HTTP, filename),
                      largeData, putSize, "Content-Type: application/octet-stream\r\n");

    tinfo("Large file PUT status: %d, size: %zu KB", status, putSize / 1024);
    ttrue(status == 201 || status == 204);

    // Verify file size on disk
    fileSize = getPutSize(filename);
    teqz(fileSize, (ssize) putSize);

    rFree(largeData);

    // Cleanup
    urlClose(up);
    urlFetch(up, "DELETE", url, NULL, 0, NULL);

    urlFree(up);
}

static void testPutAtLimit(void)
{
    Url    *up;
    char   url[128], filename[64], *limitData;
    int    status, pid;
    size_t putLimit;
    ssize  fileSize;

    up = urlAlloc(0);
    pid = getpid();

    /*
        Testing at body limit (100KB)
        Note: PUT uses body limit, not multipart upload limit
     */
    putLimit = 95 * 1024;  // 95KB (just under limit with headers)
    limitData = rAlloc(putLimit + 1);
    memset(limitData, 'M', putLimit);
    limitData[putLimit] = '\0';

    SFMT(filename, "limit-%d.dat", pid);
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/%s", HTTP, filename),
                      limitData, putLimit, "Content-Type: application/octet-stream\r\n");

    tinfo("Near-limit PUT status: %d, size: %zu KB", status, putLimit / 1024);

    // Should succeed - just under 100KB body limit
    ttrue(status == 201 || status == 204);

    // Verify file size on disk
    fileSize = getPutSize(filename);
    teqz(fileSize, (ssize) putLimit);

    rFree(limitData);

    // Cleanup
    urlClose(up);
    urlFetch(up, "DELETE", url, NULL, 0, NULL);

    urlFree(up);
}

static void testPutLargeFileHandling(void)
{
    Url    *up;
    char   url[128], filename[64], *largeData;
    int    status, pid;
    size_t putSize;
    ssize  fileSize;

    up = urlAlloc(0);
    pid = getpid();

    /*
        Test handling of moderate file size - validates server can handle
        PUTs efficiently, proper buffering, etc.
     */
    putSize = 75 * 1024;  // 75KB
    largeData = rAlloc(putSize + 1);
    memset(largeData, 'X', putSize);
    largeData[putSize] = '\0';

    SFMT(filename, "largefile-%d.dat", pid);
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/%s", HTTP, filename),
                      largeData, putSize, "Content-Type: application/octet-stream\r\n");

    tinfo("Large file PUT status: %d, size: %zu KB", status, putSize / 1024);
    ttrue(status == 201 || status == 204);

    // Verify file size on disk
    fileSize = getPutSize(filename);
    teqz(fileSize, (ssize) putSize);

    rFree(largeData);

    // Cleanup
    urlClose(up);
    urlFetch(up, "DELETE", url, NULL, 0, NULL);

    urlFree(up);
}

static void testPutVariableSizes(void)
{
    Url   *up;
    char  url[128], filename[64], *data;
    int   status, pid;
    ssize fileSize;

    up = urlAlloc(0);
    pid = getpid();

    // Test various PUT sizes to ensure handling is consistent
    size_t sizes[] = {
        1024,           // 1KB
        10 * 1024,      // 10KB
        50 * 1024,      // 50KB
        90 * 1024,      // 90KB (near limit)
        0               // Sentinel
    };

    for (int i = 0; sizes[i] != 0; i++) {
        size_t size = sizes[i];
        data = rAlloc(size + 1);
        memset(data, 'V', size);
        data[size] = '\0';

        SFMT(filename, "var%d-%d.dat", i, pid);
        urlClose(up);
        status = urlFetch(up, "PUT", SFMT(url, "%s/upload/%s", HTTP, filename),
                          data, size, "Content-Type: application/octet-stream\r\n");

        tinfo("Variable size PUT %d: %zd KB, status: %d", i, size / 1024, status);
        ttrue(status == 201 || status == 204);

        // Verify file size on disk
        fileSize = getPutSize(filename);
        teqz(fileSize, (ssize) size);

        rFree(data);

        // Cleanup
        urlClose(up);
        urlFetch(up, "DELETE", url, NULL, 0, NULL);
    }
    urlFree(up);
}

static void testPutProgressive(void)
{
    Url   *up;
    char  url[128], filename[64];
    int   status, pid;
    ssize fileSize;

    up = urlAlloc(0);
    pid = getpid();

    /*
        Note: urlFetch doesn't directly support chunked PUT
        This tests that PUTs work correctly when sent
        in a single request (server may buffer internally)
     */

    // Create 80KB file for test
    size_t size = 80 * 1024;
    char   *data = rAlloc(size + 1);
    memset(data, 'P', size);
    data[size] = '\0';

    SFMT(filename, "progressive-%d.dat", pid);
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/%s", HTTP, filename),
                      data, size, "Content-Type: application/octet-stream\r\n");

    tinfo("Progressive PUT status: %d, size: %zu KB", status, size / 1024);
    ttrue(status == 201 || status == 204);

    // Verify file size on disk
    fileSize = getPutSize(filename);
    teqz(fileSize, (ssize) size);

    rFree(data);

    // Cleanup
    urlClose(up);
    urlFetch(up, "DELETE", url, NULL, 0, NULL);
    urlFree(up);
}

static void testFilenameSanitization(void)
{
    Url  *up;
    char url[256];
    int  status, pid;

    up = urlAlloc(0);
    pid = getpid();

    // Test various problematic filenames
    struct {
        cchar *filename;
        cchar *description;
    } tests[] = {
        // Path traversal attempts
        { "..%2F..%2Fetc%2Fpasswd", "URL-encoded path traversal" },
        { "....%2F....%2Fetc%2Fpasswd", "Double-dot traversal" },

        // Special characters (URL-encoded)
        { "test%00null-%d.dat", "Null byte injection" },
        { "test%3Cscript%3E-%d.dat", "HTML injection (URL-encoded)" },
        { "test%26amp%3B-%d.dat", "Entity encoding (URL-encoded)" },

        { NULL, NULL }
    };

    for (int i = 0; tests[i].filename != NULL; i++) {
        char filename[256];
        SFMT(filename, tests[i].filename, pid);

        urlClose(up);
        status = urlFetch(up, "PUT", SFMT(url, "%s/upload/%s", HTTP, filename),
                          "test", 4, "Content-Type: text/plain\r\n");

        tinfo("Filename test: %s, status: %d", tests[i].description, status);

        /*
            Server should either:
            1. Sanitize and accept (201/204)
            2. Reject as invalid (400/403)
            3. Client may reject malformed URL (status < 0)
         */
        ttrue(status == 201 || status == 204 || status == 400 || status == 403 || status < 0);
    }

    urlFree(up);
}

static void testPutCleanup(void)
{
    Url   *up;
    char  url[128], filename[64];
    int   status, pid;
    ssize fileSize;

    up = urlAlloc(0);
    pid = getpid();

    // PUT a file
    SFMT(filename, "cleanup-%d.txt", pid);
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/%s", HTTP, filename),
                      "cleanup test", 12, "Content-Type: text/plain\r\n");
    ttrue(status == 201 || status == 204);

    // Verify file size on disk
    fileSize = getPutSize(filename);
    teqz(fileSize, 12);

    // Delete the file
    urlClose(up);
    status = urlFetch(up, "DELETE", url, NULL, 0, NULL);
    ttrue(status == 200 || status == 204);

    // Verify file is gone (should get 404)
    urlClose(up);
    status = urlFetch(up, "GET", url, NULL, 0, NULL);
    ttrue(status == 404);

    urlFree(up);
}

static void testPutConcurrent(void)
{
    Url    *up1, *up2;
    char   url1[128], url2[128], filename1[64], filename2[64];
    int    status1, status2, pid;
    size_t size;
    ssize  fileSize;
    char   *data1, *data2;

    up1 = urlAlloc(0);
    up2 = urlAlloc(0);
    pid = getpid();

    // Create 60KB files for concurrent PUT
    size = 60 * 1024;
    data1 = rAlloc(size + 1);
    data2 = rAlloc(size + 1);
    memset(data1, '1', size);
    memset(data2, '2', size);
    data1[size] = '\0';
    data2[size] = '\0';

    // PUT two large files concurrently (sequential in test, but tests server capacity)
    SFMT(filename1, "concurrent1-%d.dat", pid);
    SFMT(filename2, "concurrent2-%d.dat", pid);
    status1 = urlFetch(up1, "PUT", SFMT(url1, "%s/upload/%s", HTTP, filename1),
                       data1, size, "Content-Type: application/octet-stream\r\n");
    status2 = urlFetch(up2, "PUT", SFMT(url2, "%s/upload/%s", HTTP, filename2),
                       data2, size, "Content-Type: application/octet-stream\r\n");

    tinfo("Concurrent PUTs: status1=%d, status2=%d", status1, status2);
    ttrue(status1 == 201 || status1 == 204);
    ttrue(status2 == 201 || status2 == 204);

    // Verify file sizes on disk
    fileSize = getPutSize(filename1);
    teqz(fileSize, (ssize) size);
    fileSize = getPutSize(filename2);
    teqz(fileSize, (ssize) size);

    rFree(data1);
    rFree(data2);

    // Cleanup
    urlClose(up1);
    urlFetch(up1, "DELETE", url1, NULL, 0, NULL);

    urlClose(up2);
    urlFetch(up2, "DELETE", url2, NULL, 0, NULL);

    urlFree(up1);
    urlFree(up2);
}

static void testPutEmpty(void)
{
    Url   *up;
    char  url[128], filename[64];
    int   status, pid;
    ssize fileSize;

    up = urlAlloc(0);
    pid = getpid();

    // PUT zero-byte file
    SFMT(filename, "empty-%d.dat", pid);
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/%s", HTTP, filename),
                      "", 0, "Content-Type: application/octet-stream\r\n");

    tinfo("Empty PUT status: %d", status);
    ttrue(status == 201 || status == 204);

    // Verify file size is 0
    fileSize = getPutSize(filename);
    teqz(fileSize, 0);

    // Cleanup
    urlClose(up);
    urlFetch(up, "DELETE", url, NULL, 0, NULL);

    urlFree(up);
}

static void testPutContentLength(void)
{
    Url   *up;
    char  url[128], filename[64];
    int   status, pid;
    ssize fileSize;

    up = urlAlloc(0);
    pid = getpid();

    /*
        Note: urlFetch automatically sets correct Content-Length
        This test validates that the server handles the request properly
        when Content-Length matches the actual body
     */
    char   *data = "test data with known length";
    size_t dataLen = slen(data);

    SFMT(filename, "contentlen-%d.txt", pid);
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/%s", HTTP, filename),
                      data, dataLen, "Content-Type: text/plain\r\n");

    tinfo("Content-Length match status: %d", status);
    ttrue(status == 201 || status == 204);

    // Verify file size on disk
    fileSize = getPutSize(filename);
    teqz(fileSize, (ssize) dataLen);

    // Cleanup
    urlClose(up);
    urlFetch(up, "DELETE", url, NULL, 0, NULL);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testPutLargeUnderLimit();
        testPutAtLimit();
        testPutLargeFileHandling();
        testPutVariableSizes();
        testPutProgressive();
        testFilenameSanitization();
        testPutCleanup();
        testPutConcurrent();
        testPutEmpty();
        testPutContentLength();
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
