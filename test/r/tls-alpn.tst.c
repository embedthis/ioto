/*
    tls-alpn.tst.c - ALPN lists are encoded correctly and the selected protocol is the shared one

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#define TLS_PORT 23050
#include "tls-common.h"

#if ME_COM_SSL
#if ME_COM_OPENSSL
    #include <openssl/ssl.h>
#endif

/************************************ Code ************************************/


static bool negotiate(cchar *serverAlpn, cchar *clientAlpn, char *negotiated, size_t size)
{
    RSocket *sp;
    bool    ok;

    scopy(negotiated, size, "");
    if (!startListener("valid", NULL, 0, serverAlpn, NULL)) {
        tinfo("Skipped: No valid test certificate");
        return 0;
    }
    sp = rAllocSocket();
    rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    rSetSocketVerify(sp, 1, 1);
    rSetTls(sp);
    rSetTlsAlpn(sp->tls, clientAlpn);
    ok = rConnectSocket(sp, TEST_HOST, listenPort, rGetTicks() + 5 * TPS) >= 0;
#if ME_COM_OPENSSL
    if (ok) {
        const uchar *proto;
        size_t      len;
        void        *session;

        proto = NULL;
        len = 0;
        if ((session = rGetTlsSession(sp)) != 0) {
            SSL_SESSION_get0_alpn_selected((SSL_SESSION*) session, &proto, &len);
            if (proto && len < size) {
                memcpy(negotiated, proto, len);
                negotiated[len] = '\0';
            }
            rFreeTlsSession(session);
        }
    }
#endif
    if (ok) {
        rWriteSocket(sp, "x", 1, rGetTicks() + TPS);
    }
    rFreeSocket(sp);
    stopServer();
    return ok;
}

static void checkNegotiated(cchar *serverAlpn, cchar *clientAlpn, cchar *expect)
{
    char got[256];
    bool ok;

    ok = negotiate(serverAlpn, clientAlpn, got, sizeof(got));
    if (!rFileExists(CERT_DIR "/valid.crt")) {
        return;
    }
    ttrue(ok, "Server \"%s\" and client \"%s\" complete a handshake", serverAlpn, clientAlpn);
#if ME_COM_OPENSSL
    teqi(scmp(got, expect), 0, "Server \"%s\" and client \"%s\" negotiate \"%s\", got \"%s\"",
         serverAlpn, clientAlpn, expect, got);
#else
    (void) expect;
#endif
}

static void testSingleProtocol(void)
{
    checkNegotiated("http/1.1", "http/1.1", "http/1.1");
}

/*
    Exactly two protocols filled the MbedTLS array to capacity with no terminator.
 */
static void testTwoProtocolLists(void)
{
    checkNegotiated("h2,http/1.1", "h2,http/1.1", "h2");
    checkNegotiated("h2,http/1.1", "http/1.1", "http/1.1");
    checkNegotiated("http/1.1", "h2,http/1.1", "http/1.1");
}

static void testThreeProtocolLists(void)
{
    checkNegotiated("h2 http/1.1 spdy/3", "spdy/3, http/1.1", "http/1.1");
}

/*
    With nothing in common RFC 7301 permits either a refusal or no negotiated protocol, but never a
    protocol the client did not offer.
 */
static void testNoCommonProtocol(void)
{
    char got[256];

    negotiate("h2", "http/1.1", got, sizeof(got));
    tfalse(scmp(got, "h2") == 0, "A protocol the client did not offer must not be selected");
}

static void fiberMain(void *arg)
{
    (void) arg;
    testSingleProtocol();
    testTwoProtocolLists();
    testThreeProtocolLists();
    testNoCommonProtocol();
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
