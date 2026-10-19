/*
    tls-verify.tst.c - TLS peer certificate verification

    The peer verification callback must fail closed. It starts from the verdict the TLS stack
    supplies and may only relax it for errors the configuration explicitly waives. Any other
    error - expired, not-yet-valid, revoked, bad signature, hostname mismatch, chain too long -
    must abort the handshake.

    Each case stands up a local TLS listener presenting one certificate and connects a client
    that trusts the shared test CA, then asserts whether the handshake completes.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "testme.h"
#include "r.h"

#if ME_COM_SSL

#if ME_COM_OPENSSL
    #include <openssl/ssl.h>
#elif ME_COM_MBEDTLS
    #include <mbedtls/ssl.h>
#endif

/*********************************** Locals ***********************************/

#define CERT_DIR     ".certs"
#define CA_CERT      "../certs/ca.crt"
#define TEST_HOST    "localhost"

static RSocket *listenSock;
static RFiber  *mainFiber;
static int      listenPort;
static int      accepted;

/************************************ Code ************************************/

/*
    Server side. The handshake may legitimately fail here when the client rejects the
    certificate, so nothing is asserted about the outcome.

    This must NOT resume the main fiber. It used to, and the resume was queued rather than
    immediate, so it landed while the main fiber was inside the *next* case's rConnectSocket and
    woke it early. Every case that followed an accepted one then failed with "Cannot connect
    socket" instead of a verification verdict - which the assertions could not tell apart,
    because they only checked that some reason had been recorded. The server fiber is drained by
    the settle in runCase instead.
 */
static void acceptFn(void *arg, RSocket *sp)
{
    char buf[128];

    accepted++;
    rReadSocket(sp, buf, sizeof(buf), rGetTicks() + 2 * TPS);
    rFreeSocket(sp);
}

/*
    Start a TLS listener presenting the named certificate. Returns 0 on failure.
 */
static RSocket *startServer(cchar *name)
{
    RSocket *sp;
    char    cert[256], key[256];
    int     port;

    SFMT(cert, "%s/%s.crt", CERT_DIR, name);
    SFMT(key, "%s/%s.key", CERT_DIR, name);

    if (!rFileExists(cert) || !rFileExists(key)) {
        return 0;
    }
    if ((sp = rAllocSocket()) == 0) {
        return 0;
    }
    rSetSocketCerts(sp, NULL, key, cert, NULL);
    rSetSocketVerify(sp, 0, 0);

    for (port = 19300; port < 19400; port++) {
        if (rListenSocket(sp, TEST_HOST, port, (RSocketProc) acceptFn, NULL) == 0) {
            listenPort = port;
            return sp;
        }
    }
    rFreeSocket(sp);
    return 0;
}

/*
    Connect a verifying client and report whether the handshake completed.
    Sets *errorMsg to the socket error text when it did not.
 */
static bool connectAndVerify(int verifyIssuer, cchar *revokeFile, cchar **errorMsg)
{
    RSocket *sp;
    bool     connected;
    int      rc;

    sp = rAllocSocket();
    rSetSocketCerts(sp, CA_CERT, NULL, NULL, revokeFile);
    rSetSocketVerify(sp, 1, verifyIssuer);
    rSetTls(sp);

    rc = rConnectSocket(sp, TEST_HOST, listenPort, rGetTicks() + 5 * TPS);
    connected = (rc >= 0 && rIsSocketSecure(sp));

    if (errorMsg) {
        *errorMsg = connected ? NULL : sclone(rGetSocketError(sp));
    }
    if (rc >= 0) {
        //  Give the server fiber a chance to finish so the next case starts clean
        rWriteSocket(sp, "x", 1, rGetTicks() + TPS);
    }
    rFreeSocket(sp);
    return connected;
}

static void stopServer(void)
{
    if (listenSock) {
        rFreeSocket(listenSock);
        listenSock = 0;
    }
}

