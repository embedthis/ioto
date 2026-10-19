/*
    keep-alive-rearm.tst.c - A kept-alive connection must not be re-dispatched while a request is running.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4885"

static cchar *config = "{"
    "web: { documents: './site', listen: ['" ENDPOINT "'],"
    "  routes: [ {match: '/slow', handler: 'action'}, {match: '/test/', handler: 'action'} ],"
    "  timeouts: { parse: '5secs', inactivity: '1secs', request: '30secs' } }"
"}";

/************************************ Code ************************************/

/*
    Blocks its fiber on a timer, not on the client socket, for 1.5 seconds
 */
static void slowAction(Web *web)
{
    rSleep(1500);
    webWriteFmt(web, "slept");
    webFinalize(web);
}

static void pipelinedBehindSlowAction(void)
{
    cchar *segments[] = {
        "GET /test/success HTTP/1.1\r\nHost: x\r\n\r\n",
        "GET /slow HTTP/1.1\r\nHost: x\r\n\r\n",
        "GET /test/success HTTP/1.1\r\nHost: x\r\n\r\n",
    };
    char buf[8192];
    int  status;

    status = rawRequest(ENDPOINT, segments, 3, 300, 3, buf, sizeof(buf));
    teqi(status, 200);
    teqi(rawCountResponses(buf, (ssize) slen(buf)), 3);
    tcontains(buf, "slept");
}

static void slowRequestNearInactivityDeadline(void)
{
    cchar *segments[] = {
        "GET /test/success HTTP/1.1\r\nHost: x\r\n\r\n",
        "GET /slow HTTP/1.1\r\nHost: x\r\n\r\n",
    };
    char buf[8192];
    int  status;

    //  The second request starts 0.8s into a 1s inactivity window and runs for 1.5s
    status = rawRequest(ENDPOINT, segments, 2, 800, 2, buf, sizeof(buf));
    teqi(status, 200);
    teqi(rawCountResponses(buf, (ssize) slen(buf)), 2);
    tcontains(buf, "slept");
}

static void stillServing(void)
{
    cchar *segments[] = { "GET /test/success HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n" };
    char  buf[4096];
    int   status;

    status = rawRequest(ENDPOINT, segments, 1, 0, 1, buf, sizeof(buf));
    teqi(status, 200);
}

static void fiberMain(void *arg)
{
    WebHost *host;

    if ((host = startTestHost(config)) != 0) {
        webAddAction(host, "/slow", slowAction, NULL);
        pipelinedBehindSlowAction();
        slowRequestNearInactivityDeadline();
        stillServing();
        stopTestHost(host);
    }
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
