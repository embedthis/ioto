/*
    keep-alive-cap.tst.c - Keep-alive request cap

    A connection serves at most limits.requests requests. The last permitted response says so with
    Connection: close and the server closes the socket, so a client cannot hold one connection open
    indefinitely by pipelining. Runs on an in-process host with a cap of three.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define BASE "http://localhost:4862"
#define CAP  3

/************************************ Code ************************************/

/*
    Return the start of the nth complete response in buf, or NULL
 */
static cchar *nthResponse(cchar *buf, int n)
{
    cchar *cp;
    int   i;

    cp = buf;
    for (i = 0; i < n; i++) {
        if ((cp = scontains(cp, "HTTP/1.1 ")) == 0) {
            return 0;
        }
        if (i < n - 1) {
            cp++;
        }
    }
    return cp;
}

static void testRequestCap(void)
{
    WebHost *host;
    RSocket *sp;
    cchar   *req = "GET /index.html HTTP/1.1\r\nHost: x\r\n\r\n";
    cchar   *response, *headersEnd;
    char    buf[65536], keepAlive[64];
    ssize   total, nbytes;
    int     i, count;

    host = startTestHost("{ web: { documents: './site', listen: ['http://:4862'], limits: { requests: 3 } } }");
    if (!host) {
        return;
    }
    if ((sp = rawConnect(BASE)) == 0) {
        tfail("Cannot connect");
        stopTestHost(host);
        return;
    }
    //  Pipeline one more request than the cap allows
    for (i = 0; i < CAP + 1; i++) {
        rWriteSocket(sp, req, slen(req), rGetTicks() + 5 * TPS);
    }
    //  Read until the server closes the socket
    total = 0;
    buf[0] = '\0';
    while (total < (ssize) sizeof(buf) - 1) {
        nbytes = rReadSocket(sp, &buf[total], sizeof(buf) - 1 - (size_t) total, rGetTicks() + 3 * TPS);
        if (nbytes <= 0) {
            break;
        }
        total += nbytes;
        buf[total] = '\0';
    }
    rFreeSocket(sp);

    count = rawCountResponses(buf, total);
    teqi(count, CAP, "Exactly the capped number of requests are answered");

    for (i = 1; i <= count; i++) {
        response = nthResponse(buf, i);
        if (!response || (headersEnd = scontains(response, "\r\n\r\n")) == 0) {
            tfail("Response %d incomplete", i);
            break;
        }
        if (i < CAP) {
            ttrue(sncaselesscontains(response, "Connection: keep-alive", (size_t) (headersEnd - response)) != 0,
                  "Response %d keeps the connection alive", i);
            SFMT(keepAlive, "max=%d", CAP - i);
            ttrue(sncaselesscontains(response, keepAlive, (size_t) (headersEnd - response)) != 0,
                  "Response %d announces %s", i, keepAlive);
        } else {
            ttrue(sncaselesscontains(response, "Connection: close", (size_t) (headersEnd - response)) != 0,
                  "The last permitted response announces the close");
        }
    }
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (setup(NULL, NULL)) {
        testRequestCap();
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
