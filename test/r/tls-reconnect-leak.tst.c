/*
    tls-reconnect-leak.tst.c - Reconnecting one socket must not retain the previous connection

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#define TLS_PORT 23010
#include "tls-common.h"

#if ME_COM_SSL && ME_UNIX_LIKE
#include <sys/resource.h>

/*********************************** Locals ***********************************/

#define WARMUP   10
#define CYCLES   300
#define MAX_GROWTH_KB (4 * 1024)

/************************************ Code ************************************/

static size_t peakKB(void)
{
    struct rusage ru;

    getrusage(RUSAGE_SELF, &ru);
#if MACOSX
    return (size_t) ru.ru_maxrss / 1024;
#else
    return (size_t) ru.ru_maxrss;
#endif
}

/*
    Read to EOF before reconnecting to exercise cleanup when orderly TLS close was skipped.
 */
static bool connectUntilEof(RSocket *sp, cchar *host)
{
    char buf[16];
    int  rc;

    rc = rConnectSocket(sp, host, listenPort, rGetTicks() + 5 * TPS);
    if (rc >= 0) {
        rWriteSocket(sp, "x", 1, rGetTicks() + TPS);
        rReadSocket(sp, buf, sizeof(buf), rGetTicks() + 2 * TPS);
    }
    rResetSocket(sp);
    rSleep(5);
    return rc >= 0;
}

static void checkReconnectGrowth(bool (*connectFn)(RSocket *sp, cchar *host), cchar *label)
{
    RSocket *sp;
    size_t  after, before;
    int     i, ok;

    if (!startServer("valid")) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    sp = rAllocSocket();
    rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    rSetSocketVerify(sp, 1, 1);
    rSetTls(sp);

    for (i = 0; i < WARMUP; i++) {
        connectFn(sp, TEST_HOST);
    }
    before = peakKB();
    for (ok = 0, i = 0; i < CYCLES; i++) {
        ok += connectFn(sp, TEST_HOST);
    }
    after = peakKB();
    teqi(ok, CYCLES, "%s: every reconnect completes", label);
    tinfo("%s: peak RSS %zu KB to %zu KB over %d reconnects", label, before, after, CYCLES);
    tlti((int) (after - before), MAX_GROWTH_KB, "%s: reconnecting one socket must not grow memory per connection",
         label);
    rFreeSocket(sp);
    stopServer();
}

static void testReconnectDoesNotRetainSession(void)
{
    checkReconnectGrowth(connectOnce, "orderly close");
}

static void testReconnectAfterPeerEof(void)
{
    checkReconnectGrowth(connectUntilEof, "peer EOF");
}

static void fiberMain(void *arg)
{
    (void) arg;
    testReconnectDoesNotRetainSession();
    testReconnectAfterPeerEof();
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
