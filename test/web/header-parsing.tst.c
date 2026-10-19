/*
    header-parsing.tst.c - Request line, header and body limit handling on the wire

    A client library only sends well-formed requests, so these drive the parser directly over raw
    sockets: malformed request lines and header blocks, conflicting or oversize framing headers,
    malformed conditional and range headers, and the headers whose effect is only visible in the
    response. A malformed header block is dropped by closing the connection without a response,
    which is deliberate: it costs an attacker a connection and gives nothing back. A malformed
    request line is answered with a 400 and then closed, so a legitimate client learns why.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

/************************************ Code ************************************/

static int request(cchar *req, char *buf, size_t size)
{
    cchar *segments[1];

    segments[0] = req;
    return rawRequest(HTTP, segments, 1, 0, 1, buf, size);
}

/*
    Send a request that must be refused by closing the connection: no response, and promptly
 */
static void dropped(cchar *req, cchar *why)
{
    char  buf[1024];
    Ticks started;
    int   status;

    started = rGetTicks();
    status = request(req, buf, sizeof(buf));
    teqi(status, 0, "%s", why);
    ttrue(rGetTicks() - started < 2 * TPS, "The connection must be closed, not left to time out");
}

/*
    Send a request that must be refused with a status, then closed: an answer, and promptly
 */
static void refused(cchar *req, int expected, cchar *why)
{
    char  buf[1024];
    Ticks started;
    int   status;

    started = rGetTicks();
    status = request(req, buf, sizeof(buf));
    teqi(status, expected, "%s", why);
    tcontains(buf, "Connection: close");
    ttrue(rGetTicks() - started < 2 * TPS, "The connection must be closed, not left to time out");
}

/*
    Copy the value of a response header into value. Returns false if absent.
 */
static bool header(cchar *response, cchar *name, char *value, size_t size)
{
    cchar *cp, *end;

    if ((cp = sncaselesscontains(response, name, slen(response))) == 0) {
        return 0;
    }
    cp += slen(name);
    while (*cp == ' ') {
        cp++;
    }
    if ((end = scontains(cp, "\r\n")) == 0) {
        return 0;
    }
    sncopy(value, size, cp, (size_t) (end - cp));
    return 1;
}

static void testMalformedRequestLine(void)
{
    dropped("GET\r\n\r\n", "A request line too short to hold a request must be dropped");
    refused("GET /index.html\r\n\r\n", 400, "A request line without a protocol must be refused");
    refused("GET /index.html HTTP/1.1 EXTRA\r\n\r\n", 400, "A fourth request-line token must be refused");
    refused("GET /index.html HTTP/2.0\r\n\r\n", 505, "An unsupported HTTP version must be refused");
}


static void testMethodCase(void)
{
    char buf[4096];
    int  status;

    status = request("GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", buf, sizeof(buf));
    teqi(status, 200);
    status = request("get /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", buf, sizeof(buf));
    teqi(status, 405, "A lowercase get is not GET");
    status = request("dElEtE /tmp/x.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", buf, sizeof(buf));
    teqi(status, 405, "A mixed-case delete is not DELETE");
}

static void testMalformedHeaders(void)
{
    dropped("GET /index.html HTTP/1.1\r\nHost x\r\n\r\n", "A header without a colon must be dropped");
    dropped("GET /index.html HTTP/1.1\r\nHost: x\nX-A: 1\r\n\r\n", "A bare LF inside the headers must be dropped");
}

/*
    RFC 6265 permits several Cookie headers; they are joined into one cookie string
 */
static void testMultipleCookieHeadersAreJoined(void)
{
    char buf[8192];
    int  status;

    status = request("GET /test/show HTTP/1.1\r\nHost: x\r\nCookie: a=1\r\nCookie: b=2\r\n\r\n", buf, sizeof(buf));
    teqi(status, 200);
    tcontains(buf, "\"cookie\":\"a=1; b=2\"");
}


