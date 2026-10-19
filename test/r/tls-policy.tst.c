/*
    tls-policy.tst.c - Listener protocol floor, renegotiation and default cipher policy

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#define TLS_PORT 23070
#include "tls-common.h"

#if ME_COM_SSL && ME_COM_OPENSSL && ME_UNIX_LIKE
#include <openssl/ssl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

/*********************************** Locals ***********************************/

typedef struct Probe {
    int version;                        /* Protocol version to pin */
    cchar *cipher;                      /* TLS 1.2 cipher to pin, or null for any */
    int renegotiate;                    /* Attempt client-initiated renegotiation */
    int ok;                             /* Result: handshake (and renegotiation) completed */
    volatile int done;
} Probe;

/************************************ Code ************************************/

/*
    Blocking client handshake in a thread so the listener fiber can run
 */
static void *probeThread(void *arg)
{
    Probe              *probe;
    struct sockaddr_in sa;
    SSL_CTX            *ctx;
    SSL                *ssl;
    char               buf[16];
    int                fd;

    probe = arg;
    ctx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_security_level(ctx, 0);
    SSL_CTX_set_cipher_list(ctx, probe->cipher ? probe->cipher : "ALL:@SECLEVEL=0");
    SSL_CTX_set_min_proto_version(ctx, probe->version);
    SSL_CTX_set_max_proto_version(ctx, probe->version);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((ushort) listenPort);
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    probe->ok = 0;
    if (connect(fd, (struct sockaddr*) &sa, sizeof(sa)) == 0) {
        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, fd);
        probe->ok = SSL_connect(ssl) == 1;
        if (probe->ok && probe->renegotiate) {
            //  Success means the server completed a second handshake
            probe->ok = SSL_renegotiate(ssl) == 1 && SSL_do_handshake(ssl) == 1;
            if (probe->ok) {
                SSL_write(ssl, "x", 1);
                probe->ok = SSL_read(ssl, buf, sizeof(buf)) >= 0 && SSL_renegotiate_pending(ssl) == 0 &&
                    SSL_num_renegotiations(ssl) > 0;
            }
        } else if (probe->ok) {
            SSL_write(ssl, "x", 1);
        }
        SSL_free(ssl);
    }
    close(fd);
    SSL_CTX_free(ctx);
    probe->done = 1;
    return NULL;
}

static bool probeAt(int version, cchar *cipher, int renegotiate)
{
    pthread_t tid;
    Probe     probe;

    memset(&probe, 0, sizeof(probe));
    probe.version = version;
    probe.cipher = cipher;
    probe.renegotiate = renegotiate;
    pthread_create(&tid, NULL, probeThread, &probe);
    while (!probe.done) {
        rSleep(10);
    }
    pthread_join(tid, NULL);
    return probe.ok;
}

static void testProtocolFloor(void)
{
    if (!startServer("valid")) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    tfalse(probeAt(TLS1_VERSION, NULL, 0), "TLS 1.0 must be refused");
    tfalse(probeAt(TLS1_1_VERSION, NULL, 0), "TLS 1.1 must be refused");
    ttrue(probeAt(TLS1_2_VERSION, NULL, 0), "TLS 1.2 must be accepted");
    ttrue(probeAt(TLS1_3_VERSION, NULL, 0), "TLS 1.3 must be accepted");
    stopServer();
}

static void testRenegotiationRefused(void)
{
    if (!startServer("valid")) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    tfalse(probeAt(TLS1_2_VERSION, NULL, 1), "Client-initiated TLS 1.2 renegotiation must be refused");
    stopServer();
}

static void testDefaultCipherPolicy(void)
{
    if (!startServer("valid")) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    ttrue(probeAt(TLS1_2_VERSION, "ECDHE-RSA-AES128-GCM-SHA256", 0), "A forward secret AEAD suite is accepted");
    ttrue(probeAt(TLS1_2_VERSION, "ECDHE-RSA-CHACHA20-POLY1305", 0), "A forward secret ChaCha20 suite is accepted");
    tfalse(probeAt(TLS1_2_VERSION, "AES128-GCM-SHA256@SECLEVEL=0", 0),
           "A static RSA suite, which has no forward secrecy, must be refused by default");
    tfalse(probeAt(TLS1_2_VERSION, "ECDHE-RSA-AES128-SHA@SECLEVEL=0", 0), "A CBC-SHA1 suite must be refused by default");
    stopServer();
}


static void testConfiguredCiphersOverrideDefault(void)
{
    if (!startListener("valid", NULL, 0, NULL, "AES128-GCM-SHA256")) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    ttrue(probeAt(TLS1_2_VERSION, "AES128-GCM-SHA256@SECLEVEL=0", 0),
          "A cipher list configured on the listener must be honoured");
    stopServer();
}

