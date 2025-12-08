/*
    compressed.tst.c - Test pre-compressed content serving
 */
#include "test.h"

static char *HTTP;
static char *HTTPS;

static void testPrecompressedBrotli(void)
{
    Url   *up;
    char  url[128];
    cchar *contentEncoding;
    int   status;

    //  Request with brotli support
    up = urlAlloc(0);

    status = urlFetch(up, "GET", SFMT(url, "%s/compressed/app.js", HTTP), NULL, 0,
                      "Accept-Encoding: br, gzip\r\n");
    if (status != 200) {
        rError("test", "GET /compressed/app.js returned %d, expected 200", status);
    }
    teqi(status, 200);

    contentEncoding = urlGetHeader(up, "Content-Encoding");
    if (!contentEncoding) {
        rError("test", "Content-Encoding header is NULL");
    }
    tnotnull(contentEncoding);
    if (contentEncoding) {
        if (!smatch(contentEncoding, "br")) {
            rError("test", "Content-Encoding is '%s', expected 'br'", contentEncoding);
        }
        tmatch(contentEncoding, "br");
    }
    cchar *vary = urlGetHeader(up, "Vary");
    //  Vary header may contain multiple values (e.g., "Origin, Accept-Encoding")
    //  Check that it contains "Accept-Encoding"
    if (!vary || !scontains(vary, "Accept-Encoding")) {
        rError("test", "Vary header is '%s', expected to contain 'Accept-Encoding'", vary ? vary : "NULL");
    }
    tnotnull(vary);
    tnotnull(scontains(vary, "Accept-Encoding"));
    tmatch(urlGetHeader(up, "Content-Type"), "application/x-javascript");

    urlFree(up);
}

static void testPrecompressedGzip(void)
{
    Url   *up;
    char  url[128];
    cchar *vary;

    //  Request with only gzip support
    up = urlAlloc(0);

    teqi(urlFetch(up, "GET", SFMT(url, "%s/compressed/data.json", HTTP), NULL, 0,
                  "Accept-Encoding: gzip, deflate\r\n"), 200);
    tmatch(urlGetHeader(up, "Content-Encoding"), "gzip");

    vary = urlGetHeader(up, "Vary");
    tnotnull(vary);
    tnotnull(scontains(vary, "Accept-Encoding"));

    urlFree(up);
}

static void testNoCompression(void)
{
    Url  *up;
    char url[128];

    //  Request without compression support
    up = urlAlloc(0);

    teqi(urlFetch(up, "GET", SFMT(url, "%s/compressed/app.js", HTTP), NULL, 0, NULL), 200);
    tnull(urlGetHeader(up, "Content-Encoding"));

    urlFree(up);
}

static void testPrecompressedFallback(void)
{
    Url  *up;
    char url[128];

    //  Request file that doesn't have compressed version
    //  (assuming uncompressed.txt exists but .gz/.br don't)
    up = urlAlloc(0);

    teqi(urlFetch(up, "GET", SFMT(url, "%s/compressed/uncompressed.txt", HTTP), NULL, 0,
                  "Accept-Encoding: br, gzip\r\n"), 200);
    tnull(urlGetHeader(up, "Content-Encoding"));

    urlFree(up);
}

static void testPrecompressedConditional(void)
{
    Url   *up;
    char  url[128], *headers;
    cchar *lastModified;
    int   status;

    //  First request to get Last-Modified
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/compressed/app.js", HTTP), NULL, 0,
                      "Accept-Encoding: gzip\r\n");
    teqi(status, 200);

    lastModified = urlGetHeader(up, "Last-Modified");
    tnotnull(lastModified);

    //  Conditional request with If-Modified-Since
    urlClose(up);
    headers = sfmt("Accept-Encoding: gzip\r\nIf-Modified-Since: %s\r\n", lastModified);
    status = urlFetch(up, "GET", SFMT(url, "%s/compressed/app.js", HTTP), NULL, 0, headers);
    teqi(status, 304);
    rFree(headers);

    urlFree(up);
}

