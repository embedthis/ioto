/*
    upload-large.tst.c - Multipart uploads whose body spans several socket reads

    A small upload arrives in one buffer, so the whole terminating boundary is always present and
    the handler can write everything up to it. A file larger than the read buffer does not: each
    read may end part way through what could be the start of the boundary, so the handler has to
    hold back the trailing bytes that might be a partial boundary and write them only once the
    next read proves they are not. That hold-back is getUploadDataLength, and only a multi-read
    upload reaches it.

    Payloads containing "-" are used deliberately: the boundary begins with "-", so they make the
    hold-back search match repeatedly inside ordinary file data.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

#define BOUNDARY  "----WebKitFormBoundaryUploadLargeTest"
#define SMALL_UPLOAD "http://localhost:4876"

//  Comfortably more than ME_BUFSIZE (4096) so the body needs many reads
#define PAYLOAD_SIZE 65536

/************************************ Code ************************************/

/*
    Build a payload of the requested size. "fill" selects the byte pattern; when "dashes" is set,
    every 32nd byte is a "-" so the partial-boundary search keeps matching inside the data.
 */
static char *makePayload(size_t size, bool dashes)
{
    char   *payload;
    size_t i;

    payload = rAlloc(size + 1);
    for (i = 0; i < size; i++) {
        if (dashes && (i % 32) == 0) {
            payload[i] = '-';
        } else {
            payload[i] = (char) ('a' + (i % 26));
        }
    }
    payload[size] = '\0';
    return payload;
}

/*
    POST a single-part multipart upload carrying the given payload. Returns the status.
 */
static int uploadPayload(cchar *clientFilename, cchar *payload, size_t size)
{
    Url   *up;
    RBuf  *body;
    char  url[160], headers[256];
    char  *data;
    size_t len;
    int   status;

    body = rAllocBuf(size + 512);
    rPutToBuf(body,
              "--%s\r\n"
              "Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
              "Content-Type: application/octet-stream\r\n"
              "\r\n",
              BOUNDARY, clientFilename);
    rPutBlockToBuf(body, payload, size);
    rPutToBuf(body, "\r\n--%s--\r\n", BOUNDARY);

    len = rGetBufLength(body);
    data = rBufToStringAndFree(body);

    SFMT(headers, "Content-Type: multipart/form-data; boundary=%s\r\n", BOUNDARY);
    up = urlAlloc(0);
    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", HTTP), data, len, headers);
    urlFree(up);
    rFree(data);
    return status;
}

/*
    Upload a payload and confirm the stored file is byte-for-byte what was sent. The test action
    copies each upload to ./tmp/<clientFilename>.
 */
static void checkRoundTrip(cchar *name, size_t size, bool dashes)
{
    char   *payload, *stored, path[256];
    size_t storedLen;
    int    status;

    payload = makePayload(size, dashes);
    status = uploadPayload(name, payload, size);
    teqi(status, 200);

    if (status == 200) {
        SFMT(path, "tmp/%s", name);
        //  The upload must be stored whole - a truncated hold-back would show as a short file
        teqz((size_t) rGetFileSize(path), size);

        stored = rReadFile(path, &storedLen);
        tnotnull(stored);
        if (stored) {
            teqz(storedLen, size);
            //  Content must be unchanged - a mishandled partial boundary corrupts the interior
            teqi(memcmp(stored, payload, size), 0);
            rFree(stored);
        }
        unlink(path);
    }
    rFree(payload);
}

/*
    A payload with no "-" in it: the hold-back search finds no candidate boundary and falls
    through to its "keep everything but the trailing CRLF" case.
 */
static void testLargeUploadNoDashes(void)
{
    char name[64];

    SFMT(name, "large-plain-%d.bin", getpid());
    checkRoundTrip(name, PAYLOAD_SIZE, 0);
}

/*
    A payload sprinkled with "-": the search matches inside ordinary data on almost every read,
    so the hold-back is exercised repeatedly.
 */
static void testLargeUploadWithDashes(void)
{
    char name[64];

    SFMT(name, "large-dashes-%d.bin", getpid());
    checkRoundTrip(name, PAYLOAD_SIZE, 1);
}

/*
    A payload that ends in a run of "-" places a boundary-like sequence right at the tail, which
    is where a hold-back that is off by a byte would truncate or corrupt.
 */
static void testLargeUploadTrailingDashes(void)
{
    char   *payload, name[64];
    size_t i;
    int    status;
    char   path[256];

    SFMT(name, "large-tail-%d.bin", getpid());
    payload = makePayload(PAYLOAD_SIZE, 0);
    for (i = PAYLOAD_SIZE - 40; i < PAYLOAD_SIZE; i++) {
        payload[i] = '-';
    }
    status = uploadPayload(name, payload, PAYLOAD_SIZE);
    teqi(status, 200);
    if (status == 200) {
        SFMT(path, "tmp/%s", name);
        teqz((size_t) rGetFileSize(path), (size_t) PAYLOAD_SIZE);
        unlink(path);
    }
    rFree(payload);
}

/*
    A payload just over the read buffer, so exactly one hold-back happens.
 */
static void testJustOverBufferSize(void)
{
    char name[64];

    SFMT(name, "large-edge-%d.bin", getpid());
    checkRoundTrip(name, ME_BUFSIZE + 64, 0);
}

/*
    A chunked multipart body has no aggregate Content-Length to reject from the headers. The upload
    filter discovers the overrun while writing the part, and must report a payload error rather than
    claiming that the request URI is too long.
 */
static void testUploadPartOverLimit(void)
{
    WebHost *host;
    RBuf    *body;
    cchar   *segments[12];
    char    *payload, *pieces[3], sizeLine[3][32], response[4096];
    size_t  end, len, offset, size;
    int     count, i, status;

    host = startTestHost("{ web: { documents: './site', listen: ['http://:4876'], "
                         "limits: { body: '8K', upload: '1K' }, "
                         "routes: [ { match: '/test/', handler: 'action' }, {} ] } }");
    if (!host) {
        return;
    }
    payload = makePayload(2048, 0);
    body = rAllocBuf(2560);
    rPutToBuf(body,
              "--%s\r\n"
              "Content-Disposition: form-data; name=\"file\"; filename=\"over.bin\"\r\n"
              "Content-Type: application/octet-stream\r\n\r\n",
              BOUNDARY);
    rPutBlockToBuf(body, payload, 2048);
    rPutToBuf(body, "\r\n--%s--\r\n", BOUNDARY);
    len = rGetBufLength(body);

    count = 0;
    segments[count++] = "POST /test/upload/ HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n"
                        "Content-Type: multipart/form-data; boundary=" BOUNDARY "\r\n\r\n";
    offset = 0;
    for (i = 0; i < 3; i++) {
        end = min(len, offset + 900);
        size = end - offset;
        pieces[i] = snclone(&rBufToString(body)[offset], size);
        SFMT(sizeLine[i], "%zx\r\n", size);
        segments[count++] = sizeLine[i];
        segments[count++] = pieces[i];
        segments[count++] = "\r\n";
        offset = end;
    }
    segments[count++] = "0\r\n\r\n";
    status = rawRequest(SMALL_UPLOAD, segments, count, 0, 1, response, sizeof(response));
    teqi(status, 413, "An oversized upload part must return Payload Too Large");

    for (i = 0; i < 3; i++) {
        rFree(pieces[i]);
    }
    rFreeBuf(body);
    rFree(payload);
    stopTestHost(host);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testUploadPartOverLimit();
        testJustOverBufferSize();
        testLargeUploadNoDashes();
        testLargeUploadWithDashes();
        testLargeUploadTrailingDashes();
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
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
