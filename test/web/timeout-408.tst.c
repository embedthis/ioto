/*
    timeout-408.tst.c - A request that times out is answered, an idle connection is not

    408 was in the status table and emitted nowhere: every timeout path closed the connection
    without writing a response, so a client could not tell "you were too slow" from a crash or a
    dropped link.

    The shared server cannot test this - its web.json5 sets a 60 second parse budget, longer than
    a test may wait - so these run against an in-process host with a one second budget, on a port
    inside the 4850-4899 range reserved for that.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4873"

static cchar *CONFIG =
    "{"
    "    web: {"
    "        listen: [ '" ENDPOINT "' ],"
    "        documents: './site',"
    "        index: 'index.html',"
    "        routes: [ {} ],"
    "        timeouts: { parse: '1secs', inactivity: '1secs', request: '30secs' },"
    "    }"
    "}";

/************************************ Code ************************************/

/*
    Headers that never complete. The request has begun, so the server owes the client a status:
    RFC 9110 section 15.5.9. Before the fix this closed silently and the client read nothing.
 */
static void testPartialHeadersAreAnswered(void)
{
    RSocket *sock;
    cchar   *request;
    char    response[2048];
    ssize   nbytes;

    if ((sock = rawConnect(ENDPOINT)) == 0) {
        tfail("Cannot connect for the partial header test");
        return;
    }
    //  No final CRLF, so the header block never completes
    request = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n";
    nbytes = rWriteSocket(sock, request, slen(request), rGetTicks() + 5000);
    tgti((int) nbytes, 0);

    //  Read well past the one second parse budget so the server has had its chance to act
    nbytes = rReadSocket(sock, response, sizeof(response) - 1, rGetTicks() + 10000);
    tgti((int) nbytes, 0, "A timed-out request must be answered, not closed silently");
    if (nbytes > 0) {
        response[nbytes] = '\0';
        tcontains(response, "408");
        tcontains(response, "Connection: close");
    }
    rCloseSocket(sock);
    rFreeSocket(sock);
}

/*
    The complementary case, which must stay true: an idle keep-alive connection with no request in
    flight is closed with no response at all. RFC 9112 section 9.5 allows exactly that, and a 408
    here would be a response to a request the client never sent.
 */
static void testIdleKeepAliveIsSilent(void)
{
    RSocket *sock;
    cchar   *request;
    char    response[2048];
    ssize   nbytes, total;
    bool    sawEof;

    if ((sock = rawConnect(ENDPOINT)) == 0) {
        tfail("Cannot connect for the idle keep-alive test");
        return;
    }
    request = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    nbytes = rWriteSocket(sock, request, slen(request), rGetTicks() + 5000);
    tgti((int) nbytes, 0);

    nbytes = rReadSocket(sock, response, sizeof(response) - 1, rGetTicks() + 5000);
    tgti((int) nbytes, 0);
    if (nbytes <= 0) {
        rCloseSocket(sock);
        rFreeSocket(sock);
        return;
    }
    response[nbytes] = '\0';
    tcontains(response, "200");

    /*
        Now hold the connection open and send nothing. Drain to end of stream: whatever arrives is
        the tail of the response already in flight, and it must never carry a status line.
     */
    sawEof = 0;
    total = 0;
    while (total < 8192) {
        /*
            rReadSocket reports end of stream as R_ERR_CANT_READ, not 0: rReadSocketSync turns a
            zero-length recv into that code. Either way, no more bytes are coming.
         */
        nbytes = rReadSocket(sock, response, sizeof(response) - 1, rGetTicks() + 10000);
        if (nbytes <= 0) {
            sawEof = 1;
            break;
        }
        response[nbytes] = '\0';
        tnull(scontains(response, "408"));
        total += nbytes;
    }
    ttrue(sawEof, "An idle keep-alive connection must be closed without a response");

    rCloseSocket(sock);
    rFreeSocket(sock);
}

static void fiberMain(void *data)
{
    WebHost *host;

    if ((host = startTestHost(CONFIG)) != 0) {
        testPartialHeadersAreAnswered();
        testIdleKeepAliveIsSilent();
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
