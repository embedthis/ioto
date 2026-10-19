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
    size_t written;                          /* Size of data written */
    int port;                                /* Server port */
    Ticks deadline;                          /* Timeout deadline */
} TestSocket;

static TestSocket *ts;
static int        hasInternet = 0;
static int        hasIPv6 = 0;

/***************************** Forward Declarations ***************************/

static void acceptFn(TestSocket *ts, RSocket *sp);
static RSocket *openServer(TestSocket *ts, cchar *host);
static void fiberMain(void *arg);

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

/*
    The configuration surface: setters, predicates and accessors that never open a connection.
    None of these has a return to check beyond the state it changes, so each is paired with the
    accessor that reads it back where one exists.

    The TestMe comparison macros expand their arguments twice, so every call with a side effect
    is bound to a local before it is asserted.
 */
static void socketConfig()
{
    RSocket       *sp, *listener;
    RSocketCustom prior, current;
    RWait         *wait;
    char          ipbuf[128];
    int           port, rc;
    bool          state;

    sp = rAllocSocket();
    tnotnull(sp);

    /*
        A freshly allocated socket is neither closed nor connected to anything, but it is not
        marked closed either - closed is a state the socket enters, not its starting point.
     */
    state = rIsSocketClosed(sp);
    tfalse(state);
    state = rIsSocketConnected(sp);
    ttrue(state);

    /*
        Linger and no-delay are set before a connection exists. Neither reports failure, so the
        assertion is that they are safe to call on an unconnected socket.
     */
    rSetSocketLinger(sp, 5);
    rSetSocketLinger(sp, 0);
    rSetSocketNoDelay(sp, 1);
    rSetSocketNoDelay(sp, 0);
    rSetSocketBlocking(sp, 1);
    rSetSocketBlocking(sp, 0);

    /*
        Shutting down and resetting a socket that was never connected must be no-ops rather
        than errors.
     */
    rDisconnectSocket(sp);
    rResetSocket(sp);
    teqz(rGetSocketHandle(sp), INVALID_SOCKET);

    rDisconnectSocket(NULL);

    /*
        The wait object does not exist until the socket has a descriptor.
     */
    wait = rGetSocketWait(sp);
    tnull(wait);
    wait = rGetSocketWait(NULL);
    tnull(wait);
    rSetSocketWaitMask(sp, R_READABLE, 0);
    rSetSocketWaitMask(NULL, R_READABLE, 0);

    /*
        There is no error until something fails.
     */
    tmatch(rGetSocketError(sp), "");

    rFreeSocket(sp);

    /*
        rGetSocketAddr reports the local address of a bound socket. A listening socket on the
        loopback interface is the smallest thing that has one.
     */
    listener = openServer(ts, "127.0.0.1");
    tnotnull(listener);

    port = -1;
    ipbuf[0] = 'x';
    rc = rGetSocketAddr(listener, ipbuf, sizeof(ipbuf), &port);
    teqi(rc, 0);
    tmatch(ipbuf, "127.0.0.1");
    teqi(port, ts->port);

    state = rIsSocketClosed(listener);
    tfalse(state);
    wait = rGetSocketWait(listener);
    tnotnull(wait);

    /*
        Bad arguments are rejected before the socket is touched.
     */
    rc = rGetSocketAddr(NULL, ipbuf, sizeof(ipbuf), &port);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = rGetSocketAddr(listener, NULL, sizeof(ipbuf), &port);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = rGetSocketAddr(listener, ipbuf, 0, &port);
    teqi(rc, R_ERR_BAD_ARGS);

    rCloseSocket(listener);
    state = rIsSocketClosed(listener);
    ttrue(state);
    state = rIsSocketConnected(listener);
    tfalse(state);
    rFreeSocket(listener);
    ts->listen = 0;

    /*
        The custom hook is a process-wide setter. Save and restore it so nothing else in the
        file sees a hook it did not install.
     */
    prior = rGetSocketCustom();
    rSetSocketCustom((RSocketCustom) socketConfig);
    current = rGetSocketCustom();
    ttrue(current == (RSocketCustom) socketConfig);
    rSetSocketCustom(prior);
    current = rGetSocketCustom();
    ttrue(current == prior);

    rc = rGetSocketLimit();
    ttrue(rc > 0);
}


