/*
    shutdown-inflight.tst.c - Freeing a host with requests in flight frees each connection once

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4860"
#define CLIENTS  4

/************************************ Code ************************************/

static void testFreeWithRequestsInFlight(void)
{
    WebHost *host;
    RSocket *clients[CLIENTS];
    cchar   *partial;
    int     i;

    host = startTestHost("{ web: { documents: './site/', listen: ['" ENDPOINT "'], show: 'hH', "
                         "routes: [ { match: '/test/', handler: 'action' }, {} ] } }");
    if (!host) {
        return;
    }
    //  Half-sent POSTs: each server fiber blocks reading the rest of the body
    partial = "POST /test/show HTTP/1.1\r\nHost: 127.0.0.1:4860\r\nContent-Type: text/plain\r\n"
              "Content-Length: 100\r\n\r\nabc";
    for (i = 0; i < CLIENTS; i++) {
        clients[i] = rawConnect(ENDPOINT);
        tnotnull(clients[i]);
        if (clients[i]) {
            rWriteSocket(clients[i], partial, slen(partial), rGetTicks() + 5 * TPS);
        }
    }
    rSleep(200);
    teqi(rGetListLength(host->webs), CLIENTS, "Every connection is in flight");

    stopTestHost(host);
    //  Let any connection fiber resumed by the close run before the test ends
    rSleep(200);
    ttrue(1, "The host was freed with requests in flight");

    for (i = 0; i < CLIENTS; i++) {
        rFreeSocket(clients[i]);
    }
}

static void fiberMain(void *arg)
{
    if (setup(NULL, NULL)) {
        testFreeWithRequestsInFlight();
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
