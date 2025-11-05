/*
    socket.tst.c - Unit tests for rSocket

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/*********************************** Locals ***********************************/

#define TIMEOUT (15 * 1000)

typedef struct TestSocket {
    RFiber *fiber;                           /* Primary test fiber */
    RSocket *listen;                         /* Server listen socket */
    RSocket *client;                         /* Client socket */
    RBuf *buf;                               /* Input buffer */
    ssize written;                           /* Size of data written */
    int port;                                /* Server port */
    Ticks deadline;                          /* Timeout deadline */
} TestSocket;

static TestSocket *ts;
static int        hasInternet = 0;
static int        hasIPv6 = 0;

/***************************** Forward Declarations ***************************/

static void acceptFn(TestSocket *ts, RSocket *sp);
static RSocket *openServer(TestSocket *ts, cchar *host);
static void fiberMain();

/************************************ Code ************************************/
/*
    Initialize the TestSocket structure and find a free server port to listen on.
    Also determine if we have an internet connection.
 */
static int initSocketTests()
{
    RSocket *sp;

    if ((ts = rAllocType(TestSocket)) == 0) {
        return 0;
    }
    ts->deadline = rGetTicks() + TIMEOUT;
    hasInternet = rCheckInternet();

    //  Test for IPv6 support
    if ((sp = openServer(ts, "::1")) != 0) {
        hasIPv6 = 1;
        rFreeSocket(sp);
    }
    return 0;
}


static void termSocketTests()
{
    rCloseSocket(ts->listen);
    rFreeBuf(ts->buf);
}


static void createSocket()
{
    RSocket *sp;

    sp = rAllocSocket();

    tnotnull(sp);
    teqz(rGetSocketHandle(sp), INVALID_SOCKET);
    ttrue(!rIsSocketSecure(sp));
    ttrue(!rIsSocketEof(sp));

    rFreeSocket(sp);
}


static void client()
{
    RSocket *sp;
    int     rc;

    if (hasInternet) {
        sp = rAllocSocket();
        tnotnull(sp);
        rc = rConnectSocket(sp, "www.google.com", 80, 0);

        ttrue(rc >= 0);
        tgti(rGetSocketHandle(sp), 0);
        ttrue(!rIsSocketSecure(sp));
        ttrue(!rIsSocketEof(sp));

        rCloseSocket(sp);
        ttrue(rIsSocketEof(sp));
        teqz(rGetSocketHandle(sp), INVALID_SOCKET);

        rFreeSocket(sp);
    }
}


/*
    Open a server on a free port
 */
static RSocket *openServer(TestSocket *tp, cchar *host)
{
    RSocket *sp;
    int     port;

    if ((sp = rAllocSocket()) == 0) {
        return 0;
    }
    for (port = 9175; port < 9250; port++) {
        tp->port = port;
        if (rListenSocket(sp, host, port, (RSocketProc) acceptFn, tp) != SOCKET_ERROR) {
            return sp;
        }
    }
    return 0;
}


static void clientServer(cchar *host)
{
    RSocket *sp;
    char    *buf;
    ssize   nbytes;
    int     i, rc, count;

    ts->listen = openServer(ts, host);
    tnotnull(ts->listen);
    if (ts->listen == 0) {
        return;
    }
    sp = rAllocSocket();
    tnotnull(sp);

    ts->fiber = rGetFiber();
    ts->buf = rAllocBuf(0);
    ts->written = 0;

    rc = rConnectSocket(sp, host, ts->port, ts->deadline);
    ttrue(rc >= 0);

    /*
        Write a set of lines to the client. Server should receive. Use blocking mode. This writes about 500K of data.
     */
    buf = "01234567890123456789012345678901234567890123456789\r\n";
    count = 10000;

    for (i = 0; i < count; i++) {
        nbytes = rWriteSocket(sp, buf, slen(buf), ts->deadline);
        if (nbytes < 0) {
            ttrue(nbytes > 0);
            break;
        }
        ts->written += nbytes;
    }
    rFreeSocket(sp);

    //  Wait for read side to resume when read is complete
    rYieldFiber(0);

    //  Test complete
    rFreeSocket(ts->listen);
    ts->listen = 0;
    rFreeBuf(ts->buf);
    ts->buf = 0;
}


static void acceptFn(TestSocket *tp, RSocket *sp)
{
    ssize nbytes;

    do {
        rReserveBufSpace(tp->buf, ME_BUFSIZE);
        if ((nbytes = rReadSocket(sp, rGetBufEnd(tp->buf), rGetBufSpace(tp->buf), tp->deadline)) < 0) {
            break;
        }
        rAdjustBufEnd(tp->buf, nbytes);
    } while (nbytes > 0);

    teqz(rGetBufLength(tp->buf), tp->written);
    rResumeFiber(tp->fiber, 0);
}


static void clientServerIPv4()
{
    clientServer("127.0.0.1");
}


static void clientServerIPv6()
{
    if (hasIPv6) {
        clientServer("::1");
    }
}


#if ME_COM_SSL && UNUSED
static void clientSslv4()
{
    RSocket *sp;
    int     rc;

    if (hasInternet) {
        sp = rAllocSocket();
        ttrue(sp != 0);
        rSetSocketTLS(sp);
        rc = rConnectSocket(sp, "www.google.com", 443, 0);
        ttrue(rc >= 0);
        ttrue(rIsSocketSecure(sp));
        rCloseSocket(sp);
        rFreeSocket(sp);
    }
}
#endif


int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}


static void fiberMain(void *arg)
{
    initSocketTests();
    createSocket();
    client();
#if !WIN
    clientServerIPv4();
    clientServerIPv6();
#endif
#if ME_COM_SSL && UNUSED
    clientSslv4();
#endif
    termSocketTests();
    rStop();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