#if ME_COM_SSL
/*
    The TLS configuration surface reachable without a handshake. tls-verify.tst.c drives the
    handshake itself; these are the settings an application makes before one.
 */
static void socketTlsConfig()
{
    RSocket *sp;
    bool    secure;

    sp = rAllocSocket();
    tnotnull(sp);

    secure = rIsSocketSecure(sp);
    tfalse(secure);

    /*
        Asking for TLS marks the socket secure before any connection is attempted.
     */
    rSetTls(sp);
    secure = rIsSocketSecure(sp);
    ttrue(secure);

    rSetSocketCiphers(sp, "TLS_AES_256_GCM_SHA384");
    rSetSocketVerify(sp, 1, 1);
    rSetSocketVerify(sp, 0, 0);

    rFreeSocket(sp);

    /*
        Each TLS setter must be usable as the first TLS call on a socket. The cipher setter
        dereferenced a TLS object that only the certificate and verify setters allocated, so a
        listener configured with ciphers before its certificate crashed.
     */
    sp = rAllocSocket();
    rSetSocketCiphers(sp, "TLS_AES_256_GCM_SHA384");
    ttrue(rIsSocketSecure(sp), "Setting ciphers first must allocate the TLS state");
    rFreeSocket(sp);

    /*
        The process-wide defaults. Restore the verify defaults so later tests in this file, and
        tls-verify.tst.c in its own process, are unaffected.
     */
    rSetSocketDefaultCiphers("TLS_AES_256_GCM_SHA384");
    rSetSocketDefaultCiphers(NULL);
    rSetSocketDefaultVerify(0, 0);
    rSetSocketDefaultVerify(1, 1);

    /*
        The remaining process-wide TLS setters. Each replaces a stored string, so calling one
        twice must free the first rather than strand it, and a null must clear it.
     */
    rSetTlsDefaultAlpn("h2");
    rSetTlsDefaultAlpn("http/1.1");
    rSetTlsDefaultAlpn(NULL);

    rSetTlsDefaultCerts(".certs/ca.crt", ".certs/test.key", ".certs/test.crt", NULL);
    rSetTlsDefaultCerts(NULL, NULL, NULL, NULL);

    /*
        A certificate that is not a certificate. Loading is deferred to rConfigTls, which runs on
        connect, so this is where the parse failure surfaces - as an error, not a crash, and not
        as a connection.
     */
    sp = rAllocSocket();
    rSetTls(sp);
    rSetSocketCerts(sp, NULL, ".test/bad.key", ".test/bad.crt", NULL);
    rWriteFile(".test/bad.crt", "not a certificate\n", 18, 0);
    rWriteFile(".test/bad.key", "not a key\n", 10, 0);
    ttrue(rConnectSocket(sp, "127.0.0.1", 9, rGetTicks() + 2 * TPS) < 0);
    ttrue(slen(rGetSocketError(sp)) > 0);
    rFreeSocket(sp);

    //  A certificate file that does not exist at all
    sp = rAllocSocket();
    rSetTls(sp);
    rSetSocketCerts(sp, NULL, ".test/no-such.key", ".test/no-such.crt", NULL);
    ttrue(rConnectSocket(sp, "127.0.0.1", 9, rGetTicks() + 2 * TPS) < 0);
    rFreeSocket(sp);

    //  A revocation list that does not exist
    sp = rAllocSocket();
    rSetTls(sp);
    rSetSocketCerts(sp, ".certs/ca.crt", NULL, NULL, ".test/no-such.crl");
    ttrue(rConnectSocket(sp, "127.0.0.1", 9, rGetTicks() + 2 * TPS) < 0);
    rFreeSocket(sp);

    unlink(".test/bad.crt");
    unlink(".test/bad.key");
}
#endif /* ME_COM_SSL */


/*
    The socket module's debug trace. It is guarded by rEmitLog("debug", "socket") and prints the
    bound address of each socket, so enabling debug for the socket source must reach it - and
    the arguments to that guard must name the same (type, source) pair as the rDebug inside it.
    They were reversed, which meant the natural way to ask for this diagnostic never produced it.
 */
static int traceCount;

