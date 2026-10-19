/*
    redact-headers.tst.c - Header dumps for --show and log.show carry no credentials

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void testRedactRequest(void)
{
    char *result;

    result = webRedactHeaders("GET http://host:80/ HTTP/1.1\r\nHost: x\r\nAuthorization: Basic YWxpY2U6c2VjcmV0\r\n"
                              "cookie: -web-session-=abc123\r\nPROXY-AUTHORIZATION: Digest x\r\nX-Other: keep\r\n");
    tfalse(scontains(result, "YWxpY2U6c2VjcmV0") != NULL, "Authorization value must be redacted");
    tfalse(scontains(result, "abc123") != NULL, "Cookie value must be redacted");
    tfalse(scontains(result, "Digest x") != NULL, "Proxy-Authorization value must be redacted");
    tcontains(result, "Authorization: [redacted]\r\n");
    tcontains(result, "cookie: [redacted]\r\n");
    tcontains(result, "GET http://host:80/ HTTP/1.1\r\nHost: x\r\n");
    tcontains(result, "X-Other: keep\r\n");
    rFree(result);
}

static void testRedactResponse(void)
{
    char *result;

    result = webRedactHeaders("HTTP/1.1 200 OK\r\nSet-Cookie: -web-session-=xyz; HttpOnly\r\nContent-Length: 0");
    tfalse(scontains(result, "xyz") != NULL, "Set-Cookie value must be redacted");
    tmatch(result, "HTTP/1.1 200 OK\r\nSet-Cookie: [redacted]\r\nContent-Length: 0");
    rFree(result);

    //  A name that only starts with a secret name is not redacted
    result = webRedactHeaders("Cookie-Policy: strict\r\n");
    tmatch(result, "Cookie-Policy: strict\r\n");
    rFree(result);
}

static void fiberMain(void *arg)
{
    testRedactRequest();
    testRedactResponse();
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
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
