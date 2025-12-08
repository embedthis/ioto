/*
    upload.c.tst - Unit tests for file upload functionality

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

static void testWriteFile()
{
    Url     *up;
    Json    *json;
    char    url[128];
    char    *tempFile;
    int     status;
    cchar   *testContent = "This is test file content\nLine 2\nLine 3";

    // Create temporary test file
    tempFile = rGetTempFile(NULL, "url-test");
    if (!tempFile) {
        tfail("Cannot create temp file name");
        return;
    }

    if (rWriteFile(tempFile, testContent, slen(testContent), 0644) < 0) {
        tfail("Cannot create test file");
        rFree(tempFile);
        return;
    }

    up = urlAlloc(0);

    status = urlStart(up, "POST", SFMT(url, "%s/test/show", HTTP));
    ttrue(status == 0);

    // Write file content
    ssize written = urlWriteFile(up, tempFile);
    ttrue(written >= 0);

    status = urlFinalize(up);
    ttrue(status == 0);

    json = urlGetJsonResponse(up);
    tmatch(jsonGet(json, 0, "body", 0), testContent);
    jsonFree(json);

    urlFree(up);

    // Clean up
    unlink(tempFile);
    rFree(tempFile);
}

static void testUploadMultipleFiles()
{
    Url     *up;
    char    url[128];
    char    *file1, *file2;
    RList   *files;
    RHash   *forms;
    int     rc;

    // Create test files
    file1 = rGetTempFile(NULL, "url-upload1");
    file2 = rGetTempFile(NULL, "url-upload2");
    if (!file1 || !file2) {
        tfail("Cannot create temp file names");
        rFree(file1);
        rFree(file2);
        return;
    }

    if (rWriteFile(file1, "File 1 content", 14, 0644) < 0) {
        tfail("Cannot create test file 1");
        rFree(file1);
        rFree(file2);
        return;
    }

    if (rWriteFile(file2, "File 2 content", 14, 0644) < 0) {
        tfail("Cannot create test file 2");
        rFree(file1);
        rFree(file2);
        return;
    }

    // Create file list and form data
    files = rAllocList(0, 0);
    rAddItem(files, file1);
    rAddItem(files, file2);

    forms = rAllocHash(0, 0);
    rAddName(forms, "description", "Test upload", 0);
    rAddName(forms, "category", "testing", 0);

    up = urlAlloc(0);
    urlStart(up, "POST", SFMT(url, "%s/test/upload", HTTP));
    rc = urlUpload(up, files, forms, NULL);
    ttrue(rc == 0);

    urlFree(up);
    rFree(files);
    rFree(forms);

    // Clean up
    unlink(file1);
    unlink(file2);
    rFree(file1);
    rFree(file2);
}

static void testWriteNonExistentFile()
{
    Url     *up;
    char    url[128];
    int     status;

    up = urlAlloc(0);

    status = urlStart(up, "POST", SFMT(url, "%s/test/show", HTTP));
    ttrue(status == 0);

    // Try to write non-existent file
    ssize result = urlWriteFile(up, "/non/existent/file.txt");
    ttrue(result < 0);  // Should fail

    urlFree(up);
}

static void testUploadEdgeCases()
{
    Url     *up;
    char    url[128];
    int     status;

    up = urlAlloc(0);

    // Test upload with NULL file list
    status = urlUpload(up, NULL, NULL, SFMT(url, "%s/test/upload", HTTP), NULL);
    ttrue(status < 0);  // Should fail gracefully

    // Test upload with empty file list
    RList *emptyFiles = rAllocList(0, 0);
    status = urlUpload(up, emptyFiles, NULL, SFMT(url, "%s/test/upload", HTTP), NULL);
    // Should handle empty list gracefully

    rFree(emptyFiles);
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testWriteFile();
        testUploadMultipleFiles();
        testWriteNonExistentFile();
        testUploadEdgeCases();
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