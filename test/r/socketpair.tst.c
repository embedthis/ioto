/**
    socketpair.tst.c - Loopback socket pair, peer verified accept and descriptor inheritance

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "testme.h"
#include "r.h"

/*********************************** Locals ***********************************/

#define INTRUDERS    17
#define TEST_PORT    9700
#define TEST_PORTS   50

static RFiber *mainFiber;
static bool   acceptedInheritable;

/************************************ Code ************************************/

#if ME_UNIX_LIKE || ME_WIN_LIKE
#define HAS_INHERIT_FLAG 1
#else
#define HAS_INHERIT_FLAG 0
#endif

static bool isInheritable(Socket fd)
{
#if ME_WIN_LIKE
    DWORD flags;

    return GetHandleInformation((HANDLE) fd, &flags) && (flags & HANDLE_FLAG_INHERIT);
#elif ME_UNIX_LIKE
    return !(fcntl(fd, F_GETFD) & FD_CLOEXEC);
#else
    (void) fd;
    return 0;
#endif
}

static bool waitReadable(Socket fd, int ms)
{
    struct timeval tv;
    fd_set         readSet;

    FD_ZERO(&readSet);
    FD_SET(fd, &readSet);
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    return select((int) fd + 1, &readSet, NULL, NULL, &tv) > 0;
}

static Socket listenLoopback(struct sockaddr_in *addr)
{
    Socket    fd;
    socklen_t len;

    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr->sin_port = 0;
    len = sizeof(*addr);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    ttrue(fd != INVALID_SOCKET);
    ttrue(bind(fd, (struct sockaddr*) addr, sizeof(*addr)) == 0);
    ttrue(listen(fd, 32) == 0);
    ttrue(getsockname(fd, (struct sockaddr*) addr, &len) == 0);
    return fd;
}

static Socket connectLoopback(struct sockaddr_in *addr, struct sockaddr_in *local)
{
    Socket    fd;
    socklen_t len;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    ttrue(fd != INVALID_SOCKET);
    ttrue(connect(fd, (struct sockaddr*) addr, sizeof(*addr)) == 0);
    if (local) {
        len = sizeof(*local);
        ttrue(getsockname(fd, (struct sockaddr*) local, &len) == 0);
    }
    return fd;
}

//  Transfer one byte and check it arrives intact
static void checkTransfer(Socket from, Socket to, char byte)
{
    char  buf[8];
    ssize nbytes;

    ttrue(send(from, &byte, 1, 0) == 1);
    ttrue(waitReadable(to, 2000), "The byte must arrive");
    nbytes = recv(to, buf, sizeof(buf), 0);
    ttrue(nbytes == 1);
    if (nbytes == 1) {
        teqi(buf[0], byte);
    }
}


static void testSocketPair(void)
{
    struct sockaddr_in a, b;
    Socket             pair[2];
    socklen_t          lenA, lenB;
    char               buf[8];
    int                rc;

    rc = rCreateSocketPair(pair);
    teqi(rc, 0);
    ttrue(pair[0] != INVALID_SOCKET);
    ttrue(pair[1] != INVALID_SOCKET);
    ttrue(pair[0] != pair[1]);

    //  Each end names the other as its peer
    lenA = lenB = sizeof(a);
    ttrue(getpeername(pair[0], (struct sockaddr*) &a, &lenA) == 0);
    ttrue(getsockname(pair[1], (struct sockaddr*) &b, &lenB) == 0);
    ttrue(a.sin_port == b.sin_port);
    ttrue(a.sin_addr.s_addr == b.sin_addr.s_addr);
    ttrue(ntohl(a.sin_addr.s_addr) == INADDR_LOOPBACK, "The pair must stay on the loopback interface");

    //  Both directions carry data
    checkTransfer(pair[1], pair[0], 'w');
    checkTransfer(pair[0], pair[1], 'r');

    //  Non-blocking: reading an empty socket returns at once instead of waiting
    ttrue(recv(pair[0], buf, sizeof(buf), 0) < 0);
    ttrue(recv(pair[1], buf, sizeof(buf), 0) < 0);

    //  A child process must not receive either end
    if (HAS_INHERIT_FLAG) {
        tfalse(isInheritable(pair[0]));
        tfalse(isInheritable(pair[1]));
    } else {
        tinfo("Skipped: No descriptor inheritance flag on this platform");
    }
    closesocket(pair[0]);
    closesocket(pair[1]);
}


