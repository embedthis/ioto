/*
    tls-ip-literal.tst.c - A verifying client that connects by IP address matches iPAddress SANs

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#define TLS_PORT 23060
#include "tls-common.h"

#if ME_COM_SSL

/************************************ Code ************************************/

static bool verifiedConnect(cchar *certName, cchar *host, cchar **error)
{
    RSocket *sp;
    bool    ok;

    sp = rAllocSocket();
    rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    rSetSocketVerify(sp, 1, 1);
    rSetTls(sp);
    ok = startServer(certName) ? roundTrip(sp, host) : 0;
    if (!ok && error) {
        *error = sclone(rGetSocketError(sp));
    }
    rFreeSocket(sp);
    stopServer();
    return ok;
}

static void testIpSanVerifies(void)
{
    cchar *error;

    if (!rFileExists(CERT_DIR "/ipsan.crt")) {
        tinfo("Skipped: No IP address test certificate");
        return;
    }
    error = NULL;
    ttrue(verifiedConnect("ipsan", "127.0.0.1", &error),
          "A certificate with a matching iPAddress SAN verifies when connecting by IP: %s", error ? error : "");
    rFree((char*) error);
}

static void testWrongIpSanRejected(void)
{
    if (!rFileExists(CERT_DIR "/ipwrong.crt")) {
        tinfo("Skipped: No IP address test certificate");
        return;
    }
    tfalse(verifiedConnect("ipwrong", "127.0.0.1", NULL),
           "A certificate whose iPAddress SAN names another address must be refused");
}

/*
    Name based verification is unchanged.
 */
static void testDnsNameVerifiesByName(void)
{
    if (!rFileExists(CERT_DIR "/valid.crt")) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    ttrue(verifiedConnect("valid", "localhost", NULL), "A DNS name still verifies by name");
}

static void fiberMain(void *arg)
{
    (void) arg;
    testIpSanVerifies();
    testWrongIpSanRejected();
    testDnsNameVerifiesByName();
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