/*
    Run one case: present "name" and report whether the client accepted it.
    Returns -1 when the certificate is unavailable so the caller can skip.

    "reason" is a fragment that the rejection message must contain, and it is the point of this
    harness: a handshake that fails for a transport reason is indistinguishable from one the
    verifier refused unless the reason is checked. Pass NULL for a case expected to succeed.
 */
static int runCase(cchar *name, int verifyIssuer, cchar *revokeFile, cchar *reason)
{
    cchar *errorMsg;
    bool  connected;

    if (revokeFile && !rFileExists(revokeFile)) {
        tinfo("Skipped: No revocation list at %s", revokeFile);
        return -1;
    }
    if ((listenSock = startServer(name)) == 0) {
        tinfo("Skipped: No test certificate for %s", name);
        return -1;
    }
    errorMsg = NULL;
    connected = connectAndVerify(verifyIssuer, revokeFile, &errorMsg);

    if (!connected && reason) {
        ttrue(errorMsg != NULL && scontains(errorMsg, reason) != NULL,
              "Rejection must name the reason, not merely fail: wanted \"%s\", got \"%s\"",
              reason, errorMsg ? errorMsg : "(none)");
    }
    rFree((char*) errorMsg);

    /*
        Let the server fiber finish its read and release the accepted socket before the listener
        is freed and the next case rebinds the port.
     */
    rSleep(20);
    stopServer();
    rSleep(20);
    return connected ? 1 : 0;
}

/*
    A certificate that is valid in every respect must be accepted. Without this the suite
    would pass trivially by rejecting everything.
 */
static void testValidCertificateAccepted(void)
{
    int rc = runCase("valid", 1, NULL, NULL);
    if (rc >= 0) {
        teqi(rc, 1, "A valid CA-signed certificate for the requested host must be accepted");
    }
}

/*
    Trust only the root so verification requires the server's intermediate certificate.
 */
static void testCertificateChainPresented(void)
{
    int rc = runCase("chain", 1, NULL, NULL);
    if (rc >= 0) {
        teqi(rc, 1, "A server presenting leaf + intermediate must let a root-only client verify");
    }
}

/*
    Exceed the runtime depth limit without certificate path-length constraints.
 */
static void testChainTooDeepRejected(void)
{
    int rc = runCase("deepchain", 1, NULL, "too long");
    if (rc >= 0) {
        teqi(rc, 0, "A certificate chain deeper than the configured maximum depth must abort the handshake");
    }
}

/*
    The core of the fail-open defect: an expired certificate reached the default arm of the
    verification switch and was accepted.
 */
static void testExpiredCertificateRejected(void)
{
    int rc = runCase("expired", 1, NULL, "expired");
    if (rc >= 0) {
        teqi(rc, 0, "An expired certificate must abort the handshake");
    }
}

static void testNotYetValidCertificateRejected(void)
{
    int rc = runCase("notyet", 1, NULL, "not yet valid");
    if (rc >= 0) {
        teqi(rc, 0, "A not-yet-valid certificate must abort the handshake");
    }
}

/*
    X509_VERIFY_PARAM_set1_host is configured for the peer name, so a mismatch must be fatal.
    The callback previously discarded that result.
 */
static void testHostnameMismatchRejected(void)
{
    int rc = runCase("wronghost", 1, NULL, "hostname mismatch");
    if (rc >= 0) {
        teqi(rc, 0, "A certificate whose name does not match the peer must abort the handshake");
    }
}

/*
    A self-signed certificate has no path to the trusted CA. It is the one class of failure
    the configuration may waive, and only when issuer verification is explicitly disabled.
 */
static void testSelfSignedRejectedWhenVerifyingIssuer(void)
{
    int rc = runCase("selfsigned", 1, NULL, "issuer not trusted");
    if (rc >= 0) {
        teqi(rc, 0, "A self-signed certificate must be rejected when verifying the issuer");
    }
}

