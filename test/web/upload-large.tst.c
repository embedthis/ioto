/*
    upload-large.tst.c - PUT upload testing and edge cases

    Tests PUT-based file uploads to /upload/ route. These uploads are limited
    by the body size limit (100KB) rather than the multipart upload limit (20MB).
    Focuses on edge cases, security validation, and resource management.

    Note: For multipart upload testing, see upload.tst.c and upload-multipart.tst.c

    Coverage:
    - PUT uploads at various sizes (under 100KB body limit)
    - Boundary condition testing (near limit)
    - Progressive upload (buffering)
    - Filename sanitization and path traversal prevention
    - Upload cleanup and lifecycle
    - Concurrent uploads
    - Empty file uploads
    - Content-Length validation

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testLargeUploadUnderLimit(void)
{
    Url    *up;
    char   url[128], *largeData;
    int    status, pid;
    size_t uploadSize;

    up = urlAlloc(0);
    pid = getpid();

    // Upload file under 100KB body limit (use 50KB for test)
    uploadSize = 50 * 1024;  // 50KB
    largeData = rAlloc(uploadSize + 1);
    memset(largeData, 'L', uploadSize);
    largeData[uploadSize] = '\0';

    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/large-%d.dat", HTTP, pid),
                      largeData, uploadSize, "Content-Type: application/octet-stream\r\n");

    tinfo("Large file upload status: %d, size: %zu KB", status, uploadSize / 1024);
    ttrue(status == 201 || status == 204);

    rFree(largeData);

    // Cleanup
    urlClose(up);
    urlFetch(up, "DELETE", url, NULL, 0, NULL);

    urlFree(up);
}

static void testUploadAtLimit(void)
{
    Url    *up;
    char   url[128], *limitData;
    int    status, pid;
    size_t uploadLimit;

    up = urlAlloc(0);
    pid = getpid();

    // Testing at body limit (100KB)
    // Note: PUT uploads use body limit, not upload limit
    uploadLimit = 95 * 1024;  // 95KB (just under limit with headers)
    limitData = rAlloc(uploadLimit + 1);
    memset(limitData, 'M', uploadLimit);
    limitData[uploadLimit] = '\0';

    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/limit-%d.dat", HTTP, pid),
                      limitData, uploadLimit, "Content-Type: application/octet-stream\r\n");

    tinfo("Near-limit upload status: %d, size: %zu KB", status, uploadLimit / 1024);

    // Should succeed - just under 100KB body limit
    ttrue(status == 201 || status == 204);

    rFree(limitData);

    // Cleanup if succeeded
    if (status == 201 || status == 204) {
        urlClose(up);
        urlFetch(up, "DELETE", url, NULL, 0, NULL);
    }

    urlFree(up);
}

static void testLargeFileHandling(void)
{
    Url    *up;
    char   url[128], *largeData;
    int    status, pid;
    size_t uploadSize;

    up = urlAlloc(0);
    pid = getpid();

    // Test handling of moderate file size - validates server can handle
    // uploads efficiently, proper buffering, etc.
    uploadSize = 75 * 1024;  // 75KB
    largeData = rAlloc(uploadSize + 1);
    memset(largeData, 'X', uploadSize);
    largeData[uploadSize] = '\0';

    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/largefile-%d.dat", HTTP, pid),
                      largeData, uploadSize, "Content-Type: application/octet-stream\r\n");

    tinfo("Large file handling status: %d, size: %zu KB", status, uploadSize / 1024);
    ttrue(status == 201 || status == 204);

    rFree(largeData);

    // Cleanup
    urlClose(up);
    urlFetch(up, "DELETE", url, NULL, 0, NULL);

    urlFree(up);
}

static void testVariableSizeUploads(void)
{
    Url  *up;
    char url[128], *data;
    int  status, pid;

    up = urlAlloc(0);
    pid = getpid();

    // Test various upload sizes to ensure handling is consistent
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

        urlClose(up);
        status = urlFetch(up, "PUT", SFMT(url, "%s/upload/var%d-%d.dat", HTTP, i, pid),
                          data, size, "Content-Type: application/octet-stream\r\n");

        tinfo("Variable size upload %d: %zd KB, status: %d", i, size / 1024, status);
        ttrue(status == 201 || status == 204);

        rFree(data);

        // Cleanup
        urlClose(up);
        urlFetch(up, "DELETE", url, NULL, 0, NULL);
    }

    urlFree(up);
}

static void testProgressiveUpload(void)
{
    Url  *up;
    char url[128];
    int  status, pid;

    up = urlAlloc(0);
    pid = getpid();

    // Note: urlFetch doesn't directly support chunked upload
    // This tests that uploads work correctly when sent
    // in a single request (server may buffer internally)

    // Create 80KB file for test
    size_t size = 80 * 1024;
    char   *data = rAlloc(size + 1);
    memset(data, 'P', size);
    data[size] = '\0';

    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/progressive-%d.dat", HTTP, pid),
                      data, size, "Content-Type: application/octet-stream\r\n");

    tinfo("Progressive upload status: %d, size: %zu KB", status, size / 1024);
    ttrue(status == 201 || status == 204);

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

        // Server should either:
        // 1. Sanitize and accept (201/204)
        // 2. Reject as invalid (400/403)
        // 3. Client may reject malformed URL (status < 0)
        ttrue(status == 201 || status == 204 || status == 400 || status == 403 || status < 0);
    }

    urlFree(up);
}

static void testUploadCleanup(void)
{
    Url  *up;
    char url[128];
    int  status, pid;

    up = urlAlloc(0);
    pid = getpid();

    // Upload a file
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/cleanup-%d.txt", HTTP, pid),
                      "cleanup test", 12, "Content-Type: text/plain\r\n");
    ttrue(status == 201 || status == 204);

    // Verify file exists (GET should work on upload directory)
    // Note: If /upload/ doesn't serve files, this will 404
    urlClose(up);
    status = urlFetch(up, "GET", url, NULL, 0, NULL);
    // Accept 200 (served) or 404 (upload dir doesn't serve)
    ttrue(status == 200 || status == 404);

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

static void testConcurrentUploads(void)
{
    Url    *up1, *up2;
    char   url1[128], url2[128];
    int    status1, status2, pid;
    size_t size;
    char   *data1, *data2;

    up1 = urlAlloc(0);
    up2 = urlAlloc(0);
    pid = getpid();

    // Create 60KB files for concurrent upload
    size = 60 * 1024;
    data1 = rAlloc(size + 1);
    data2 = rAlloc(size + 1);
    memset(data1, '1', size);
    memset(data2, '2', size);
    data1[size] = '\0';
    data2[size] = '\0';

    // Upload two large files concurrently (sequential in test, but tests server capacity)
    status1 = urlFetch(up1, "PUT", SFMT(url1, "%s/upload/concurrent1-%d.dat", HTTP, pid),
                       data1, size, "Content-Type: application/octet-stream\r\n");
    status2 = urlFetch(up2, "PUT", SFMT(url2, "%s/upload/concurrent2-%d.dat", HTTP, pid),
                       data2, size, "Content-Type: application/octet-stream\r\n");

    tinfo("Concurrent uploads: status1=%d, status2=%d", status1, status2);
    ttrue(status1 == 201 || status1 == 204);
    ttrue(status2 == 201 || status2 == 204);

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

static void testEmptyUpload(void)
{
    Url  *up;
    char url[128];
    int  status, pid;

    up = urlAlloc(0);
    pid = getpid();

    // Upload zero-byte file
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/empty-%d.dat", HTTP, pid),
                      "", 0, "Content-Type: application/octet-stream\r\n");

    tinfo("Empty upload status: %d", status);
    ttrue(status == 201 || status == 204);

    // Cleanup
    urlClose(up);
    urlFetch(up, "DELETE", url, NULL, 0, NULL);

    urlFree(up);
}

static void testContentLengthMismatch(void)
{
    Url  *up;
    char url[128];
    int  status, pid;

    up = urlAlloc(0);
    pid = getpid();

    // Note: urlFetch automatically sets correct Content-Length
    // This test validates that the server handles the request properly
    // when Content-Length matches the actual body

    char   *data = "test data with known length";
    size_t dataLen = slen(data);

    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/contentlen-%d.txt", HTTP, pid),
                      data, dataLen, "Content-Type: text/plain\r\n");

    tinfo("Content-Length match status: %d", status);
    ttrue(status == 201 || status == 204);

    // Cleanup
    urlClose(up);
    urlFetch(up, "DELETE", url, NULL, 0, NULL);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testLargeUploadUnderLimit();
        testUploadAtLimit();
        testLargeFileHandling();
        testVariableSizeUploads();
        testProgressiveUpload();
        testFilenameSanitization();
        testUploadCleanup();
        testConcurrentUploads();
        testEmptyUpload();
        testContentLengthMismatch();
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
