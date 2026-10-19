/*
    tls-config.tst.c - TLS listener configuration

    Client certificate verification, the configured cipher list and missing certificate files,
    each on an in-process HTTPS host so the shared server's TLS setup is untouched. Mutual TLS is
    a security boundary: a client without an acceptable certificate must not reach the document.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define CA      "../certs/ca.crt"
#define CERT    "../certs/test.crt"
#define KEY     "../certs/test.key"
#define SELF    "../certs/self.crt"
#define SELFKEY "../certs/self.key"
#define ROOTS   "../certs/roots.crt"

/************************************ Code ************************************/

/*
    Fetch /index.html from a local HTTPS port presenting the given client certificate, if any.
    Returns the status, negative when the handshake fails.
 */
static int fetchWith(int port, cchar *key, cchar *cert)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);
    urlSetCerts(up, CA, key, cert, NULL);
    urlSetTimeout(up, 5 * TPS);
    status = urlFetch(up, "GET", SFMT(url, "https://localhost:%d/index.html", port), NULL, 0, NULL);
    urlFree(up);
    return status;
}

/*
    With verify.client set, a client must present a certificate the authority signed
 */
static void testClientCertificateVerification(void)
{
    WebHost *host;

    host = startTestHost("{ tls: { authority: '" CA "', certificate: '" CERT "', key: '" KEY "', "
                         "verify: { client: true, issuer: true, authority: '" CA "' } }, "
                         "web: { documents: './site', routes: [{}], listen: ['https://:4853'] } }");
    if (!host) {
        return;
    }
    teqi(fetchWith(4853, KEY, CERT), 200, "A client certificate signed by the authority must be accepted");
    ttrue(fetchWith(4853, NULL, NULL) < 0, "A client without a certificate must be refused");
    ttrue(fetchWith(4853, SELFKEY, SELF) < 0, "A self-signed client certificate must be refused");
    stopTestHost(host);
}

/*
    Omitting "issuer" must inherit the runtime default, which verifies. It used to be read as a
    plain boolean, so an absent key became an explicit "do not verify the chain" and any
    self-signed certificate passed the only mutual-TLS gate the module has.
 */
static void testIssuerDefaultsToVerifying(void)
{
    WebHost *host;

    host = startTestHost("{ tls: { authority: '" CA "', certificate: '" CERT "', key: '" KEY "', "
                         "verify: { client: true, authority: '" CA "' } }, "
                         "web: { documents: './site', routes: [{}], listen: ['https://:4856'] } }");
    if (!host) {
        return;
    }
    teqi(fetchWith(4856, KEY, CERT), 200, "A client certificate signed by the authority must be accepted");
    ttrue(fetchWith(4856, SELFKEY, SELF) < 0,
          "With issuer omitted, a self-signed client certificate must still be refused");
    stopTestHost(host);
}

/*
    Requiring a client certificate that need not chain to anything is not a coherent request, so
    the listener refuses to come up rather than pretending to authenticate.
 */
static void testClientWithoutIssuerIsRefused(void)
{
    WebHost *host;

    host = startTestHost("{ tls: { authority: '" CA "', certificate: '" CERT "', key: '" KEY "', "
                         "verify: { client: true, issuer: false, authority: '" CA "' } }, "
                         "web: { documents: './site', routes: [{}], listen: ['https://:4857'] } }");
    ttrue(host == 0, "verify.client with verify.issuer false must be refused at startup");
    if (host) {
        stopTestHost(host);
    }
}

/* Verify listener client certificates against tls.verify.authority only, excluding outbound/system public roots. */
static void testClientAuthorityIsItsOwn(void)
{
    WebHost *host;

    host = startTestHost("{ tls: { authority: '" ROOTS "', certificate: '" CERT "', key: '" KEY "', "
                         "verify: { client: true, authority: '" CA "' } }, "
                         "web: { documents: './site', routes: [{}], listen: ['https://:4884'] } }");
    if (host) {
        teqi(fetchWith(4884, KEY, CERT), 200, "A client certificate signed by verify.authority must be accepted");
        stopTestHost(host);
    }
    host = startTestHost("{ tls: { authority: '" CA "', certificate: '" CERT "', key: '" KEY "', "
                         "verify: { client: true, authority: '" SELF "' } }, "
                         "web: { documents: './site', routes: [{}], listen: ['https://:4887'] } }");
    if (host) {
        ttrue(fetchWith(4887, KEY, CERT) < 0, "A client certificate signed only by tls.authority must be refused");
        stopTestHost(host);
    } else {
        tfail("Cannot start the listener with a distinct verify.authority");
    }
}

