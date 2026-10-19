/*
    status-line.tst.c - Request-line validation and response status-line framing

    The request line used to be split with stok on " \t", which skips runs of delimiters, and the
    version token was taken up to the CR and echoed verbatim as the first token of every response.
    A bare LF is not a CR, so a version token could carry one and split the response: the client
    received an attacker-authored response ahead of the server's own. Separately, chunked framing was
    selected purely on an unknown length, so an HTTP/1.0 response carried Transfer-Encoding and hex
    chunk sizes that a genuine 1.0 client reads as body content.

    These run over raw sockets: the point is the bytes on the wire, not what a client library would
    let us express.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

/************************************ Code ************************************/

/*
    Send one raw request and return the response status. The whole response is left in buf.
 */
static int probe(cchar *request, char *buf, size_t size)
{
    cchar *segments[1];

    segments[0] = request;
    return rawRequest(HTTP, segments, 1, 0, 1, buf, size);
}

/*
    Return the first line of the response, null terminated in line.
 */
static void firstLine(cchar *buf, char *line, size_t size)
{
    cchar *cp;
    size_t len;

    if ((cp = scontains(buf, "\r\n")) == 0) {
        cp = &buf[slen(buf)];
    }
    len = min((size_t) (cp - buf), size - 1);
    memcpy(line, buf, len);
    line[len] = '\0';
}

/*
    A well-formed request must be unchanged: the server's own version, the right status, and a
    response the client library reads normally.
 */
