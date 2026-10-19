/*
    connection-tokens.tst.c - The Connection header is a token list, and "close" anywhere in it closes

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4854"

/************************************ Code ************************************/


static int responses(cchar *connection, bool *announcedClose)
{
    cchar *segments[1], *end;
    char  *req, buf[16384];
    int   status;

    req = sfmt("GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: %s\r\n\r\n"
               "GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", connection);
    segments[0] = req;
    status = rawRequest(ENDPOINT, segments, 1, 0, 2, buf, sizeof(buf));
    rFree(req);
    //  Only the first response's header block
    end = scontains(buf, "\r\n\r\n");
    *announcedClose = end && sncaselesscontains(buf, "Connection: close", (size_t) (end - buf)) != 0;
    return status == 200 ? rawCountResponses(buf, slen(buf)) : 0;
}

static void testCloseInTokenList(void)
{
    WebHost *host;
    cchar   *values[] = { "close", "Close", "keep-alive, close", "close, TE", "TE, close", "Upgrade, close", 0 };
    bool    closed;
    int     i;

    host = startTestHost("{ web: { documents: './site/', listen: ['" ENDPOINT "'], routes: [ {} ] } }");
    if (!host) {
        return;
    }
    teqi(responses("keep-alive", &closed), 2, "keep-alive serves the pipelined request");
    tfalse(closed, "keep-alive is not announced as close");

    for (i = 0; values[i]; i++) {
        teqi(responses(values[i], &closed), 1, "Connection: %s must end the connection", values[i]);
        ttrue(closed, "Connection: %s must be answered with Connection: close", values[i]);
    }
    //  Trailing whitespace is optional whitespace, not part of the option
    teqi(responses("close ", &closed), 1, "Trailing OWS must not hide close");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (setup(NULL, NULL)) {
        testCloseInTokenList();
    }
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
