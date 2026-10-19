/*
    tls-reconfig.tst.c - A policy set on a socket before a reconnect applies to that reconnect

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#define TLS_PORT 23020
#include "tls-common.h"

#if ME_COM_SSL

/************************************ Code ************************************/

static void testTightenedVerifyApplies(void)
{
    RSocket *sp;

    if (!startServer("selfsigned")) {
        tinfo("Skipped: No self-signed test certificate");
        return;
    }
    sp = rAllocSocket();
    rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    rSetSocketVerify(sp, 0, 0);
    rSetTls(sp);
    ttrue(connectOnce(sp, TEST_HOST), "With verification off an untrusted certificate is accepted");

    rSetSocketVerify(sp, 1, 1);
    tfalse(connectOnce(sp, TEST_HOST),
           "After verification is turned on, the next connection on the same socket must verify");

    rSetSocketVerify(sp, 0, 0);
    ttrue(connectOnce(sp, TEST_HOST), "After verification is turned off again, the next connection is not verified");
    rFreeSocket(sp);
    stopServer();
}

static void testChangedAuthorityApplies(void)
{
    RSocket *sp;

    if (!startServer("valid")) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    sp = rAllocSocket();
    rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    rSetSocketVerify(sp, 1, 1);
    rSetTls(sp);
    ttrue(connectOnce(sp, TEST_HOST), "A certificate from the trusted authority is accepted");

    //  Name a different authority that did not issue the server's certificate
    rSetSocketCerts(sp, CERT_DIR "/selfsigned.crt", NULL, NULL, NULL);
    tfalse(connectOnce(sp, TEST_HOST),
           "After the authority changes, the next connection must verify against the new authority");

    rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    ttrue(connectOnce(sp, TEST_HOST), "After the authority is restored, the connection succeeds again");
    rFreeSocket(sp);
    stopServer();
}

static void testChangedCiphersApply(void)
{
    RSocket *sp;

    if (!startServer("valid")) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    sp = rAllocSocket();
    rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    rSetSocketVerify(sp, 1, 1);
    rSetTls(sp);
    ttrue(connectOnce(sp, TEST_HOST), "The default cipher list connects");

    rSetSocketCiphers(sp, "NO_SUCH_CIPHER_SUITE");
    tfalse(connectOnce(sp, TEST_HOST), "After the cipher list changes, the next connection must use the new list");
    rFreeSocket(sp);
    stopServer();
}

static void fiberMain(void *arg)
{
    (void) arg;
    testTightenedVerifyApplies();
    testChangedAuthorityApplies();
    testChangedCiphersApply();
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
