/*
    unread-body-close.tst.c - An error response with an unread body must release the connection

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define BASE  "http://localhost:4884"
#define LIMIT 4

static cchar *config =
    "{ web: { documents: './site', listen: ['http://:4884'], limits: { connections: 4 },"
    "  timeouts: { inactivity: '30 secs', request: '60 secs' },"
    "  auth: { realm: 'Test Realm', authType: 'digest', secret: 'unread-body-close-secret-1234',"
    "          roles: { user: [] }, users: {} },"
    "  routes: [ { match: '/digest/', authType: 'digest', role: 'user', handler: 'file' }, { } ] } }";

/************************************ Code ************************************/

static void refuseAndClose(void)
{
    RSocket *sp;
    char    buf[1024];
    cchar   *req;
    ssize   nbytes;

    req = "POST /digest/index.html HTTP/1.1\r\nHost: x\r\nContent-Length: 10000\r\n\r\n"
          "0123456789012345678901234567890123456789012345678901234567890123456789";
    if ((sp = rawConnect(BASE)) == 0) {
        tfail("Cannot connect");
        return;
    }
    rWriteSocket(sp, req, slen(req), rGetTicks() + 5 * TPS);
    nbytes = rReadSocket(sp, buf, sizeof(buf) - 1, rGetTicks() + 5 * TPS);
    buf[max(nbytes, 0)] = '\0';
    ttrue(scontains(buf, " 401 ") != 0, "Request is refused before its body is read");
    //  Close with most of the declared body unsent
    rFreeSocket(sp);
}

static void testUnreadBodyClose(void)
{
    WebHost *host;
    RSocket *sp;
    char    buf[1024];
    cchar   *req;
    ssize   nbytes;
    int     connections, i;

    if ((host = startTestHost(config)) == 0) {
        return;
    }
    for (i = 0; i < LIMIT * 2; i++) {
        refuseAndClose();
    }
    //  Let the server observe each close
    rSleep(500);

    connections = (int) host->connections;
    teqi(connections, 0, "Every refused connection is released");

    req = "GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
    if ((sp = rawConnect(BASE)) == 0) {
        tfail("Cannot connect");
    } else {
        rWriteSocket(sp, req, slen(req), rGetTicks() + 5 * TPS);
        nbytes = rReadSocket(sp, buf, sizeof(buf) - 1, rGetTicks() + 5 * TPS);
        buf[max(nbytes, 0)] = '\0';
        ttrue(scontains(buf, "HTTP/1.1 200") != 0, "Server still serves after the refused requests");
        rFreeSocket(sp);
    }
    stopTestHost(host);
}

static void fiberMain(void *data)
{
    testUnreadBodyClose();
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