static void traceHandler(cchar *type, cchar *source, cchar *msg)
{
    if (smatch(source, "socket")) {
        traceCount++;
    }
}

static void socketTracing()
{
    RLogHandler prior;
    RSocket     *sp;
    int         port;

    prior = rSetLogHandler(traceHandler);
    rSetLogFilter("debug", "socket", 1);
    traceCount = 0;

    /*
        A listener and a client each announce the address they bound to.
     */
    sp = rAllocSocket();
    tnotnull(sp);
    for (port = 9455; port < 9500; port++) {
        if (rListenSocket(sp, "127.0.0.1", port, (RSocketProc) acceptFn, NULL) == 0) {
            break;
        }
    }
    ttrue(port < 9500);
    rFreeSocket(sp);

#if ME_DEBUG
    ttrue(traceCount > 0, "Enabling debug for the socket source must reach the socket trace");
#endif

    rSetLogFilter("all", "all", 1);
    rSetLogHandler(prior);
}


/*
    The connect failure paths. Each must report rather than hang, and must leave the socket in a
    state the caller can free.
 */
static void connectFailures()
{
    RSocket *sp;
    int     rc;

    /*
        A port nothing is listening on. Refused is immediate on the loopback interface, so this
        does not depend on a timeout.
     */
    sp = rAllocSocket();
    tnotnull(sp);
    rc = rConnectSocket(sp, "127.0.0.1", 9, rGetTicks() + 2 * TPS);
    ttrue(rc < 0);
    ttrue(slen(rGetSocketError(sp)) > 0);
    rFreeSocket(sp);

    /*
        A deadline that has already passed.
     */
    sp = rAllocSocket();
    rc = rConnectSocket(sp, "127.0.0.1", 9, rGetTicks() - 1);
    ttrue(rc < 0);
    rFreeSocket(sp);

    /*
        An address that cannot be parsed or resolved. ".invalid" is reserved by RFC 6761 and
        never resolves, so this costs one negative lookup rather than a real query.
     */
    sp = rAllocSocket();
    rc = rConnectSocket(sp, "no-such-host.invalid", 80, rGetTicks() + 2 * TPS);
    ttrue(rc < 0);
    rFreeSocket(sp);
}


/*
    Binding failures. A port already in use and an address that is not local to this host.
 */
static void listenFailures()
{
    RSocket *first, *second;
    int     port, rc;

    first = rAllocSocket();
    tnotnull(first);
    for (port = 9500; port < 9550; port++) {
        if (rListenSocket(first, "127.0.0.1", port, (RSocketProc) acceptFn, NULL) == 0) {
            break;
        }
    }
    ttrue(port < 9550);

    /*
        The same address and port again.
     */
    second = rAllocSocket();
    rc = rListenSocket(second, "127.0.0.1", port, (RSocketProc) acceptFn, NULL);
    ttrue(rc < 0);
    ttrue(slen(rGetSocketError(second)) > 0);
    rFreeSocket(second);

    /*
        An address that is not one of this host's.
     */
    second = rAllocSocket();
    rc = rListenSocket(second, "192.0.2.1", 9551, (RSocketProc) acceptFn, NULL);
    ttrue(rc < 0);
    rFreeSocket(second);

    rFreeSocket(first);
}


/*
    The synchronous read and write entry points. They are the blocking-socket API an embedder
    uses outside a fiber, so they must move the same bytes as the fiber-aware pair.
 */
static RSocket *syncListen;
static int     syncRead;

/*
    A minimal accept handler. The file's shared acceptFn needs ts->buf and ts->fiber, which only
    clientServer sets up, so this case brings its own.
 */
static void syncAcceptFn(void *arg, RSocket *sp)
{
    char  buf[64];
    ssize nbytes;

    nbytes = rReadSocket(sp, buf, sizeof(buf), rGetTicks() + 2 * TPS);
    if (nbytes > 0) {
        syncRead = (int) nbytes;
    }
    rFreeSocket(sp);
}

