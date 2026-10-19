/**
    close-drain.tst.c - rCloseSocket must not be held open by a peer that keeps sending

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "testme.h"
#include "r.h"

#if ME_UNIX_LIKE
#include <signal.h>
#include <sys/wait.h>

/*********************************** Locals ***********************************/

#define CLOSE_LIMIT     1000            //  Milliseconds rCloseSocket may take
#define WATCHDOG        10              //  Seconds before a regression is reported as a failure

static pid_t blaster;

/************************************ Code ************************************/

//  A hung drain must fail the test rather than hang the suite
static void watchdog(int sig)
{
    static cchar msg[] = "rCloseSocket is still draining while the peer streams\n";

    (void) sig;
    (void) !write(2, msg, sizeof(msg) - 1);
    if (blaster > 0) {
        kill(blaster, SIGKILL);
    }
    _exit(1);
}


static void blast(int port)
{
    struct sockaddr_in addr;
    static char        chunk[1 << 18];
    int                fd;

    memset(chunk, 'a', sizeof(chunk));
    fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((ushort) port);
    if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        _exit(2);
    }
    alarm(30);
    while (send(fd, chunk, sizeof(chunk), 0) > 0) {
    }
    _exit(0);
}

static int openListener(int *portp)
{
    struct sockaddr_in addr;
    socklen_t          len;
    int                lfd;

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(lfd, (struct sockaddr*) &addr, sizeof(addr)) < 0 || listen(lfd, 4) < 0) {
        close(lfd);
        return -1;
    }
    len = sizeof(addr);
    getsockname(lfd, (struct sockaddr*) &addr, &len);
    *portp = ntohs(addr.sin_port);
    return lfd;
}

//  Close a socket whose peer never stops sending. The close must return promptly.
static void testCloseWithStreamingPeer(void)
{
    RSocket *sp;
    Ticks   start, elapsed;
    int     fd, lfd, port;

    lfd = openListener(&port);
    ttrue(lfd >= 0);
    if (lfd < 0) {
        return;
    }
    if ((blaster = fork()) == 0) {
        blast(port);
    }
    fd = accept(lfd, NULL, NULL);
    ttrue(fd >= 0);

    sp = rAllocSocket();
    sp->fd = fd;
    rSleep(200);

    signal(SIGALRM, watchdog);
    alarm(WATCHDOG);
    start = rGetTicks();
    rCloseSocket(sp);
    elapsed = rGetTicks() - start;
    alarm(0);
    tinfo("rCloseSocket returned after %lld ms with the peer streaming", (long long) elapsed);
    ttrue(elapsed < CLOSE_LIMIT);

    kill(blaster, SIGKILL);
    waitpid(blaster, NULL, 0);
    blaster = 0;
    rFreeSocket(sp);
    close(lfd);
}

//  The bound must not cost anything when the peer is well behaved
static void testCloseWithSmallBacklog(void)
{
    struct sockaddr_in addr;
    RSocket            *sp;
    char               buf[1024];
    int                fd, lfd, peer, port;

    lfd = openListener(&port);
    ttrue(lfd >= 0);
    if (lfd < 0) {
        return;
    }
    peer = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((ushort) port);
    ttrue(connect(peer, (struct sockaddr*) &addr, sizeof(addr)) == 0);
    fd = accept(lfd, NULL, NULL);
    ttrue(fd >= 0);
    memset(buf, 'b', sizeof(buf));
    ttrue(send(peer, buf, sizeof(buf), 0) == (ssize_t) sizeof(buf));
    rSleep(50);

    sp = rAllocSocket();
    sp->fd = fd;
    rCloseSocket(sp);
    ttrue(rIsSocketClosed(sp));
    teqz(rGetSocketHandle(sp), INVALID_SOCKET);


    ttrue(recv(peer, buf, sizeof(buf), 0) == 0);
    rFreeSocket(sp);
    close(peer);
    close(lfd);
}

static void fiberMain(void *arg)
{
    (void) arg;
    testCloseWithSmallBacklog();
    testCloseWithStreamingPeer();
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

#else
int main(void)
{
    return 0;
}
#endif

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