static void fiberMain(void *arg)
{
    (void) arg;
    testProtocolFloor();
    testRenegotiationRefused();
    testDefaultCipherPolicy();
    testConfiguredCiphersOverrideDefault();
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

#elif ME_COM_SSL && ME_COM_MBEDTLS
#include <mbedtls/ssl.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/net_sockets.h>

/************************************ Code ************************************/

static int probeSend(void *ctx, cuchar *buf, size_t len)
{
    ssize rc;

    rc = rWriteSocket(ctx, buf, len, rGetTicks() + 2 * TPS);
    return rc < 0 ? MBEDTLS_ERR_NET_SEND_FAILED : (int) rc;
}

static int probeRecv(void *ctx, uchar *buf, size_t len)
{
    ssize rc;

    rc = rReadSocket(ctx, buf, len, rGetTicks() + 2 * TPS);
    return rc < 0 ? MBEDTLS_ERR_NET_RECV_FAILED : (int) rc;
}


static bool probeAt(mbedtls_ssl_protocol_version version, int suite, int renegotiate)
{
    mbedtls_ssl_config  conf;
    mbedtls_ssl_context ssl;
    RSocket             *sp;
    uchar               buf[16];
    int                 suites[2];
    bool                ok;

    suites[0] = suite;
    suites[1] = 0;
    mbedtls_ssl_config_init(&conf);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, rGetTlsRng());
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_min_tls_version(&conf, version);
    mbedtls_ssl_conf_max_tls_version(&conf, version);
    if (suite) {
        mbedtls_ssl_conf_ciphersuites(&conf, suites);
    }
#ifdef MBEDTLS_SSL_RENEGOTIATION
    mbedtls_ssl_conf_renegotiation(&conf, MBEDTLS_SSL_RENEGOTIATION_ENABLED);
#endif
    ok = 0;
    sp = rAllocSocket();
    if (rConnectSocket(sp, "127.0.0.1", listenPort, rGetTicks() + 5 * TPS) >= 0 &&
        mbedtls_ssl_setup(&ssl, &conf) == 0 && mbedtls_ssl_set_hostname(&ssl, TEST_HOST) == 0) {
        mbedtls_ssl_set_bio(&ssl, sp, probeSend, probeRecv, NULL);
        ok = mbedtls_ssl_handshake(&ssl) == 0;
        if (ok && renegotiate) {
#ifdef MBEDTLS_SSL_RENEGOTIATION
            //  Success means the server completed a second handshake
            ok = mbedtls_ssl_renegotiate(&ssl) == 0;
#else
            ok = 0;
#endif
        }
        if (ok) {
            ok = mbedtls_ssl_write(&ssl, (cuchar*) "x", 1) == 1 && mbedtls_ssl_read(&ssl, buf, sizeof(buf)) == 1;
        }
    }
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    rFreeSocket(sp);
    return ok;
}

static void testProtocolFloor(void)
{
    if (!startServer("valid")) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    ttrue(probeAt(MBEDTLS_SSL_VERSION_TLS1_2, 0, 0), "TLS 1.2 must be accepted");
    ttrue(probeAt(MBEDTLS_SSL_VERSION_TLS1_3, 0, 0), "TLS 1.3 must be accepted");
    stopServer();
}

static void testRenegotiationRefused(void)
{
    if (!startServer("valid")) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    tfalse(probeAt(MBEDTLS_SSL_VERSION_TLS1_2, 0, 1), "Client-initiated TLS 1.2 renegotiation must be refused");
    stopServer();
}

static void testDefaultCipherPolicy(void)
{
    if (!startServer("valid")) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    ttrue(probeAt(MBEDTLS_SSL_VERSION_TLS1_2, MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, 0),
          "A forward secret AEAD suite is accepted");
    ttrue(probeAt(MBEDTLS_SSL_VERSION_TLS1_2, MBEDTLS_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, 0),
          "A forward secret ChaCha20 suite is accepted");
    tfalse(probeAt(MBEDTLS_SSL_VERSION_TLS1_2, MBEDTLS_TLS_RSA_WITH_AES_128_GCM_SHA256, 0),
           "A static RSA suite, which has no forward secrecy, must be refused by default");
    tfalse(probeAt(MBEDTLS_SSL_VERSION_TLS1_2, MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA, 0),
           "A CBC-SHA1 suite must be refused by default");
    stopServer();
}


static void testConfiguredCiphersOverrideDefault(void)
{
    if (!startListener("valid", NULL, 0, NULL, "TLS_RSA_WITH_AES_128_GCM_SHA256")) {
        tinfo("Skipped: No valid test certificate");
        return;
    }
    ttrue(probeAt(MBEDTLS_SSL_VERSION_TLS1_2, MBEDTLS_TLS_RSA_WITH_AES_128_GCM_SHA256, 0),
          "A cipher list configured on the listener must be honoured");
    stopServer();
}

static void fiberMain(void *arg)
{
    (void) arg;
    testProtocolFloor();
    testRenegotiationRefused();
    testDefaultCipherPolicy();
    testConfiguredCiphersOverrideDefault();
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