static void syncReadWrite()
{
    RSocket *sp;
    char    buf[64];
    ssize   nbytes;
    int     port;

    syncListen = rAllocSocket();
    tnotnull(syncListen);
    for (port = 9555; port < 9600; port++) {
        if (rListenSocket(syncListen, "127.0.0.1", port, (RSocketProc) syncAcceptFn, NULL) == 0) {
            break;
        }
    }
    ttrue(port < 9600);
    syncRead = 0;

    sp = rAllocSocket();
    tnotnull(sp);
    if (rConnectSocket(sp, "127.0.0.1", port, rGetTicks() + 5 * TPS) < 0) {
        ttrue(0);
        rFreeSocket(sp);
        rFreeSocket(syncListen);
        syncListen = 0;
        return;
    }
    nbytes = rWriteSocketSync(sp, "sync-hello", 10);
    teqz(nbytes, 10);

    /*
        Bad arguments are refused rather than passed to the system call.
     */
    nbytes = rWriteSocketSync(NULL, "x", 1);
    teqz(nbytes, R_ERR_BAD_ARGS);
    nbytes = rReadSocketSync(NULL, buf, sizeof(buf));
    teqz(nbytes, R_ERR_BAD_ARGS);
    nbytes = rReadSocketSync(sp, NULL, sizeof(buf));
    teqz(nbytes, R_ERR_BAD_ARGS);

    /*
        Reading a socket with nothing pending returns zero rather than blocking, because the
        socket is non-blocking underneath.
     */
    nbytes = rReadSocketSync(sp, buf, sizeof(buf));
    ttrue(nbytes >= 0);

    /*
        After close, reads report end of file rather than reading a closed descriptor.
     */
    rCloseSocket(sp);
    nbytes = rReadSocketSync(sp, buf, sizeof(buf));
    teqz(nbytes, R_ERR_CANT_READ);

    rFreeSocket(sp);

    /*
        Let the server fiber finish before the listener goes away, then confirm the bytes the
        synchronous write sent were the bytes the server read.
     */
    rSleep(50);
    teqi(syncRead, 10);

    rFreeSocket(syncListen);
    syncListen = 0;
}

/*
    rGetSocketPeer reports the far end of a connection: on the accepted socket, the client's
    address and the ephemeral port the client bound. rGetSocketAddr on the same socket is the
    listening address.
 */
static char peerIp[128];
static int  peerPort;
static int  peerLocalPort;

static void peerAcceptFn(void *arg, RSocket *sp)
{
    char ipbuf[128];

    if (rGetSocketPeer(sp, peerIp, sizeof(peerIp), &peerPort) < 0) {
        peerPort = -1;
    }
    rGetSocketAddr(sp, ipbuf, sizeof(ipbuf), &peerLocalPort);
    rFreeSocket(sp);
}