static void testSelfSignedAcceptedWhenIssuerVerifyDisabled(void)
{
    int rc = runCase("selfsigned", 0, NULL, NULL);
    if (rc >= 0) {
        teqi(rc, 1, "A self-signed certificate is accepted when issuer verification is disabled");
    }
}

/*
    Disabling issuer verification waives only the issuer. It must not waive expiry.
    This is the distinction between relaxing a verdict and discarding it.
 */
static void testExpiredRejectedEvenWithoutIssuerVerify(void)
{
    int rc = runCase("expired", 0, NULL, "expired");
    if (rc >= 0) {
        teqi(rc, 0, "An expired certificate must still be rejected when issuer verification is disabled");
    }
}

/*
    Likewise a hostname mismatch is not an issuer problem and must survive the waiver.
 */
static void testHostnameMismatchRejectedWithoutIssuerVerify(void)
{
    int rc = runCase("wronghost", 0, NULL, "hostname mismatch");
    if (rc >= 0) {
        teqi(rc, 0, "A hostname mismatch must still be rejected when issuer verification is disabled");
    }
}

/*
    Certificate revocation.

    8c4cda9: the revocation list was loaded into the X509 store but X509_V_FLAG_CRL_CHECK was
    never set. OpenSSL's check_revocation() early-returns success without that flag, so the list
    was never opened and a revoked certificate was accepted on every handshake - silently,
    because an operator who configures a CRL has no indication it is inert.

    The three cases below are the whole argument, and only all three together:

      - a revoked certificate with the list configured is refused;
      - a certificate from the same CA that is NOT on the list is still accepted, so the first
        result is revocation and not a CRL that simply breaks every handshake;
      - a revoked certificate with no list configured is accepted, so the first result comes
        from consulting the list rather than from something else about that certificate.

    Only the second of these fails if the flag is dropped again - but on its own it would be
    indistinguishable from a misconfiguration, which is why the other two are here.
 */
#define REVOKED_CRL  CERT_DIR "/revoked.crl"

static void testRevokedCertificateRejected(void)
{
    int rc = runCase("revoked", 1, REVOKED_CRL, "revoked");

    if (rc >= 0) {
        teqi(rc, 0, "A certificate named on the configured CRL must abort the handshake");
    }
}

static void testUnrevokedCertificateAcceptedWithCrl(void)
{
    int rc = runCase("notrevoked", 1, REVOKED_CRL, NULL);

    if (rc >= 0) {
        teqi(rc, 1, "A certificate not on the CRL must still be accepted with a CRL configured");
    }
}

/*
    Without a list there is nothing to check against, so the certificate is accepted on its own
    merits. This is the pre-fix behaviour for every client, and it is what makes the difference
    between the two results above attributable to the CRL.
 */
static void testRevokedCertificateAcceptedWithoutCrl(void)
{
    int rc = runCase("revoked", 1, NULL, NULL);

    if (rc >= 0) {
        teqi(rc, 1, "With no CRL configured a revoked certificate is accepted on its own merits");
    }
}

/*
    An empty list is a configured list. It must be honoured - and must not reject everything,
    which is the failure mode of requiring a CRL for each link in the chain.
 */
static void testEmptyCrlAcceptsValidCertificate(void)
{
    int rc = runCase("notrevoked", 1, CERT_DIR "/empty.crl", NULL);

    if (rc >= 0) {
        teqi(rc, 1, "An empty CRL must not reject a certificate that is not on it");
    }
}

/*
    Cipher selection. The list is only applied when the context is built, which happens on
    connect, so a bad list surfaces as a failed handshake rather than a failed setter.
 */
