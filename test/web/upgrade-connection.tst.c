/*
    upgrade-connection.tst.c - The protocol switch is decided from Upgrade and Connection together

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4855"
#define WS_HEADERS "Upgrade: websocket\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" \
                   "Sec-WebSocket-Version: 13\r\n"

/************************************ Code ************************************/

static int upgrade(cchar *extra, cchar *body)
{
    cchar *segments[1];
    char  *req, buf[8192];
    int   status;

    req = sfmt("GET /test/ws HTTP/1.1\r\nHost: 127.0.0.1:4855\r\n" WS_HEADERS "%s\r\n%s", extra, body);
    segments[0] = req;
    status = rawRequest(ENDPOINT, segments, 1, 0, 1, buf, sizeof(buf));
    rFree(req);
    return status;
}

static void testUpgradeNeedsConnectionOption(void)
{
    WebHost *host;

    host = startTestHost("{ web: { documents: './site/', listen: ['" ENDPOINT "'], "
                         "webSockets: { enable: true }, "
                         "routes: [ { match: '/test/', handler: 'action' }, {} ] } }");
    if (!host) {
        return;
    }
    teqi(upgrade("Connection: Upgrade\r\n", ""), 101, "Connection: Upgrade switches protocols");
    teqi(upgrade("Connection: keep-alive, Upgrade\r\n", ""), 101, "The upgrade option may appear in a list");

    tneqi(upgrade("", ""), 101, "Upgrade without the Connection upgrade option must not switch protocols");
    tneqi(upgrade("Connection: keep-alive\r\n", ""), 101, "Connection without the upgrade option must not switch");
    tneqi(upgrade("Connection: close\r\n", ""), 101, "A client that asked to close must not be switched");
    tneqi(upgrade("Connection: Upgrade\r\nContent-Length: 6\r\n", "\x81\x80\x01\x02\x03\x04"), 101,
          "A declared request body must not be consumed as WebSocket frames");
    stopTestHost(host);
}


static void testUnactedUpgrade(void)
{
    WebHost *host;
    cchar   *segments[1];
    char    buf[8192];

    host = startTestHost("{ web: { documents: './site/', listen: ['" ENDPOINT "'], "
                         "webSockets: { enable: true }, "
                         "routes: [ { match: '/test/', handler: 'action' }, {} ] } }");
    if (!host) {
        return;
    }
    segments[0] = "GET /test/ws HTTP/1.1\r\nHost: 127.0.0.1:4855\r\nUpgrade: h2c\r\nConnection: Upgrade\r\n\r\n";
    teqi(rawRequest(ENDPOINT, segments, 1, 0, 1, buf, sizeof(buf)), 400,
         "A non-websocket Upgrade to the websocket action is refused");

    segments[0] = "GET /index.html HTTP/1.1\r\nHost: 127.0.0.1:4855\r\nUpgrade: h2c\r\nConnection: Upgrade\r\n\r\n";
    teqi(rawRequest(ENDPOINT, segments, 1, 0, 1, buf, sizeof(buf)), 200, "An unacted Upgrade is served as HTTP");
    tfalse(scontains(buf, "Connection: Upgrade") != NULL, "A non-101 response must not announce Connection: Upgrade");
    ttrue(scontains(buf, "Connection: keep-alive") != NULL, "A non-101 response carries the keep-alive decision");

    segments[0] = "GET /missing.html HTTP/1.1\r\nHost: 127.0.0.1:4855\r\nUpgrade: websocket\r\n"
                  "Connection: Upgrade\r\n\r\n";
    rawRequest(ENDPOINT, segments, 1, 0, 1, buf, sizeof(buf));
    tfalse(scontains(buf, "Connection: Upgrade") != NULL, "An error response must not announce Connection: Upgrade");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (setup(NULL, NULL)) {
        testUpgradeNeedsConnectionOption();
        testUnactedUpgrade();
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