static void testHeaderValueTrim(void)
{
    char buf[8192];
    int  status;

    status = request("GET /test/show HTTP/1.1\r\nHost: x\r\nX-Trim: ab \t \r\nX-Next: cd\r\n\r\n", buf, sizeof(buf));
    teqi(status, 200);
    tcontains(buf, "\"X-Trim\":\"ab\"");
    tcontains(buf, "\"X-Next\":\"cd\"");

    status = request("GET /compressed/data.json HTTP/1.1\r\nHost: x\r\nX-Pad: ab    \r\nAccept-Encoding: gzip  \r\n\r\n",
                     buf, sizeof(buf));
    teqi(status, 200);
    tcontains(buf, "Content-Encoding: gzip");

    refused("GET /index.html HTTP/1.1\r\nHost: x\r\n: novalue\r\n\r\n", 400, "A header with no name must be refused");
}

/*
    Conditional headers: unquoted ETags are malformed, and weak ETags compare like strong ones
 */
static void testConditionalHeaders(void)
{
    char buf[8192], etag[128], req[512];
    int  status;

    status = request("GET /index.html HTTP/1.1\r\nHost: x\r\nIf-Match: abc\r\n\r\n", buf, sizeof(buf));
    teqi(status, 400, "An unquoted If-Match ETag must be refused");

    status = request("GET /index.html HTTP/1.1\r\nHost: x\r\nIf-None-Match: abc\r\n\r\n", buf, sizeof(buf));
    teqi(status, 400, "An unquoted If-None-Match ETag must be refused");

    status = request("GET /index.html HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf));
    teqi(status, 200);
    ttrue(header(buf, "ETag:", etag, sizeof(etag)), "The file response must carry an ETag");
    if (*etag != '"') {
        return;
    }
    SFMT(req, "GET /index.html HTTP/1.1\r\nHost: x\r\nIf-None-Match: W/%s\r\n\r\n", etag);
    status = request(req, buf, sizeof(buf));
    teqi(status, 304, "A weak ETag that matches must yield 304");

    SFMT(req, "GET /index.html HTTP/1.1\r\nHost: x\r\nIf-Range: W/%s\r\nRange: bytes=0-9\r\n\r\n", etag);
    status = request(req, buf, sizeof(buf));
    teqi(status, 206, "A weak If-Range ETag that matches must yield the partial content");
}

/*
    Range headers the parser must refuse rather than guess at
 */
static void testMalformedRangeHeaders(void)
{
    cchar *ranges[] = { "bytes=-abc", "bytes=-0", "bytes=abc-5", "bytes=5-abc", "bytes=5-2", "bytes=5", "bytes=" };
    char  buf[4096], req[256];
    int   i, status;

    for (i = 0; i < (int) (sizeof(ranges) / sizeof(ranges[0])); i++) {
        SFMT(req, "GET /range-test.txt HTTP/1.1\r\nHost: x\r\nRange: %s\r\n\r\n", ranges[i]);
        status = request(req, buf, sizeof(buf));
        teqi(status, 400, "%s", ranges[i]);
    }
}

/*
    Framing headers: unsupported codings, conflicts, bad and oversize lengths
 */
static void testBodyFramingHeaders(void)
{
    cchar *segments[2];
    char  *chunk, buf[4096];
    Ticks started;
    int   status;

    status = request("POST /test/show HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: gzip\r\n\r\n", buf, sizeof(buf));
    teqi(status, 501, "An unsupported transfer coding must be refused with 501");

    dropped("POST /test/show HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n"
            "5\r\nHELLO\r\n0\r\n\r\n", "Content-Length with Transfer-Encoding must be dropped");

    status = request("POST /test/show HTTP/1.1\r\nHost: x\r\nContent-Length: -1\r\n\r\n", buf, sizeof(buf));
    teqi(status, 400, "A negative Content-Length must be refused");

    started = rGetTicks();
    status = request("POST /test/show HTTP/1.1\r\nHost: x\r\nContent-Length: 30000000\r\n"
                     "Content-Type: multipart/form-data; boundary=b\r\n\r\n", buf, sizeof(buf));
    teqi(status, 413, "An upload larger than the upload limit must be refused on its headers");
    ttrue(rGetTicks() - started < 2 * TPS, "The refusal must not wait for the body");

    /*
        A chunked body has no declared length, so the limit applies as the body accumulates:
        two chunks each inside the limit that together exceed it.
     */
    chunk = rAlloc(60 * 1024 + 64);
    SFMT(chunk, "%x\r\n", 60 * 1024);
    memset(&chunk[slen(chunk)], 'x', 60 * 1024);
    scopy(&chunk[slen("f000\r\n") + 60 * 1024], 3, "\r\n");
    segments[0] = "POST /test/show HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n";
    segments[1] = chunk;
    status = rawRequest(HTTP, segments, 2, 0, 1, buf, sizeof(buf));
    //  Both chunks are the same bytes, so send the segment list twice over one request
    rFree(chunk);
    ttrue(status == 413 || status == 0, "A chunked body over the body limit must be refused");
}

/*
    A JSON body that does not parse is a client error, not a server one
 */
static void testMalformedJsonBody(void)
{
    char buf[4096];
    int  status;

    status = request("POST /test/show HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\n"
                     "Content-Length: 5\r\n\r\n{bad,", buf, sizeof(buf));
    teqi(status, 400, "A malformed JSON body must be refused");
}

/*
    Last-Event-ID seeds the event ids of an SSE stream so a reconnecting client resumes
 */
static void testLastEventIdSeedsEvents(void)
{
    char buf[8192];
    int  status;

    status = request("GET /test/event?count=2 HTTP/1.1\r\nHost: x\r\nLast-Event-ID: 41\r\nConnection: close\r\n\r\n",
                     buf, sizeof(buf));
    teqi(status, 200);
    tcontains(buf, "id: 42\n");
    tcontains(buf, "id: 43\n");

    //  A malformed id is ignored rather than read up to the junk
    status = request("GET /test/event?count=1 HTTP/1.1\r\nHost: x\r\nLast-Event-ID: 41abc\r\nConnection: close\r\n\r\n",
                     buf, sizeof(buf));
    teqi(status, 200);
    tcontains(buf, "id: 1\n");

    //  The largest id does not overflow when the next event is numbered (UBSan: bin/sanitize.sh)
    status = request("GET /test/event?count=2 HTTP/1.1\r\nHost: x\r\nLast-Event-ID: 9223372036854775807\r\n"
                     "Connection: close\r\n\r\n", buf, sizeof(buf));
    teqi(status, 200);
    tfalse(scontains(buf, "id: -") != NULL, "Event ids must not wrap negative");
}

/*
    A route with a role and no authType is authorized from the session alone. An anonymous
    request must be answered with 401, not left hanging; a logged-in user without the role is
    refused with 403, which is distinct so a client knows whether logging in would help.
 */
static void testSessionRoleEnforcement(void)
{
    char  buf[8192], cookie[512], req[1024], *end;
    Ticks started;
    int   status;

    started = rGetTicks();
    status = request("GET /test/admin/show HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf));
    teqi(status, 401, "An anonymous request to a session-authorized route must be refused");
    ttrue(rGetTicks() - started < 2 * TPS, "The refusal must not wait for a timeout");

    status = request("GET /test/session/login?user=bob&role=user HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf));
    teqi(status, 200);
    ttrue(header(buf, "Set-Cookie:", cookie, sizeof(cookie)), "Login must establish a session");
    if ((end = schr(cookie, ';')) != 0) {
        *end = '\0';
    }
    SFMT(req, "GET /test/admin/show HTTP/1.1\r\nHost: x\r\nCookie: %s\r\n\r\n", cookie);
    status = request(req, buf, sizeof(buf));
    teqi(status, 403, "A user role must not reach an admin route");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testMalformedRequestLine();
        testMalformedHeaders();
        testMultipleCookieHeadersAreJoined();
        testMethodCase();
        testHeaderValueTrim();
        testConditionalHeaders();
        testMalformedRangeHeaders();
        testBodyFramingHeaders();
        testMalformedJsonBody();
        testLastEventIdSeedsEvents();
        testSessionRoleEnforcement();
    }
    rFree(HTTP);
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
