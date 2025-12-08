/*
    upload-multipart.tst.c - Multipart form-data file upload testing

    Tests multipart/form-data file uploads including single files, multiple files,
    form data combination, field parsing, and boundary handling. Validates proper
    parsing of multipart boundaries, Content-Disposition headers, and form field
    extraction.

    Coverage:
    - Single file upload via multipart/form-data
    - Multiple file uploads in single request
    - Upload with regular form fields (text data)
    - Form field extraction and validation
    - Filename parsing from Content-Disposition
    - Multipart boundary handling
    - Content-Type in multipart sections
    - Empty file uploads
    - Large form data with files

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testSingleFileUpload(void)
{
    Url  *up;
    char url[128], headers[256], *boundary, body[1024], filepath[256];
    int  status, pid;

    up = urlAlloc(0);
    pid = getpid();

    // Create multipart/form-data request with single file
    boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

    SFMT(body,
         "--%s\r\n"
         "Content-Disposition: form-data; name=\"file\"; filename=\"test-%d.txt\"\r\n"
         "Content-Type: text/plain\r\n"
         "\r\n"
         "This is test file content\r\n"
         "--%s--\r\n",
         boundary, pid, boundary);

    SFMT(headers, "Content-Type: multipart/form-data; boundary=%s\r\n", boundary);

    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", HTTP), body, slen(body), headers);

    tinfo("Single file upload status: %d", status);

    // Upload should succeed with 200 OK
    teqi(status, 200);

    // Cleanup - delete uploaded file
    SFMT(filepath, "tmp/test-%d.txt", pid);
    ttrue(rFileExists(filepath));
    unlink(filepath);

    urlFree(up);
}

static void testMultipleFileUploads(void)
{
    Url  *up;
    RBuf *buf;
    char url[128], headers[256], *boundary, filepath[256];
    int  status, pid;

    up = urlAlloc(0);
    pid = getpid();

    // Create multipart/form-data request with multiple files
    boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

    buf = rAllocBuf(2048);
    rPutToBuf(buf,
              "--%s\r\n"
              "Content-Disposition: form-data; name=\"file1\"; filename=\"file1-%d.txt\"\r\n"
              "Content-Type: text/plain\r\n"
              "\r\n"
              "First file content\r\n"
              "--%s\r\n"
              "Content-Disposition: form-data; name=\"file2\"; filename=\"file2-%d.txt\"\r\n"
              "Content-Type: text/plain\r\n"
              "\r\n"
              "Second file content\r\n"
              "--%s\r\n"
              "Content-Disposition: form-data; name=\"file3\"; filename=\"file3-%d.txt\"\r\n"
              "Content-Type: text/plain\r\n"
              "\r\n"
              "Third file content\r\n"
              "--%s--\r\n",
              boundary, pid, boundary, pid, boundary, pid, boundary);

    SFMT(headers, "Content-Type: multipart/form-data; boundary=%s\r\n", boundary);

    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", HTTP),
                      rGetBufStart(buf), rGetBufLength(buf), headers);

    // Upload should succeed with 200 OK
    teqi(status, 200);

    rFreeBuf(buf);

    // Cleanup
    SFMT(filepath, "tmp/file1-%d.txt", pid);
    ttrue(rFileExists(filepath));
    unlink(filepath);

    SFMT(filepath, "tmp/file2-%d.txt", pid);
    ttrue(rFileExists(filepath));
    unlink(filepath);

    SFMT(filepath, "tmp/file3-%d.txt", pid);
    ttrue(rFileExists(filepath));
    unlink(filepath);

    urlFree(up);
}

static void testUploadWithFormData(void)
{
    Url  *up;
    RBuf *buf;
    char url[128], headers[256], *boundary, filepath[256];
    int  status, pid;

    up = urlAlloc(0);
    pid = getpid();

    // Upload file along with regular form fields
    boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

    buf = rAllocBuf(2048);
    rPutToBuf(buf,
              "--%s\r\n"
              "Content-Disposition: form-data; name=\"username\"\r\n"
              "\r\n"
              "testuser\r\n"
              "--%s\r\n"
              "Content-Disposition: form-data; name=\"email\"\r\n"
              "\r\n"
              "test@example.com\r\n"
              "--%s\r\n"
              "Content-Disposition: form-data; name=\"file\"; filename=\"data-%d.txt\"\r\n"
              "Content-Type: text/plain\r\n"
              "\r\n"
              "File content with form data\r\n"
              "--%s\r\n"
              "Content-Disposition: form-data; name=\"description\"\r\n"
              "\r\n"
              "Test file description\r\n"
              "--%s--\r\n",
              boundary, boundary, boundary, pid, boundary, boundary);

    SFMT(headers, "Content-Type: multipart/form-data; boundary=%s\r\n", boundary);

    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", HTTP),
                      rGetBufStart(buf), rGetBufLength(buf), headers);

    // Should process both form fields and file or 405 if POST not allowed
    teqi(status, 200);

    rFreeBuf(buf);

    // Cleanup
    SFMT(filepath, "tmp/data-%d.txt", pid);
    ttrue(rFileExists(filepath));
    unlink(filepath);
    urlFree(up);
}

static void testEmptyFileUpload(void)
{
    Url  *up;
    char url[128], headers[256], *boundary, body[1024], filepath[256];
    int  status, pid;

    up = urlAlloc(0);
    pid = getpid();

    // Upload empty file (zero bytes)
    boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

    SFMT(body,
         "--%s\r\n"
         "Content-Disposition: form-data; name=\"file\"; filename=\"empty-%d.txt\"\r\n"
         "Content-Type: text/plain\r\n"
         "\r\n"
         "\r\n"
         "--%s--\r\n",
         boundary, pid, boundary);
    SFMT(headers, "Content-Type: multipart/form-data; boundary=%s\r\n", boundary);

    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", HTTP), body, slen(body), headers);

    // Empty file upload should succeed or 405 if POST not allowed
    teqi(status, 200);

    // Cleanup
    SFMT(filepath, "tmp/empty-%d.txt", pid);
    ttrue(rFileExists(filepath));
    unlink(filepath);

    urlFree(up);
}

static void testFilenameWithSpaces(void)
{
    Url  *up;
    char url[128], headers[256], *boundary, body[1024], filepath[256];
    int  status, pid;

    up = urlAlloc(0);
    pid = getpid();

    // Upload file with spaces in filename
    boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

    SFMT(body,
         "--%s\r\n"
         "Content-Disposition: form-data; name=\"file\"; filename=\"test file %d.txt\"\r\n"
         "Content-Type: text/plain\r\n"
         "\r\n"
         "Content with spaces in filename\r\n"
         "--%s--\r\n",
         boundary, pid, boundary);
    SFMT(headers, "Content-Type: multipart/form-data; boundary=%s\r\n", boundary);

    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", HTTP), body, slen(body), headers);

    // Should handle filename with spaces or 405 if POST not allowed
    teqi(status, 200);

    // Cleanup
    SFMT(filepath, "tmp/test file %d.txt", pid);
    ttrue(rFileExists(filepath));
    unlink(filepath);

    urlFree(up);
}

static void testBinaryFileUpload(void)
{
    Url  *up;
    RBuf *buf;
    char url[128], headers[256], *boundary, filepath[256];
    char binaryData[256];
    int  status, pid, i;

    up = urlAlloc(0);
    pid = getpid();

    boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    SFMT(headers, "Content-Type: multipart/form-data; boundary=%s\r\n", boundary);

    // Create binary data (non-text bytes)
    for (i = 0; i < (int) sizeof(binaryData); i++) {
        binaryData[i] = (char) i;
    }

    // Build multipart body with binary data using RBuf
    buf = rAllocBuf(1024);
    rPutToBuf(buf,
              "--%s\r\n"
              "Content-Disposition: form-data; name=\"file\"; filename=\"binary-%d.dat\"\r\n"
              "Content-Type: application/octet-stream\r\n"
              "\r\n",
              boundary, pid);
    rPutBlockToBuf(buf, binaryData, sizeof(binaryData));
    rPutToBuf(buf, "\r\n--%s--\r\n", boundary);

    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", HTTP),
                      rGetBufStart(buf), rGetBufLength(buf), headers);

    // Binary upload should succeed or 405 if POST not allowed
    teqi(status, 200);

    rFreeBuf(buf);

    // Cleanup
    SFMT(filepath, "tmp/binary-%d.dat", pid);
    ttrue(rFileExists(filepath));
    unlink(filepath);

    urlFree(up);
}

static void testLargeFormData(void)
{
    Url  *up;
    RBuf *buf;
    char url[128], headers[256], *boundary, filepath[256];
    int  status, pid, i;

    up = urlAlloc(0);
    pid = getpid();

    boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

    // Create large form data (10KB of form fields + small file) using RBuf
    buf = rAllocBuf(10 * 1024);

    // Add many form fields
    for (i = 0; i < 100; i++) {
        rPutToBuf(buf,
                  "--%s\r\n"
                  "Content-Disposition: form-data; name=\"field%d\"\r\n"
                  "\r\n"
                  "This is field %d with some data to make it larger than a few bytes\r\n",
                  boundary, i, i);
    }

    // Add file at the end
    rPutToBuf(buf,
              "--%s\r\n"
              "Content-Disposition: form-data; name=\"file\"; filename=\"large-form-%d.txt\"\r\n"
              "Content-Type: text/plain\r\n"
              "\r\n"
              "File in large form\r\n"
              "--%s--\r\n",
              boundary, pid, boundary);

    SFMT(headers, "Content-Type: multipart/form-data; boundary=%s\r\n", boundary);

    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", HTTP),
                      rGetBufStart(buf), rGetBufLength(buf), headers);
    teqi(status, 200);

    rFreeBuf(buf);

    // Cleanup
    SFMT(filepath, "tmp/large-form-%d.txt", pid);
    ttrue(rFileExists(filepath));
    unlink(filepath);

    urlFree(up);
}

static void testMissingFilename(void)
{
    Url  *up;
    char url[128], headers[256], *boundary, body[1024];
    int  status;

    up = urlAlloc(0);

    // Upload without filename attribute
    boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

    SFMT(body,
         "--%s\r\n"
         "Content-Disposition: form-data; name=\"file\"\r\n"
         "Content-Type: text/plain\r\n"
         "\r\n"
         "Content without filename\r\n"
         "--%s--\r\n",
         boundary, boundary);
    SFMT(headers, "Content-Type: multipart/form-data; boundary=%s\r\n", boundary);

    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", HTTP), body, slen(body), headers);
    teqi(status, 200);
    urlFree(up);
}

static void testInvalidBoundary(void)
{
    Url  *up;
    char url[128], headers[256], *boundary, body[1024];
    int  status;

    up = urlAlloc(0);

    // Send data with no valid parts but proper termination
    boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

    // Send invalid multipart data (missing Content-Disposition) but with proper boundary termination
    SFMT(body,
         "--%s\r\n"
         "Invalid-Header: this should cause parsing error\r\n"
         "\r\n"
         "Test content\r\n"
         "--%s--\r\n",
         boundary, boundary);
    SFMT(headers, "Content-Type: multipart/form-data; boundary=%s\r\n", boundary);

    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", HTTP), body, slen(body), headers);

    // Server correctly rejects invalid multipart data (missing Content-Disposition) with 400
    teqi(status, 400);
    urlFree(up);
}

static void testContentTypeVariations(void)
{
    Url  *up;
    char url[128], headers[256], *boundary, body[1024], filepath[256];
    int  status, pid;

    up = urlAlloc(0);
    pid = getpid();

    // Test various Content-Type values in multipart sections
    boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

    SFMT(body,
         "--%s\r\n"
         "Content-Disposition: form-data; name=\"html\"; filename=\"test-%d.html\"\r\n"
         "Content-Type: text/html\r\n"
         "\r\n"
         "<html><body>Test</body></html>\r\n"
         "--%s\r\n"
         "Content-Disposition: form-data; name=\"json\"; filename=\"data-%d.json\"\r\n"
         "Content-Type: application/json\r\n"
         "\r\n"
         "{\"test\": \"data\"}\r\n"
         "--%s--\r\n",
         boundary, pid, boundary, pid, boundary);
    SFMT(headers, "Content-Type: multipart/form-data; boundary=%s\r\n", boundary);

    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", HTTP), body, slen(body), headers);

    // Should handle different Content-Types or 405 if POST not allowed
    teqi(status, 200);

    // Cleanup
    SFMT(filepath, "tmp/test-%d.html", pid);
    ttrue(rFileExists(filepath));
    unlink(filepath);

    SFMT(filepath, "tmp/data-%d.json", pid);
    ttrue(rFileExists(filepath));
    unlink(filepath);
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testSingleFileUpload();
        testMultipleFileUploads();
        testUploadWithFormData();
        testEmptyFileUpload();
        testFilenameWithSpaces();
        testBinaryFileUpload();
        testLargeFormData();
        testMissingFilename();
        testInvalidBoundary();
        testContentTypeVariations();
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
