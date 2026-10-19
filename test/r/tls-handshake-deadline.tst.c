/*
    tls-handshake-deadline.tst.c - A TLS handshake must stop at its deadline

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#define TLS_PORT 23000
#include "tls-common.h"

#if ME_COM_SSL && ME_UNIX_LIKE
#include <signal.h>
#include <unistd.h>

/*********************************** Locals ***********************************/

#define DEADLINE_MS 1000

static int  heartbeats;
static bool beating;

/************************************ Code ************************************/

static void onAlarm(int sig)
{
    static cchar msg[] = "FAIL: TLS handshake spun past its deadline and starved the event loop\n";

    (void) sig;
    if (write(2, msg, sizeof(msg) - 1) < 0) {
        //  Nothing more can be done
    }
    _exit(1);
}

static void heartbeat(void *arg)
{
    (void) arg;
    if (!beating) {
        return;
    }
    heartbeats++;
    rStartEvent((REventProc) heartbeat, 0, 100);
}

//  Accept and hold the connection without sending a byte
static void silentAccept(void *arg, RSocket *sp)
{
    (void) arg;
    rSleep(4 * TPS);
    rFreeSocket(sp);
}

static void testClientDeadline(void)
{
    RSocket *listen, *sp;
    Ticks   start, elapsed;
    int     before, rc;

    listen = rAllocSocket();
    ttrue(rListenSocket(listen, "127.0.0.1", TLS_PORT, (RSocketProc) silentAccept, NULL) == 0);
    beating = 1;
    rStartEvent((REventProc) heartbeat, 0, 100);

    signal(SIGALRM, onAlarm);
    alarm(10);

    sp = rAllocSocket();
    rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    rSetSocketVerify(sp, 0, 0);
    before = heartbeats;
    start = rGetTicks();
    rc = rConnectSocket(sp, "127.0.0.1", TLS_PORT, start + DEADLINE_MS);
    elapsed = rGetTicks() - start;
    alarm(0);

    tlti(rc, 0, "A handshake with a silent peer must fail");
    tgtei((int) elapsed, DEADLINE_MS - 50, "The handshake must wait for its deadline");
    tlti((int) elapsed, 3 * DEADLINE_MS, "The handshake must give up at its deadline, took %d ms", (int) elapsed);
    tgtei(heartbeats - before, 5, "Other fibers and timers must keep running during the handshake");

    beating = 0;
    rFreeSocket(sp);
    rFreeSocket(listen);
}

static void fiberMain(void *arg)
{
    (void) arg;
    testClientDeadline();
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
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