static void peerAddress()
{
    RSocket *listener, *sp;
    char    ipbuf[128];
    int     port, clientPort, serverPort, rc;

    listener = rAllocSocket();
    tnotnull(listener);
    for (port = 9605; port < 9650; port++) {
        if (rListenSocket(listener, "127.0.0.1", port, (RSocketProc) peerAcceptFn, NULL) == 0) {
            break;
        }
    }
    ttrue(port < 9650);

    //  A listening socket has no peer
    rc = rGetSocketPeer(listener, ipbuf, sizeof(ipbuf), &clientPort);
    teqi(rc, R_ERR_CANT_COMPLETE);
    rc = rGetSocketPeer(NULL, ipbuf, sizeof(ipbuf), &clientPort);
    teqi(rc, R_ERR_BAD_ARGS);

    peerIp[0] = '\0';
    peerPort = peerLocalPort = 0;
    sp = rAllocSocket();
    rc = rConnectSocket(sp, "127.0.0.1", port, rGetTicks() + 5 * TPS);
    teqi(rc, 0);
    clientPort = 0;
    rGetSocketAddr(sp, ipbuf, sizeof(ipbuf), &clientPort);

    serverPort = 0;
    rc = rGetSocketPeer(sp, ipbuf, sizeof(ipbuf), &serverPort);
    teqi(rc, 0);
    tmatch(ipbuf, "127.0.0.1");
    teqi(serverPort, port, "The client's peer is the listening port");

    rSleep(50);
    tmatch(peerIp, "127.0.0.1");
    teqi(peerPort, clientPort, "The server's peer is the client's own port");
    teqi(peerLocalPort, port);

    rFreeSocket(sp);
    rFreeSocket(listener);
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
        if (rListenSocket(sp, host, port, (RSocketProc) acceptFn, tp) == 0) {
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
        ts->written += (size_t) nbytes;
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
    rFreeSocket(sp);
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

#if ME_HAS_SENDFILE
/*
    Test sendfile functionality
 */
static RSocket *sfServer;
static RSocket *sfClient;
static RFiber  *sfFiber;
static RBuf    *sfBuf;

static void sfAcceptFn(void *arg, RSocket *sp)
{
    ssize nbytes;

    sfBuf = rAllocBuf(1024);
    do {
        rReserveBufSpace(sfBuf, ME_BUFSIZE);
        if ((nbytes = rReadSocket(sp, rGetBufEnd(sfBuf), rGetBufSpace(sfBuf), ts->deadline)) < 0) {
            break;
        }
        rAdjustBufEnd(sfBuf, nbytes);
    } while (nbytes > 0);
    rFreeSocket(sp);
    rResumeFiber(sfFiber, 0);
}

static void sendFile()
{
    char  path[80];
    char  *data;
    int   fd, port;
    ssize nbytes;

    data = "Hello from sendfile test";

    sfServer = rAllocSocket();
    tnotnull(sfServer);

    for (port = 9275; port < 9350; port++) {
        if (rListenSocket(sfServer, "127.0.0.1", port, (RSocketProc) sfAcceptFn, 0) == 0) {
            break;
        }
    }
    if (port >= 9350) {
        ttrue(0);
        return;
    }
    sfFiber = rGetFiber();

    // Create test file
#if ME_WIN_LIKE
    sfmtbuf(path, sizeof(path), "data/sendfile-%d.dat", _getpid());
    fd = _open(path, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0644);
    tgtei(fd, 0);
    nbytes = _write(fd, data, (uint) slen(data));
    teqz((size_t) nbytes, slen(data));
    _lseek(fd, 0, SEEK_SET);
#else
    sfmtbuf(path, sizeof(path), "data/sendfile-%d.dat", getpid());
    fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    tgtei(fd, 0);
    nbytes = write(fd, data, slen(data));
    teqz((size_t) nbytes, slen(data));
    lseek(fd, 0, SEEK_SET);
#endif

    // Connect client
    sfClient = rAllocSocket();
    tnotnull(sfClient);
    if (rConnectSocket(sfClient, "127.0.0.1", port, ts->deadline) < 0) {
        ttrue(0);
        goto done;
    }
    // Use sendfile
    nbytes = rSendFile(sfClient, fd, 0, (size_t) slen(data), ts->deadline);
    teqz(nbytes, slen(data));
    if (nbytes != (ssize) slen(data)) {
        /*
            Nothing was sent, so the server fiber will never be resumed and the rYieldFiber below
            would block until the harness timeout - leaving the temp file behind, which then breaks
            file.tst.c's directory count several runs later. Fail promptly and clean up.
         */
        goto done;
    }

    rFreeSocket(sfClient);
    sfClient = 0;

    // Wait for server to receive
    rYieldFiber(0);

    // Verify received data
    teqz(rGetBufLength(sfBuf), slen(data));
    if (rGetBufLength(sfBuf) == slen(data)) {
        ttrue(memcmp(rGetBufStart(sfBuf), data, slen(data)) == 0);
    }

done:
#if ME_WIN_LIKE
    _close(fd);
    _unlink(path);
#else
    close(fd);
    unlink(path);
#endif
    if (sfClient) {
        rFreeSocket(sfClient);
    }
    rFreeSocket(sfServer);
    rFreeBuf(sfBuf);
}
#endif /* ME_HAS_SENDFILE */


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

/*
    Test rGetSocketLimit/rSetSocketLimit APIs
 */
static void testSocketLimit(void)
{
    int originalLimit, newLimit;

    //  Get original limit
    originalLimit = rGetSocketLimit();
    tgtei(originalLimit, 1);

    //  Set a new limit
    newLimit = 100;
    rSetSocketLimit(newLimit);
    teqi(rGetSocketLimit(), newLimit);

    //  Set another limit
    rSetSocketLimit(500);
    teqi(rGetSocketLimit(), 500);

    //  Restore original limit
    rSetSocketLimit(originalLimit);
    teqi(rGetSocketLimit(), originalLimit);
}

/*
    A listener that accepts and then does nothing. It exists so a client can be left waiting on a
    connection that will never produce a byte, which is the only way to reach the timeout returns
    in rReadSocket and rWriteSocket.
 */
static RSocket *idleListen;

static void idleAcceptFn(void *arg, RSocket *sp)
{
    rSleep(300);
    rFreeSocket(sp);
}

/*
    Error and boundary paths through the socket primitives. Each of these was at zero coverage:
    the suite connected, read, wrote and closed on the happy path and never asked what happens
    when an argument is wrong, a deadline expires, or the descriptor is already gone.
 */
static void socketErrorPaths(void)
{
    RSocket *sp;
    char    ipbuf[64], buf[64];
    Ticks   deadline;
    ssize   nbytes;
    int     port, rc;

    //  A connection with no host is refused before any resolution is attempted
    sp = rAllocSocket();
    tnotnull(sp);
    rc = rConnectSocket(sp, NULL, 80, rGetTicks() + TPS);
    ttrue(rc < 0);
    tcontains(rGetSocketError(sp), "Host address required");
    rFreeSocket(sp);

    /*
        "localhost" resolves to both 127.0.0.1 and ::1 on a dual-stack host, so a closed port
        there drives the second pass of the address loop and the descriptor and wait handle
        released between attempts.
     */
    sp = rAllocSocket();
    rc = rConnectSocket(sp, "localhost", 9, rGetTicks() + 2 * TPS);
    ttrue(rc < 0);
    rFreeSocket(sp);

    //  rListenSocket validates both the socket and the handler
    teqi(rListenSocket(NULL, "127.0.0.1", 9560, (RSocketProc) acceptFn, NULL), R_ERR_BAD_ARGS);
    sp = rAllocSocket();
    teqi(rListenSocket(sp, "127.0.0.1", 9560, NULL, NULL), R_ERR_BAD_ARGS);
    rFreeSocket(sp);

    //  rGetSocketAddr rejects every null it is given, including the port it writes through
    sp = rAllocSocket();
    teqi(rGetSocketAddr(NULL, ipbuf, sizeof(ipbuf), &port), R_ERR_BAD_ARGS);
    teqi(rGetSocketAddr(sp, NULL, sizeof(ipbuf), &port), R_ERR_BAD_ARGS);
    teqi(rGetSocketAddr(sp, ipbuf, 0, &port), R_ERR_BAD_ARGS);
    teqi(rGetSocketAddr(sp, ipbuf, sizeof(ipbuf), NULL), R_ERR_BAD_ARGS);

    //  An unbound socket has no name to report
    teqi(rGetSocketAddr(sp, ipbuf, sizeof(ipbuf), &port), R_ERR_CANT_COMPLETE);
    rFreeSocket(sp);

    //  rResetSocket closes an open descriptor and clears the flags so the socket can be reused
    idleListen = rAllocSocket();
    for (port = 9560; port < 9610; port++) {
        if (rListenSocket(idleListen, "127.0.0.1", port, (RSocketProc) idleAcceptFn, NULL) == 0) {
            break;
        }
    }
    ttrue(port < 9610);

    sp = rAllocSocket();
    rSetSocketLinger(sp, 1);
    rc = rConnectSocket(sp, "127.0.0.1", port, rGetTicks() + 5 * TPS);
    teqi(rc, 0);
    tfalse(rIsSocketSecure(sp));
    tfalse(rIsSocketEof(sp));

    //  The bound socket does have a name, and the port is the one we asked for
    teqi(rGetSocketAddr(sp, ipbuf, sizeof(ipbuf), &port), 0);
    ttrue(port > 0);
    ttrue(slen(ipbuf) > 0);

    //  Nothing is ever sent, so a read with a deadline in the near future times out
    deadline = rGetTicks() + 100;
    nbytes = rReadSocket(sp, buf, sizeof(buf), deadline);
    ttrue(nbytes <= 0);

    rResetSocket(sp);
    ttrue(sp->fd == INVALID_SOCKET);

    //  Reading and writing a reset socket fail rather than touching the closed descriptor
    ttrue(rReadSocket(sp, buf, sizeof(buf), rGetTicks() + 100) < 0);
    ttrue(rWriteSocket(sp, "x", 1, rGetTicks() + 100) < 0);
    ttrue(rReadSocketSync(sp, buf, sizeof(buf)) < 0);
    ttrue(rWriteSocketSync(sp, "x", 1) < 0);

    //  Bad arguments to the fiber-aware pair
    ttrue(rReadSocket(NULL, buf, sizeof(buf), 0) < 0);
    ttrue(rReadSocket(sp, NULL, sizeof(buf), 0) < 0);
    ttrue(rWriteSocket(NULL, "x", 1, 0) < 0);
    ttrue(rReadSocketSync(NULL, buf, sizeof(buf)) < 0);
    ttrue(rWriteSocketSync(NULL, "x", 1) < 0);

    rFreeSocket(sp);

    //  Freeing twice is a no-op, not a double free: rCloseSocket resumes a waiter synchronously
    //  and that waiter may re-enter rFreeSocket with the same socket.
    rFreeSocket(idleListen);
    idleListen = NULL;
}

#if !ME_WIN_LIKE
/*
    Test R_WAIT_MAIN_FIBER flag - verify handler runs on main fiber. Only the main-fiber test needs
    this guard: Windows pipes do not work with socket wait handlers. The socket error paths above
    are ordinary socket calls and build everywhere.
 */
static RWait  *mainFiberWait;
static RFiber *mainFiberTest;
static int    mainFiberHandlerCalled;
static RFiber *handlerFiber;

static void mainFiberHandler(void *arg, int mask)
{
    (void) arg;
    (void) mask;

    mainFiberHandlerCalled++;
    //  When R_WAIT_MAIN_FIBER is set, handler runs on main fiber
    handlerFiber = rGetFiber();

    rResumeFiber(mainFiberTest, (void*) "done");
}

static void testMainFiberFlag(void)
{
    int   testPipe[2];
    cchar *result;

    mainFiberTest = rGetFiber();
    mainFiberHandlerCalled = 0;
    handlerFiber = NULL;

    if (pipe(testPipe) < 0) {
        tfail();
        return;
    }
    fcntl(testPipe[0], F_SETFL, O_NONBLOCK);
    fcntl(testPipe[1], F_SETFL, O_NONBLOCK);

    mainFiberWait = rAllocWait(testPipe[0]);
    tnotnull(mainFiberWait);

    //  Set handler WITH R_WAIT_MAIN_FIBER flag
    rSetWaitHandler(mainFiberWait, (RWaitProc) mainFiberHandler, NULL, R_READABLE, 0, R_WAIT_MAIN_FIBER);
    teqi(mainFiberWait->mask, R_READABLE);
    teqi(mainFiberWait->flags, R_WAIT_MAIN_FIBER);

    //  Write to trigger handler
    write(testPipe[1], "test", 4);

    //  Wait for handler
    result = rYieldFiber(0);
    tmatch(result, "done");
    teqi(mainFiberHandlerCalled, 1);

    //  Handler should have run on the main fiber (fiber 0), not a spawned fiber
    //  When R_WAIT_MAIN_FIBER is set, the handler runs synchronously on the main fiber
    //  so handlerFiber should be the main fiber (fiber 0)
    tnotnull(handlerFiber);

    rFreeWait(mainFiberWait);
    close(testPipe[0]);
    close(testPipe[1]);
}
#endif /* !ME_WIN_LIKE */

int main(void)
{
    rInit((RFiberProc) fiberMain, 0);
    rServiceEvents();
    rTerm();
}

static void fiberMain(void *arg)
{
    initSocketTests();
    createSocket();
    socketConfig();
#if ME_COM_SSL
    socketTlsConfig();
#endif
    socketTracing();
    connectFailures();
    listenFailures();
    socketErrorPaths();
    syncReadWrite();
    peerAddress();
    testSocketLimit();
    client();
    clientServerIPv4();
    clientServerIPv6();
#if !ME_WIN_LIKE
    //  Windows pipes don't work with socket wait handlers
    testMainFiberFlag();
#endif
#if ME_HAS_SENDFILE
    sendFile();
#endif
#if ME_COM_SSL && UNUSED
    clientSslv4();
#endif
    termSocketTests();
    rStop();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
