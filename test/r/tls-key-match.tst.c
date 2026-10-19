/*
    tls-key-match.tst.c - A listener refuses a certificate and private key that do not belong together

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#define TLS_PORT 23080
#include "tls-common.h"

#if ME_COM_SSL

/************************************ Code ************************************/

static int listenWith(cchar *cert, cchar *key)
{
    RSocket *sp;
    int     rc;

    sp = rAllocSocket();
    rSetSocketCerts(sp, NULL, key, cert, NULL);
    rc = rListenSocket(sp, "127.0.0.1", TLS_PORT, (RSocketProc) acceptFn, NULL);
    rFreeSocket(sp);
    rSleep(20);
    return rc;
}

static void testMismatchedKeyRefused(void)
{
    int rc;

    if (!rFileExists(CERT_DIR "/valid.crt") || !rFileExists(CERT_DIR "/selfsigned.key")) {
        tinfo("Skipped: No test certificates");
        return;
    }
    rc = listenWith(CERT_DIR "/valid.crt", CERT_DIR "/valid.key");
    teqi(rc, 0);

    rc = listenWith(CERT_DIR "/valid.crt", CERT_DIR "/selfsigned.key");
    ttrue(rc < 0, "A listener with a private key that does not match its certificate must not start");
}

static void fiberMain(void *arg)
{
    (void) arg;
    testMismatchedKeyRefused();
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