static void testCipherSelection(void)
{
    RSocket *sp;
    bool    connected;
    int     rc;

    if ((listenSock = startServer("valid")) == 0) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    /*
        A suite the peer offers.
     */
    sp = rAllocSocket();
    rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    rSetSocketVerify(sp, 1, 1);
    rSetSocketCiphers(sp, "TLS_AES_256_GCM_SHA384");
    rSetTls(sp);
    rc = rConnectSocket(sp, TEST_HOST, listenPort, rGetTicks() + 5 * TPS);
    connected = (rc >= 0 && rIsSocketSecure(sp));
    ttrue(connected, "A cipher suite the peer supports must complete the handshake");
    if (rc >= 0) {
        rWriteSocket(sp, "x", 1, rGetTicks() + TPS);
    }
    rFreeSocket(sp);
    rSleep(20);
    stopServer();
    rSleep(20);

    /*
        A list that names nothing real. The context cannot be configured, so the connection
        fails rather than quietly falling back to a suite the caller did not ask for.
     */
    if ((listenSock = startServer("valid")) == 0) {
        return;
    }
    sp = rAllocSocket();
    rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    rSetSocketVerify(sp, 1, 1);
    rSetSocketCiphers(sp, "NO_SUCH_CIPHER_SUITE");
    rSetTls(sp);
    rc = rConnectSocket(sp, TEST_HOST, listenPort, rGetTicks() + 5 * TPS);
    connected = (rc >= 0 && rIsSocketSecure(sp));
    tfalse(connected, "An unusable cipher list must fail rather than silently fall back");
    rFreeSocket(sp);
    rSleep(20);
    stopServer();
    rSleep(20);
}

/*
    Verify mixed TLS 1.2/1.3 cipher lists through the negotiated suite. Request ChaCha20 to distinguish configured
    TLS 1.3 from the AES-256-GCM default.
 */
static void testCipherBothGenerationsApplied(void)
{
    RSocket *sp;
    bool    connected;
    int     rc;

    if ((listenSock = startServer("valid")) == 0) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    sp = rAllocSocket();
    rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    rSetSocketVerify(sp, 1, 1);
    rSetSocketCiphers(sp, "TLS_CHACHA20_POLY1305_SHA256,ECDHE-RSA-AES256-GCM-SHA384");
    rSetTls(sp);
    rc = rConnectSocket(sp, TEST_HOST, listenPort, rGetTicks() + 5 * TPS);
    connected = (rc >= 0 && rIsSocketSecure(sp));
    ttrue(connected, "A cipher list naming both a TLS1.3 suite and a TLS1.2 cipher must connect");
#if ME_COM_OPENSSL
    if (connected) {
        SSL_SESSION *session;

        if ((session = (SSL_SESSION*) rGetTlsSession(sp)) != 0) {
            teqi(scmp(SSL_CIPHER_get_name(SSL_SESSION_get0_cipher(session)), "TLS_CHACHA20_POLY1305_SHA256"), 0,
                 "The configured TLS 1.3 suite must be the one negotiated");
            rFreeTlsSession(session);
        }
    }
#elif ME_COM_MBEDTLS
    if (connected) {
        mbedtls_ssl_session *session;

        if ((session = (mbedtls_ssl_session*) rGetTlsSession(sp)) != 0) {
            teqi(mbedtls_ssl_session_get_ciphersuite_id(session), MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256,
                 "The configured TLS 1.3 suite must be the one negotiated");
            rFreeTlsSession(session);
        }
    }
#endif
    if (rc >= 0) {
        rWriteSocket(sp, "x", 1, rGetTicks() + TPS);
    }
    rFreeSocket(sp);
    rSleep(20);
    stopServer();
    rSleep(20);
}

/*
    Session resumption. The session taken from a completed handshake is handed to a second
    socket, which must still verify the peer - a resumed session is not a waived one.
 */
