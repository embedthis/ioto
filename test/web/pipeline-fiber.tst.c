/*
    pipeline-fiber.tst.c - Every pipelined request sees its fiber

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4879"
#define REQUEST  "GET /fiber HTTP/1.1\r\nHost: x\r\n\r\n"

/************************************ Code ************************************/

static void fiberAction(Web *web)
{
    webWriteResponse(web, 200, "fiber=%s\n", web->fiber == rGetFiber() ? "ok" : (web->fiber ? "stale" : "null"));
}

static void testPipelinedFiber(void)
{
    WebHost *host;
    cchar   *segments[1];
    char    response[4096], *cp;
    int     ok, status;

    host = startTestHost("{ web: { documents: './site', listen: ['" ENDPOINT "'], "
                         "routes: [ { match: '/fiber', handler: 'action' } ] } }");
    if (!host) {
        return;
    }
    webAddAction(host, "/fiber", fiberAction, NULL);

    segments[0] = REQUEST REQUEST "GET /fiber HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
    status = rawRequest(ENDPOINT, segments, 1, 0, 3, response, sizeof(response));
    teqi(status, 200);
    teqi(rawCountResponses(response, (ssize) slen(response)), 3);

    for (ok = 0, cp = response; (cp = scontains(cp, "fiber=ok")) != 0; cp++) {
        ok++;
    }
    teqi(ok, 3, "Every pipelined request must see its fiber: %s", response);
    stopTestHost(host);
}

static void fiberMain(void *data)
{
    testPipelinedFiber();
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
