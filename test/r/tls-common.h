/*
    tls-common.h - Shared fixture for the TLS tests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef _h_TLS_COMMON
#define _h_TLS_COMMON 1

#include "testme.h"
#include "r.h"

#define CERT_DIR  ".certs"
#define CA_CERT   "../certs/ca.crt"
#define TEST_HOST "localhost"

#ifndef TLS_PORT
    #error "Define TLS_PORT before including tls-common.h"
#endif

static RSocket *listenSock;
static int     listenPort;

/*
    Server handshakes may fail when clients reject certificates. Acknowledge successful data, then close.
 */
static inline void acceptFn(void *arg, RSocket *sp)
{
    char buf[128];

    (void) arg;
    if (rReadSocket(sp, buf, sizeof(buf), rGetTicks() + 2 * TPS) > 0) {
        rWriteSocket(sp, "k", 1, rGetTicks() + TPS);
    }
    rFreeSocket(sp);
}

/*
    Start a loopback listener with .certs/<name>.crt and optional client CA. Negative verifyPeer leaves policy
    unset. Return null if unavailable.
 */
static inline RSocket *startListener(cchar *name, cchar *ca, int verifyPeer, cchar *alpn, cchar *ciphers)
{
    RSocket *sp;
    char    cert[256], key[256];
    int     port;

    SFMT(cert, "%s/%s.crt", CERT_DIR, name);
    SFMT(key, "%s/%s.key", CERT_DIR, name);
    if (!rFileExists(cert) || !rFileExists(key)) {
        return 0;
    }
    sp = rAllocSocket();
    rSetSocketCerts(sp, ca, key, cert, NULL);
    if (verifyPeer >= 0) {
        rSetSocketVerify(sp, verifyPeer, verifyPeer);
    }
    if (ciphers) {
        rSetSocketCiphers(sp, ciphers);
    }
    if (alpn) {
        rSetTls(sp);
        rSetTlsAlpn(sp->tls, alpn);
    }
    for (port = TLS_PORT; port < TLS_PORT + 10; port++) {
        if (rListenSocket(sp, "127.0.0.1", port, (RSocketProc) acceptFn, NULL) == 0) {
            listenPort = port;
            listenSock = sp;
            return sp;
        }
    }
    rFreeSocket(sp);
    return 0;
}

static inline RSocket *startServer(cchar *name)
{
    return startListener(name, NULL, 0, NULL, NULL);
}

static inline void stopServer(void)
{
    rSleep(20);
    if (listenSock) {
        rFreeSocket(listenSock);
        listenSock = 0;
    }
    rSleep(20);
}


static inline bool connectOnce(RSocket *sp, cchar *host)
{
    int rc;

    rc = rConnectSocket(sp, host, listenPort, rGetTicks() + 5 * TPS);
    if (rc >= 0) {
        rWriteSocket(sp, "x", 1, rGetTicks() + TPS);
    }
    rResetSocket(sp);
    rSleep(5);
    return rc >= 0;
}

/*
    Require a byte/ack round trip to prove server acceptance, including TLS 1.3 client verification after the client
    handshake.
 */
static inline bool roundTrip(RSocket *sp, cchar *host)
{
    char buf[8];
    bool ok;

    ok = 0;
    if (rConnectSocket(sp, host, listenPort, rGetTicks() + 5 * TPS) >= 0) {
        if (rWriteSocket(sp, "x", 1, rGetTicks() + TPS) == 1) {
            ok = rReadSocket(sp, buf, sizeof(buf), rGetTicks() + 2 * TPS) == 1;
        }
    }
    rResetSocket(sp);
    rSleep(5);
    return ok;
}

#endif /* _h_TLS_COMMON */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