static void testAcceptFromPeer(void)
{
    struct sockaddr_in addr, expected, accepted;
    Socket             listener, intruder, client, server;
    socklen_t          len;
    char               buf[8];

    listener = listenLoopback(&addr);
    intruder = connectLoopback(&addr, NULL);
    client = connectLoopback(&addr, &expected);

    server = rAcceptFromPeer(listener, &expected);
    ttrue(server != INVALID_SOCKET, "The expected peer must be accepted");
    if (server != INVALID_SOCKET) {
        len = sizeof(accepted);
        ttrue(getpeername(server, (struct sockaddr*) &accepted, &len) == 0);
        ttrue(accepted.sin_port == expected.sin_port);
        checkTransfer(client, server, 'c');
        closesocket(server);
    }
    //  The intruder was closed, not left connected
    ttrue(waitReadable(intruder, 2000), "The intruder must be disconnected");
    ttrue(recv(intruder, buf, sizeof(buf), 0) <= 0);

    closesocket(intruder);
    closesocket(client);
    closesocket(listener);
}


static void testAcceptFromPeerLimit(void)
{
    struct sockaddr_in addr, expected;
    Socket             intruders[INTRUDERS], listener, server;
    int                i;

    listener = listenLoopback(&addr);
    for (i = 0; i < INTRUDERS; i++) {
        intruders[i] = connectLoopback(&addr, NULL);
    }
    //  Nothing will ever connect from this endpoint
    memset(&expected, 0, sizeof(expected));
    expected.sin_family = AF_INET;
    expected.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    expected.sin_port = htons(1);

    server = rAcceptFromPeer(listener, &expected);
    ttrue(server == INVALID_SOCKET, "No connection matches, so the call must fail");
    if (server != INVALID_SOCKET) {
        closesocket(server);
    }
    //  It stopped at the bound rather than draining the queue
    ttrue(waitReadable(listener, 1000), "A connection must still be waiting");

    for (i = 0; i < INTRUDERS; i++) {
        closesocket(intruders[i]);
    }
    closesocket(listener);
}

static void testCloseOnExecFlag(void)
{
    Socket fd;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    ttrue(fd != INVALID_SOCKET);
    if (!HAS_INHERIT_FLAG) {
        tinfo("Skipped: No descriptor inheritance flag on this platform");
        closesocket(fd);
        return;
    }
    //  A new socket is inheritable until it is marked
    ttrue(isInheritable(fd));
    rSetSocketCloseOnExec(fd);
    tfalse(isInheritable(fd));
    closesocket(fd);
}

static void acceptHandler(void *arg, RSocket *sp)
{
    (void) arg;
    acceptedInheritable = isInheritable(rGetSocketHandle(sp));
    rFreeSocket(sp);
    rResumeFiber(mainFiber, (void*) "accepted");
}


static void testRuntimeSockets(void)
{
    RSocket *listener, *client;
    int     port, rc;

    if (!HAS_INHERIT_FLAG) {
        tinfo("Skipped: No descriptor inheritance flag on this platform");
        return;
    }
    mainFiber = rGetFiber();
    listener = rAllocSocket();
    for (port = TEST_PORT; port < TEST_PORT + TEST_PORTS; port++) {
        if (rListenSocket(listener, "127.0.0.1", port, (RSocketProc) acceptHandler, NULL) == 0) {
            break;
        }
    }
    ttrue(port < TEST_PORT + TEST_PORTS, "A listen port must be available");
    if (port >= TEST_PORT + TEST_PORTS) {
        rFreeSocket(listener);
        return;
    }
    tfalse(isInheritable(rGetSocketHandle(listener)), "Listener");

    client = rAllocSocket();
    rc = rConnectSocket(client, "127.0.0.1", port, rGetTicks() + 5 * TPS);
    teqi(rc, 0);
    tfalse(isInheritable(rGetSocketHandle(client)), "Outbound connection");

    acceptedInheritable = 1;
    rYieldFiber(0);
    tfalse(acceptedInheritable, "Accepted connection");

    rFreeSocket(client);
    rFreeSocket(listener);
}

static void fiberMain(void *arg)
{
    (void) arg;

    testSocketPair();
    testAcceptFromPeer();
    testAcceptFromPeerLimit();
    testCloseOnExecFlag();
    testRuntimeSockets();
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
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
