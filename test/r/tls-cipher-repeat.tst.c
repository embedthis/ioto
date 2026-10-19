/*
    tls-cipher-repeat.tst.c - Any configured cipher list must configure or be refused, never corrupt

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#define TLS_PORT 23040
#include "tls-common.h"

#if ME_COM_SSL

#if ME_COM_MBEDTLS
    #define SUITE "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256"
#else
    #define SUITE "ECDHE-RSA-AES128-GCM-SHA256"
#endif
#define REPEATS 200
//  A TLS 1.3 suite completes the list so both protocol generations have something to negotiate
#define TLS13_SUITE "TLS_AES_128_GCM_SHA256"

/************************************ Code ************************************/

static char *repeatedSuite(int count, cchar *separator)
{
    RBuf *buf;
    char *result;
    int  i;

    buf = rAllocBuf(0);
    for (i = 0; i < count; i++) {
        rPutToBuf(buf, "%s%s", i ? separator : "", SUITE);
    }
    rPutToBuf(buf, "%s%s", separator, TLS13_SUITE);
    result = rBufToString(buf);
    //  Take ownership of the string and release the buffer shell
    result = sclone(result);
    rFreeBuf(buf);
    return result;
}

static void checkRepeated(cchar *separator)
{
    RSocket *listener, *sp;
    char    *ciphers;

    if (!rFileExists(CERT_DIR "/valid.crt")) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    ciphers = repeatedSuite(REPEATS, separator);
    listener = startListener("valid", NULL, 0, NULL, ciphers);
    tnotnull(listener, "A long cipher list naming a supported suite configures the listener");
    if (!listener) {
        rFree(ciphers);
        return;
    }

    sp = rAllocSocket();
    rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    rSetSocketVerify(sp, 1, 1);
    rSetTls(sp);
    ttrue(roundTrip(sp, TEST_HOST), "A client can use a listener configured with a repeated cipher list");
    rFreeSocket(sp);
    stopServer();
    rFree(ciphers);
}

static void testRepeatedSuiteDoesNotOverflow(void)
{
    checkRepeated(":");
    checkRepeated(", ");
}

static void testUnsupportedListRefused(void)
{
    RSocket *sp;

    if (!startServer("valid")) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    sp = rAllocSocket();
    rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    rSetSocketVerify(sp, 1, 1);
    rSetSocketCiphers(sp, "NO-SUCH-CIPHER");
    tfalse(rConnectSocket(sp, TEST_HOST, listenPort, rGetTicks() + 5 * TPS) >= 0,
           "A cipher list naming no supported suite must not connect");
    tcontains(rGetSocketError(sp), "ciphers");
    rFreeSocket(sp);
    stopServer();
}

static void fiberMain(void *arg)
{
    (void) arg;
    testRepeatedSuiteDoesNotOverflow();
    testUnsupportedListRefused();
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