static void testPrecompressedDisabled(void)
{
    Url  *up;
    char url[128];

    //  Request to route without compressed flag
    //  (assuming /trace route doesn't have compressed: true)
    up = urlAlloc(0);

    teqi(urlFetch(up, "GET", SFMT(url, "%s/trace/index.html", HTTP), NULL, 0,
                  "Accept-Encoding: gzip\r\n"), 200);
    tnull(urlGetHeader(up, "Content-Encoding"));

    urlFree(up);
}

static void testHeadRequest(void)
{
    Url   *up;
    char  url[128];
    cchar *vary;

    //  HEAD request with compression support
    up = urlAlloc(0);

    teqi(urlFetch(up, "HEAD", SFMT(url, "%s/compressed/app.js", HTTP), NULL, 0,
                  "Accept-Encoding: br, gzip\r\n"), 200);
    tnotnull(urlGetHeader(up, "Content-Encoding"));

    vary = urlGetHeader(up, "Vary");
    tnotnull(vary);
    tnotnull(scontains(vary, "Accept-Encoding"));

    urlFree(up);
}

static void testQualityValues(void)
{
    Url  *up;
    char url[128];

    //  Test Accept-Encoding with quality values (q-values)
    //  Higher q-value should be preferred
    up = urlAlloc(0);

    teqi(urlFetch(up, "GET", SFMT(url, "%s/compressed/app.js", HTTP), NULL, 0,
                  "Accept-Encoding: gzip;q=0.8, br;q=1.0\r\n"), 200);
    tmatch(urlGetHeader(up, "Content-Encoding"), "br");

    urlFree(up);
}

static void testMimeType(void)
{
    Url   *up;
    char  url[128];
    cchar *contentType;

    //  MIME type should be based on original file extension, not .gz/.br
    up = urlAlloc(0);

    teqi(urlFetch(up, "GET", SFMT(url, "%s/compressed/app.js", HTTP), NULL, 0,
                  "Accept-Encoding: gzip\r\n"), 200);
    contentType = urlGetHeader(up, "Content-Type");
    tnotnull(contentType);
    tnotnull(scontains(contentType, "javascript"));

    urlFree(up);
}

static void testETag(void)
{
    Url   *up;
    char  url[128];
    cchar *etag;

    //  ETag should be present on compressed responses
    up = urlAlloc(0);

    teqi(urlFetch(up, "GET", SFMT(url, "%s/compressed/app.js", HTTP), NULL, 0,
                  "Accept-Encoding: gzip\r\n"), 200);
    etag = urlGetHeader(up, "ETag");
    tnotnull(etag);
    ttrue(sstarts(etag, "\""));

    urlFree(up);
}

static void testRangeWithCompression(void)
{
    Url   *up;
    char  url[128];
    cchar *contentEncoding, *acceptRanges;
    int   status;

    //  Range requests should work with compressed content
    up = urlAlloc(0);

    status = urlFetch(up, "GET", SFMT(url, "%s/compressed/app.js", HTTP), NULL, 0,
                      "Accept-Encoding: gzip\r\nRange: bytes=0-99\r\n");
    ttrue(status == 206 || status == 200);

    contentEncoding = urlGetHeader(up, "Content-Encoding");
    tnotnull(contentEncoding);
    tmatch(contentEncoding, "gzip");

    acceptRanges = urlGetHeader(up, "Accept-Ranges");
    tnotnull(acceptRanges);
    tmatch(acceptRanges, "bytes");

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testPrecompressedBrotli();
        testPrecompressedGzip();
        testNoCompression();
        testPrecompressedFallback();
        testPrecompressedConditional();
        testPrecompressedDisabled();
        testHeadRequest();
        testQualityValues();
        testMimeType();
        testETag();
        testRangeWithCompression();
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
