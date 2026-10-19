/*
    tls-mtls-authority.tst.c - A listener verifies client certificates against its own authority only

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#define TLS_PORT 23030
#include "tls-common.h"

#if ME_COM_SSL

/************************************ Code ************************************/


static bool roundTripAs(cchar *clientCert)
{
    RSocket *sp;
    char    cert[256], key[256];
    bool    ok;

    sp = rAllocSocket();
    if (clientCert) {
        SFMT(cert, "%s/%s.crt", CERT_DIR, clientCert);
        SFMT(key, "%s/%s.key", CERT_DIR, clientCert);
        rSetSocketCerts(sp, CA_CERT, key, cert, NULL);
    } else {
        rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    }
    rSetSocketVerify(sp, 1, 1);
    rSetTls(sp);
    ok = roundTrip(sp, TEST_HOST);
    rFreeSocket(sp);
    return ok;
}

static void testClientCertificateVerifiedAgainstListenerAuthority(void)
{
    if (!startListener("valid", CA_CERT, 1, NULL, NULL)) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    ttrue(roundTripAs("notrevoked"), "A client certificate issued by the listener authority is accepted");
    tfalse(roundTripAs("selfsigned"), "A client certificate not issued by the listener authority is refused");
    tfalse(roundTripAs(NULL), "A client presenting no certificate is refused");
    stopServer();
}

/*
    The outbound default trusts this certificate; accepting it would reveal unintended listener inheritance.
 */
static void testDefaultAuthorityIsNotClientAuthority(void)
{
    if (!rFileExists(CERT_DIR "/selfsigned.crt")) {
        tinfo("Skipped: No self-signed test certificate");
        return;
    }
    rSetSocketDefaultCerts(CERT_DIR "/selfsigned.crt", NULL, NULL, NULL);
    if (!startListener("valid", CA_CERT, 1, NULL, NULL)) {
        return;
    }
    tfalse(roundTripAs("selfsigned"), "The process default authority must not verify client certificates");
    stopServer();
    rSetSocketDefaultCerts(CA_CERT, NULL, NULL, NULL);
}


static void testVerifyWithoutAuthorityRefused(void)
{
    RSocket *sp;
    char    cert[256], key[256];

    SFMT(cert, "%s/valid.crt", CERT_DIR);
    SFMT(key, "%s/valid.key", CERT_DIR);
    if (!rFileExists(cert)) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    sp = rAllocSocket();
    rSetSocketCerts(sp, NULL, key, cert, NULL);
    rSetSocketVerify(sp, 1, 1);
    tlti(rListenSocket(sp, "127.0.0.1", TLS_PORT + 5, (RSocketProc) acceptFn, NULL), 0,
         "Client verification without an authority must refuse to start");
    rFreeSocket(sp);
}


static void testListenerDoesNotInheritOutboundVerify(void)
{
    if (!startListener("valid", NULL, -1, NULL, NULL)) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    ttrue(roundTripAs(NULL), "A listener with no verification policy must accept a client without a certificate");
    stopServer();
}

static void fiberMain(void *arg)
{
    (void) arg;
    testClientCertificateVerifiedAgainstListenerAuthority();
    testDefaultAuthorityIsNotClientAuthority();
    testVerifyWithoutAuthorityRefused();
    testListenerDoesNotInheritOutboundVerify();
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