static void testWellFormedRequestLine(void)
{
    char buf[8192], line[256];
    int  status;

    status = probe("GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n", buf, sizeof(buf));
    teqi(status, 200);
    firstLine(buf, line, sizeof(line));
    tmatch(line, "HTTP/1.1 200 OK");
    tcontains(buf, "<html>");

    status = probe("GET /index.html HTTP/1.0\r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    teqi(status, 200);
    firstLine(buf, line, sizeof(line));
    tmatch(line, "HTTP/1.0 200 OK");
    tcontains(buf, "<html>");
}

/*
    Anything but "method SP target SP version" is a shape a front end may resolve differently, so it
    must be refused rather than normalized.
 */
static void testMalformedRequestLineRefused(void)
{
    char buf[8192];
    int  status;

    status = probe("GET /index.html HTTP/1.1 EXTRA\r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    teqi(status, 400, "A fourth request-line token must be refused, not echoed into the status line");

    status = probe("GET\t\t/index.html\t\tHTTP/1.1\r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    teqi(status, 400, "Tabs are not request-line delimiters");

    status = probe("GET  /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    teqi(status, 400, "A run of spaces must be refused");

    status = probe("GET /index.html  HTTP/1.1\r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    teqi(status, 400, "A run of spaces before the version must be refused");

    status = probe("GET /index.html \r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    teqi(status, 400, "A trailing space with no version must be refused");

    status = probe("GET /index.html\r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    teqi(status, 400, "A missing version must be refused");
}

/*
    An unknown version is 505, never an echo and never treated as HTTP/1.1 with keep-alive.
 */
static void testUnknownVersionRefused(void)
{
    char buf[8192], line[256];
    int  status;

    status = probe("GET /index.html HTTP/9.9\r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    teqi(status, 505, "An unknown HTTP version must be refused");
    firstLine(buf, line, sizeof(line));
    tcontains(line, "HTTP/1.1 505");

    status = probe("GET /index.html JUNK/9.9\r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    teqi(status, 505, "A junk version token must be refused");
    firstLine(buf, line, sizeof(line));
    tcontains(line, "HTTP/1.1 505");

    status = probe("GET /index.html GARBAGE\r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    teqi(status, 505, "A version with no slash must be refused");
}

/*
    The response splitting regression. A bare LF in the version token used to land in the status
    line, so a recipient that honours a lone LF read an entire attacker-authored response first.
 */
static void testNoResponseSplitting(void)
{
    char buf[8192], line[256];
    cchar *cp;
    int  status;

    status = probe("GET /index.html HTTP/1.1\nX-INJECTED: YES\r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    teqi(status, 400, "A bare LF in the version token must be refused");
    tfalse(scontains(buf, "X-INJECTED") != 0, "The injected header must not reach the response");

    status = probe("GET /index.html HTTP/1.1\nCONTENT-LENGTH: 22\nCONTENT-TYPE: TEXT/HTML\n\n"
                   "<HTML>INJECTED</HTML>\nX:\r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    teqi(status, 400, "A full injected response in the version token must be refused");
    tfalse(scontains(buf, "INJECTED") != 0, "The injected body must not reach the response");

    //  A bare LF with no space after it still cannot reach the version token
    status = probe("GET /index.html HTTP/1.1\nX-INJECTED:YES\r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    teqi(status, 505, "A bare LF glued to the version must be refused");
    tfalse(scontains(buf, "X-INJECTED") != 0, "The injected header must not reach the response");

    /*
        Whatever the request, the status line is the server's own and the response carries no bare
        LF: every LF must be preceded by a CR.
     */
    firstLine(buf, line, sizeof(line));
    ttrue(sstarts(line, "HTTP/1.1 ") || sstarts(line, "HTTP/1.0 "), "The status line must be the server's own");

    for (cp = buf; *cp; cp++) {
        if (*cp == '\n' && (cp == buf || cp[-1] != '\r')) {
            tfail("The response must contain no bare LF");
            break;
        }
    }
}

/*
    An unknown method must be refused before it is stored, and a method that is not an RFC 9110
    token must never be accepted at all.
 */
static void testMethodValidation(void)
{
    char buf[8192], line[256];
    int  status;

    status = probe("BAD\x01METHOD /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    teqi(status, 400, "A control byte in the method must be refused");

    status = probe("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n",
                   buf, sizeof(buf));
    teqi(status, 400, "An over-long method must be refused");

    //  An unknown but well-formed token is refused by the route, not by the parser
    status = probe("BREW /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    tgtei(status, 400, "An unknown method must not be served");
    firstLine(buf, line, sizeof(line));
    ttrue(sstarts(line, "HTTP/1.1 "), "The status line must be the server's own");
}

/*
    HTTP/1.0 has no chunked coding. A 1.0 response of unknown length must be framed by the
    connection close, and its body must match the dechunked HTTP/1.1 body byte for byte.
 */
static void testNoChunkingOnHttp10(void)
{
    char  buf[65536], *body;
    ssize len;
    int   status;

    //  /test/bulk streams 100 records of "Hello World %010d\n" with no Content-Length
    status = probe("GET /test/bulk HTTP/1.0\r\nHost: localhost\r\n\r\n", buf, sizeof(buf));
    teqi(status, 200);
    tfalse(sncaselesscontains(buf, "transfer-encoding", slen(buf)) != 0,
           "An HTTP/1.0 response must carry no Transfer-Encoding");
    tcontains(buf, "Connection: close");

    ttrue((body = scontains(buf, "\r\n\r\n")) != 0, "The response must have a header terminator");
    body += 4;
    ttrue(sstarts(body, "Hello World 0000000000\n"), "The 1.0 body must start with data, not a chunk size");
    ttrue(sends(body, "Hello World 0000000099\n"), "The 1.0 body must end with data, not a zero chunk");
    len = (ssize) slen(body);
    teqz((size_t) len, (size_t) (100 * 23), "The 1.0 body must be the data alone with no chunk framing");
    tfalse(scontains(body, "\r\n") != 0, "The 1.0 body must carry no chunk dividers");

    //  The same route over HTTP/1.1 still chunks, and 0x17 is the 23-byte record
    status = probe("GET /test/bulk HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n", buf, sizeof(buf));
    teqi(status, 200);
    ttrue(sncaselesscontains(buf, "transfer-encoding: chunked", slen(buf)) != 0,
          "An HTTP/1.1 response of unknown length must still chunk");
    tcontains(buf, "\r\n\r\n17\r\nHello World 0000000000\n");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testWellFormedRequestLine();
        testMalformedRequestLineRefused();
        testUnknownVersionRefused();
        testNoResponseSplitting();
        testMethodValidation();
        testNoChunkingOnHttp10();
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
