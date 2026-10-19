/*
    host-allowlist.tst.c - web.hosts refuses a Host the server does not answer to

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4861"
#define WS_HEADERS "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" \
                   "Sec-WebSocket-Version: 13\r\n"

/************************************ Code ************************************/

static int get(cchar *path, cchar *headers)
{
    cchar *segments[1];
    char  *req, buf[4096];
    int   status;

    req = sfmt("GET %s HTTP/1.1\r\n%s\r\n", path, headers);
    segments[0] = req;
    status = rawRequest(ENDPOINT, segments, 1, 0, 1, buf, sizeof(buf));
    rFree(req);
    return status;
}

static void testAllowlist(void)
{
    WebHost *host;
    int     status;

    host = startTestHost("{ web: { documents: './site/', listen: ['" ENDPOINT "'], "
                         "hosts: ['device.local', '127.0.0.1:4861', '[::1]'], webSockets: { enable: true }, "
                         "routes: [ { match: '/test/', handler: 'action' }, {} ] } }");
    if (!host) {
        return;
    }
    status = get("/index.html", "Host: device.local\r\nConnection: close\r\n");
    teqi(status, 200, "A listed name is accepted");
    status = get("/index.html", "Host: DEVICE.local:4861\r\nConnection: close\r\n");
    teqi(status, 200, "A listed name is accepted with a port and in any case");
    status = get("/index.html", "Host: 127.0.0.1:4861\r\nConnection: close\r\n");
    teqi(status, 200, "A listed name and port is accepted");
    status = get("/index.html", "Host: [::1]:4861\r\nConnection: close\r\n");
    teqi(status, 200, "A listed IPv6 literal is accepted with a port");
    status = get("/index.html", "Host: rebind.example.com\r\nConnection: close\r\n");
    teqi(status, 421, "An unlisted name is misdirected");
    status = get("/index.html", "Host: device.local.example.com\r\nConnection: close\r\n");
    teqi(status, 421, "A name that only starts with a listed name is misdirected");
    status = get("/test/ws", "Host: rebind.example.com:4861\r\nOrigin: http://rebind.example.com:4861\r\n" WS_HEADERS);
    teqi(status, 421, "A rebinding WebSocket whose Origin matches its Host is refused");
    stopTestHost(host);

    host = startTestHost("{ web: { documents: './site/', listen: ['" ENDPOINT "'], routes: [ {} ] } }");
    if (host) {
        status = get("/index.html", "Host: anything.example.com\r\nConnection: close\r\n");
        teqi(status, 200, "Without web.hosts any Host is accepted");
        stopTestHost(host);
    }
}

static void fiberMain(void *arg)
{
    if (setup(NULL, NULL)) {
        testAllowlist();
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