/*
    verify.client without verify.authority has no CA to verify against, so it must not start
 */
static void testClientWithoutAuthorityIsRefused(void)
{
    WebHost *host;

    host = startTestHost("{ tls: { authority: '" CA "', certificate: '" CERT "', key: '" KEY "', "
                         "verify: { client: true } }, "
                         "web: { documents: './site', routes: [{}], listen: ['https://:4898'] } }");
    ttrue(host == 0, "verify.client without verify.authority must be refused at startup");
    if (host) {
        stopTestHost(host);
    }
}

/*
    Without verify.client, no certificate is required
 */
static void testClientCertificateNotRequiredByDefault(void)
{
    WebHost *host;

    host = startTestHost("{ tls: { authority: '" CA "', certificate: '" CERT "', key: '" KEY "' }, "
                         "web: { documents: './site', routes: [{}], listen: ['https://:4854'] } }");
    if (!host) {
        return;
    }
    teqi(fetchWith(4854, NULL, NULL), 200);
    stopTestHost(host);
}

/*
    A configured cipher list is applied to the listener and a compatible client still connects
 */
static void testConfiguredCiphers(void)
{
    WebHost *host;

    host = startTestHost("{ tls: { authority: '" CA "', certificate: '" CERT "', key: '" KEY "', "
                         "ciphers: ['TLS_AES_256_GCM_SHA384', 'TLS_AES_128_GCM_SHA256', "
                         "'ECDHE-RSA-AES256-GCM-SHA384', 'ECDHE-RSA-AES128-GCM-SHA256'] }, "
                         "web: { documents: './site', routes: [{}], listen: ['https://:4855'] } }");
    if (!host) {
        return;
    }
    teqi(fetchWith(4855, NULL, NULL), 200, "A configured cipher list must still admit a standard client");
    stopTestHost(host);
}


static void testMissingCertificateFilesRefuseToStart(void)
{
    WebHost *host;
    Json    *json;
    cchar   *configs[] = {
        "{ tls: { authority: '" CA "', certificate: '../certs/missing.crt', key: '" KEY "' }, "
        "web: { documents: './site', routes: [{}], listen: ['https://:4856'] } }",
        "{ tls: { authority: '" CA "', certificate: '" CERT "', key: '../certs/missing.key' }, "
        "web: { documents: './site', routes: [{}], listen: ['https://:4856'] } }",
        "{ tls: { certificate: '" CERT "', key: '" KEY "', "
        "verify: { client: true, authority: '../certs/missing-ca.crt' } }, "
        "web: { documents: './site', routes: [{}], listen: ['https://:4856'] } }",
        "{ tls: { authority: '" CA "', key: '" KEY "' }, "
        "web: { documents: './site', routes: [{}], listen: ['https://:4856'] } }",
        "{ tls: { authority: '" CA "', certificate: '" CERT "' }, "
        "web: { documents: './site', routes: [{}], listen: ['https://:4856'] } }",
        "{ web: { documents: './site', routes: [{}], listen: ['https://:4856'] } }",
    };
    int i;

    for (i = 0; i < (int) (sizeof(configs) / sizeof(configs[0])); i++) {
        json = jsonParse(configs[i], 0);
        host = webAllocHost(json, 0);
        tnotnull(host);
        if (host) {
            ttrue(webStartHost(host) < 0, "Config %d must refuse to start", i);
            webStopHost(host);
            webFreeHost(host);
        }
        jsonFree(json);
    }
}

static void fiberMain(void *arg)
{
    if (setup(NULL, NULL)) {
        testClientCertificateVerification();
        testIssuerDefaultsToVerifying();
        testClientWithoutIssuerIsRefused();
        testClientAuthorityIsItsOwn();
        testClientWithoutAuthorityIsRefused();
        testClientCertificateNotRequiredByDefault();
        testConfiguredCiphers();
        testMissingCertificateFilesRefuseToStart();
    }
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