static void testSessionReuse(void)
{
    RSocket *sp;
    void    *session;
    bool    connected;
    int     rc;

    if ((listenSock = startServer("valid")) == 0) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    sp = rAllocSocket();
    rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    rSetSocketVerify(sp, 1, 1);
    rSetTls(sp);
    rc = rConnectSocket(sp, TEST_HOST, listenPort, rGetTicks() + 5 * TPS);
    ttrue(rc >= 0);

    /*
        A session exists only once a handshake has completed.
     */
    session = rGetTlsSession(sp);
    tnotnull(session);

    if (rc >= 0) {
        rWriteSocket(sp, "x", 1, rGetTicks() + TPS);
    }
    rFreeSocket(sp);
    rSleep(20);
    stopServer();
    rSleep(20);

    /*
        Offer it on a fresh connection. Whether the peer accepts the resumption is its choice;
        what must hold is that the handshake still completes and still verifies.
     */
    if ((listenSock = startServer("valid")) == 0) {
        rFreeTlsSession(session);
        return;
    }
    sp = rAllocSocket();
    rSetSocketCerts(sp, CA_CERT, NULL, NULL, NULL);
    rSetSocketVerify(sp, 1, 1);
    rSetTls(sp);
    rSetTlsSession(sp, session);
    rc = rConnectSocket(sp, TEST_HOST, listenPort, rGetTicks() + 5 * TPS);
    connected = (rc >= 0 && rIsSocketSecure(sp));
    ttrue(connected, "A socket offering a prior session must still complete a verified handshake");
    if (rc >= 0) {
        rWriteSocket(sp, "x", 1, rGetTicks() + TPS);
    }
    rFreeSocket(sp);
    rSleep(20);
    stopServer();
    rSleep(20);

    rFreeTlsSession(session);

    /*
        The accessors decline rather than crash when there is no session to give.
     */
    sp = rAllocSocket();
    session = rGetTlsSession(sp);
    tnull(session);
    rSetTlsSession(sp, NULL);
    rFreeSocket(sp);
    rFreeTlsSession(NULL);
}


/*
    The outbound default must be "verify".

    A client that sets no explicit policy inherits the process-wide default, and that default is
    what a server-side listener configuration must never touch. web's webSecureEndpoint used to
    route "tls.verify.client" - a mutual-TLS server setting, false by default - into
    rSetTlsDefaultVerify, which disabled certificate verification on every outbound connection the
    process made. Configuring https therefore disarmed the TLS client.

    This asserts the invariant directly: with no per-socket policy, an expired certificate is
    still refused. If anything clobbers the default again, this fails.
 */
static void testDefaultVerifyRejectsExpired(void)
{
    RSocket *sp;
    int      rc;

    if ((listenSock = startServer("expired")) == 0) {
        tinfo("Skipped: No expired test certificate");
        return;
    }
    rSetSocketDefaultCerts(CA_CERT, 0, 0, 0);

    sp = rAllocSocket();
    //  Deliberately NO rSetSocketVerify - this socket must inherit the default
    rSetTls(sp);
    rc = rConnectSocket(sp, TEST_HOST, listenPort, rGetTicks() + 5 * TPS);

    tfalse(rc >= 0 && rIsSocketSecure(sp),
           "A client with no explicit policy must still reject an expired certificate");
    rFreeSocket(sp);
    stopServer();
}

static void fiberMain(void *arg)
{
    mainFiber = rGetFiber();

    testValidCertificateAccepted();
    testCertificateChainPresented();
    testChainTooDeepRejected();
    testExpiredCertificateRejected();
    testNotYetValidCertificateRejected();
    testHostnameMismatchRejected();
    testSelfSignedRejectedWhenVerifyingIssuer();
    testSelfSignedAcceptedWhenIssuerVerifyDisabled();
    testExpiredRejectedEvenWithoutIssuerVerify();
    testHostnameMismatchRejectedWithoutIssuerVerify();

    testRevokedCertificateRejected();
    testUnrevokedCertificateAcceptedWithCrl();
    testRevokedCertificateAcceptedWithoutCrl();
    testEmptyCrlAcceptsValidCertificate();

    testCipherSelection();
    testCipherBothGenerationsApplied();
    testSessionReuse();

    testDefaultVerifyRejectsExpired();

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
#endif /* ME_COM_SSL */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
