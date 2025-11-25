/*
    streaming.c.tst - Unit tests for streaming read/write operations

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

static void testStreamingRead()
{
    Url     *up;
    char    url[128];
    char    buffer[1024];
    ssize   bytesRead, totalRead = 0;
    int     rc, status;

    up = urlAlloc(0);

    // Request a large file for streaming
    rc = urlStart(up, "GET", SFMT(url, "%s/size/10K.txt", HTTP));
    ttrue(rc == 0);

    status = urlFinalize(up);
    ttrue(status == 0);

    // Stream the response data
    while ((bytesRead = urlRead(up, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        totalRead += bytesRead;
        ttrue(bytesRead <= (ssize)(sizeof(buffer) - 1));
    }

    ttrue(totalRead > 1000);  // Should have read substantial data
    ttrue(urlGetStatus(up) == 200);

    urlFree(up);
}

static void testStreamingWrite()
{
    Url     *up;
    Json    *json;
    char    url[128];
    cchar   *chunk1 = "First chunk ";
    cchar   *chunk2 = "Second chunk ";
    cchar   *chunk3 = "Third chunk";
    int     status;

    up = urlAlloc(0);

    status = urlStart(up, "POST", SFMT(url, "%s/test/show", HTTP));
    ttrue(status == 0);

    // Write data in chunks
    ttrue(urlWrite(up, chunk1, 0) > 0);
    ttrue(urlWrite(up, chunk2, 0) > 0);
    ttrue(urlWrite(up, chunk3, 0) > 0);

    status = urlFinalize(up);
    ttrue(status == 0);

    json = urlGetJsonResponse(up);
    tmatch(jsonGet(json, 0, "body", 0), "First chunk Second chunk Third chunk");
    jsonFree(json);

    urlFree(up);
}

static void testLargeUpload()
{
    Url     *up;
    Json    *json;
    char    url[128];
    char    *largeData;
    ssize   i, nbytes;
    size_t  size;
    int     rc, status;

    // Create large data buffer
    size = 50000;  // 50KB
    largeData = rAlloc(size + 1);
    for (i = 0; i < size; i++) {
        largeData[i] = 'A' + (i % 26);
    }
    largeData[size] = '\0';

    up = urlAlloc(0);

    status = urlStart(up, "POST", SFMT(url, "%s/test/show", HTTP));
    ttrue(status == 0);

    nbytes = urlWrite(up, largeData, size);
    ttrue(nbytes == size);

    rc = urlFinalize(up);
    ttrue(rc == 0);

    json = urlGetJsonResponse(up);
    ttrue(jsonGet(json, 0, "body", 0) != 0);
    ttrue(slen(jsonGet(json, 0, "body", 0)) == size);
    jsonFree(json);

    urlFree(up);
    rFree(largeData);
}

static void testStreamingWithBufferLimit()
{
    Url     *up;
    char    url[128];
    ssize   limit = 5000;  // 5KB limit
    char    buffer[1024];
    ssize   bytesRead, totalRead = 0;
    int     rc;

    up = urlAlloc(0);
    urlSetBufLimit(up, limit);

    rc = urlStart(up, "GET", SFMT(url, "%s/size/10K.txt", HTTP));
    ttrue(rc == 0);
    rc = urlFinalize(up);
    ttrue(rc == 0);

    // The limit should not impact using urlRead
    while ((bytesRead = urlRead(up, buffer, sizeof(buffer))) > 0) {
        totalRead += bytesRead;
    }
    // Should bypass buffer limit
    ttrue(totalRead > limit);

    urlFree(up);
}

static void testZeroSizeRead()
{
    Url     *up;
    char    url[128];
    char    buffer[1024];
    ssize   bytesRead;
    int     status;

    up = urlAlloc(0);

    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), 0, 0, 0);
    ttrue(status == 200);

    // Try to read with zero buffer size
    bytesRead = urlRead(up, buffer, 0);
    ttrue(bytesRead == 0);

    // Try to read with NULL buffer
    bytesRead = urlRead(up, NULL, sizeof(buffer));
    ttrue(bytesRead < 0);

    urlFree(up);
}

static void testFormatWrite()
{
    Url     *up;
    Json    *json;
    char    url[128];
    int     status;

    up = urlAlloc(0);

    status = urlStart(up, "POST", SFMT(url, "%s/test/show", HTTP));
    ttrue(status == 0);

    // Test formatted write
    ttrue(urlWriteFmt(up, "Number: %d, String: %s", 42, "test") > 0);

    status = urlFinalize(up);
    ttrue(status == 0);

    json = urlGetJsonResponse(up);
    tmatch(jsonGet(json, 0, "body", 0), "Number: 42, String: test");
    jsonFree(json);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testStreamingRead();
        testStreamingWrite();
        testLargeUpload();
        testStreamingWithBufferLimit();
        testZeroSizeRead();
        testFormatWrite();
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