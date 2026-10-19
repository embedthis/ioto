/*
    duplicate-headers.tst.c - Reject ambiguous authentication and framing headers (issue 20)

    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult LICENSE.md for details.
 */

#include "test.h"

static char *HTTP;

static int probe(cchar *headers, cchar *body)
{
    cchar *segments[1];
    char  request[1024], response[4096];
    int   status;

    SFMT(request, "POST /test/show HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n%s\r\n%s",
         headers, body);
    segments[0] = request;
    status = rawRequest(HTTP, segments, 1, 0, 1, response, sizeof(response));
    return status;
}

static void testDuplicateAuthorization(void)
{
    int status;

    status = probe("Authorization: Basic first\r\nContent-Length: 0\r\n", "");
    teqi(status, 200);
    status = probe("Authorization: Basic first\r\nAuthorization: Basic first\r\nContent-Length: 0\r\n", "");
    teqi(status, 400);
    status = probe("Authorization: Basic first\r\naUtHoRiZaTiOn: Digest second\r\nContent-Length: 0\r\n", "");
    teqi(status, 400);
    status = probe("Authorization:\r\nAuthorization: Basic second\r\nContent-Length: 0\r\n", "");
    teqi(status, 400);
    status = probe("Authorization: Basic\r\nAuthorization: Basic second\r\nContent-Length: 0\r\n", "");
    teqi(status, 400);
}

static void testDuplicateTransferEncoding(void)
{
    int status;

    status = probe("Transfer-Encoding: chunked\r\n", "0\r\n\r\n");
    teqi(status, 200);
    status = probe("Transfer-Encoding: chunked\r\nTransfer-Encoding: chunked\r\n", "0\r\n\r\n");
    teqi(status, 400);
    status = probe("Transfer-Encoding: chunked\r\ntRaNsFeR-EnCoDiNg: chunked\r\n", "0\r\n\r\n");
    teqi(status, 400);
    status = probe("Transfer-Encoding: chunked\r\nTransfer-Encoding: gzip\r\n", "0\r\n\r\n");
    teqi(status, 400);
}

static void testDuplicateContentType(void)
{
    int status;

    status = probe("Content-Type: text/plain\r\nContent-Length: 0\r\n", "");
    teqi(status, 200);
    status = probe("Content-Type: text/plain\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n", "");
    teqi(status, 400);
    status = probe("Content-Type: multipart/form-data; boundary=AAA\r\n"
                   "Content-Type: multipart/form-data; boundary=BBB\r\n"
                   "Content-Length: 0\r\n", "");
    teqi(status, 400);
}

static void testRepeatedCookies(void)
{
    WebHost host = {0};
    Web     web = {0};
    char    headers[] = "Cookie: first=one\r\ncOoKiE: second=two\r\nCookie: third=three\r\n";
    bool    parsed;

    web.host = &host;
    parsed = webParseHeadersBlock(&web, headers, sizeof(headers) - 1, 0);
    ttrue(parsed);
    tmatch(web.cookie, "first=one; second=two; third=three");
    rFree(web.cookie);
}

/*
    The probe already sends Host; this adds the forbidden duplicate.
 */
static void testDuplicateHost(void)
{
    teqi(probe("Content-Length: 0\r\n", ""), 200, "A single Host is accepted");
    teqi(probe("Host: other\r\nContent-Length: 0\r\n", ""), 400, "A second Host is refused");
    teqi(probe("hOsT: localhost\r\nContent-Length: 0\r\n", ""), 400, "A repeated Host is refused whatever its case");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testDuplicateAuthorization();
        testDuplicateHost();
        testDuplicateTransferEncoding();
        testDuplicateContentType();
        testRepeatedCookies();
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
