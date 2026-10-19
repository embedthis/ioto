/*
    tls.tst.c - Unit tests for HTTPS connections, certificate verification and TLS settings

    Nothing else in the suite makes an HTTPS request. The test server listens on the second
    endpoint with a private CA and requires a client certificate, which makes every case here
    observable: the wrong trust anchor is refused, the right one is accepted, and the verify
    switch demonstrably controls the outcome.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define CA      "../certs/ca.crt"
#define KEY     "../certs/test.key"
#define CERT    "../certs/test.crt"
#define ROOTS   "../certs/roots.crt"

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static int get(Url *up, cchar *base)
{
    Ticks start;
    char  url[128];
    int   status;

    start = rGetTicks();
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", base), NULL, 0, NULL);
    tinfo("GET %s -> %d in %lld ms%s%s", base, status, (long long) (rGetTicks() - start),
          urlGetError(up) ? ": " : "", urlGetError(up) ? urlGetError(up) : "");
    return status;
}

/*
    The private CA plus a client certificate is the configuration the server requires
 */
static void testHandshake()
{
    Url *up;

    up = urlAlloc(0);
    urlSetCerts(up, CA, KEY, CERT, NULL);
    urlSetVerify(up, 1, 1);
    ttrue(get(up, HTTPS) == 200);
    tcontains(urlGetResponse(up), "Hello /index.html");
    ttrue(urlGetError(up) == NULL);
    urlFree(up);
}

/*
    The server requires a client certificate, so trusting its CA is not enough on its own
 */
static void testClientCertRequired()
{
    Url *up;

    up = urlAlloc(0);
    urlSetCerts(up, CA, NULL, NULL, NULL);
    ttrue(get(up, HTTPS) < 0);
    ttrue(urlGetError(up) != NULL);
    urlFree(up);
}

/*
    A server certificate from a CA we do not trust must be refused, and must say so
 */
static void testIssuerNotTrusted()
{
    Url *up;

    //  The runtime default trust store is the public roots, which do not include the test CA
    up = urlAlloc(0);
    ttrue(get(up, HTTPS) < 0);
    tcontains(urlGetError(up), "not trusted");
    urlFree(up);

    //  An explicit wrong CA is refused the same way, client certificate or not
    up = urlAlloc(0);
    urlSetCerts(up, ROOTS, KEY, CERT, NULL);
    ttrue(get(up, HTTPS) < 0);
    tcontains(urlGetError(up), "not trusted");
    urlFree(up);
}

/*
    Turning verification off must actually turn it off: the same wrong CA is then accepted
 */
static void testVerifyOff()
{
    Url *up;

    up = urlAlloc(0);
    urlSetCerts(up, ROOTS, KEY, CERT, NULL);
    urlSetVerify(up, 0, 0);
    ttrue(get(up, HTTPS) == 200);
    urlFree(up);
}

/*
    Cipher selection: a list nothing can negotiate fails cleanly, a restrictive valid list works
 */
static void testCiphers()
{
    Url *up;

    up = urlAlloc(0);
    urlSetCerts(up, CA, KEY, CERT, NULL);
    urlSetCiphers(up, "NO-SUCH-CIPHER");
    ttrue(get(up, HTTPS) < 0);
    tcontains(urlGetError(up), "ciphers");
    urlFree(up);

    up = urlAlloc(0);
    urlSetCerts(up, CA, KEY, CERT, NULL);
    urlSetCiphers(up, "ECDHE-RSA-AES256-GCM-SHA384:TLS_AES_256_GCM_SHA384");
    ttrue(get(up, HTTPS) == 200);
    urlFree(up);
}

/*
    Plain HTTP to the TLS port must fail cleanly. The converse, TLS to the plain port, is not
    tested here: the server holds the unparseable ClientHello for its full header-parse timeout
    and the client waits for it, which takes a minute and is tracked as its own issue.
 */
static void testSchemeMismatch()
{
    Url  *up;
    char url[128];

    up = urlAlloc(0);
    SFMT(url, "http%s", &HTTPS[5]);
    ttrue(get(up, url) < 0);
    ttrue(urlGetError(up) != NULL);
    urlFree(up);
}

/*
    Settings belong to the Url, not to the socket it happens to hold. urlClose replaces the
    socket, so certs given once must still apply afterwards.
 */
static void testSettingsSurviveClose()
{
    Url *up;

    up = urlAlloc(0);
    urlSetCerts(up, CA, KEY, CERT, NULL);
    ttrue(get(up, HTTPS) == 200);
    urlClose(up);
    ttrue(get(up, HTTPS) == 200);
    urlClose(up);
    ttrue(get(up, HTTPS) == 200);
    ttrue(urlGetError(up) == NULL);
    urlFree(up);
}

/*
    A scheme change replaces the socket too. Alternating http and https on one object must keep
    working in both directions.
 */
static void testSettingsSurviveSchemeChange()
{
    Url *up;

    up = urlAlloc(0);
    urlSetCerts(up, CA, KEY, CERT, NULL);
    ttrue(get(up, HTTP) == 200);
    ttrue(get(up, HTTPS) == 200);
    ttrue(get(up, HTTP) == 200);
    ttrue(get(up, HTTPS) == 200);
    ttrue(urlGetError(up) == NULL);
    urlFree(up);
}

/*
    The verify switch must survive a reconnect as well. If it silently reverted to the default,
    the third fetch here would fail verification against the wrong CA.
 */
static void testVerifyOffSurvivesReconnect()
{
    Url *up;

    up = urlAlloc(0);
    urlSetCerts(up, ROOTS, KEY, CERT, NULL);
    urlSetVerify(up, 0, 0);
    ttrue(get(up, HTTPS) == 200);
    ttrue(get(up, HTTP) == 200);
    ttrue(get(up, HTTPS) == 200);
    urlFree(up);
}

/*
    A second request on the same object reuses the TLS connection rather than handshaking again.
    The body must be consumed first: the client will not reuse a connection with an unread body,
    and over TLS the body arrives in a record of its own rather than alongside the headers.
 */
static void testTlsKeepAlive()
{
    Url *up;

    up = urlAlloc(0);
    urlSetCerts(up, CA, KEY, CERT, NULL);
    ttrue(get(up, HTTPS) == 200);
    tcontains(urlGetResponse(up), "Hello /index.html");
    ttrue(get(up, HTTPS) == 200);
    ttrue(up->reused);
    tcontains(urlGetResponse(up), "Hello /index.html");
    ttrue(get(up, HTTPS) == 200);
    ttrue(up->reused);
    urlFree(up);
}

#if ME_COM_WEBSOCK
static int wsMessages;

static void wsCallback(WebSocket *ws, int event, cchar *data, size_t len, void *arg)
{
    if (event == WS_EVENT_OPEN) {
        webSocketSend(ws, "over tls");
    } else if (event == WS_EVENT_MESSAGE) {
        wsMessages++;
        ttrue(len == slen("over tls"));
        ttrue(sncmp(data, "over tls", len) == 0);
        webSocketSendClose(ws, WS_STATUS_OK, "OK");
    }
}

/*
    A WebSocket upgrade over TLS. The high level urlWebSocket allocates its own Url and so cannot
    carry certificates, so this drives the low level path.
 */
static void testWebSocketOverTls()
{
    Url  *up;
    char url[128];

    wsMessages = 0;
    up = urlAlloc(0);
    urlSetCerts(up, CA, KEY, CERT, NULL);
    ttrue(urlStart(up, "GET", SFMT(url, "wss%s/test/ws", &HTTPS[5])) == 0);
    ttrue(urlFinalize(up) == 0);
    ttrue(urlGetStatus(up) == 101);
    ttrue(webSocketRun(urlGetWebSocket(up), (WebSocketProc) wsCallback, up, up->rx, up->timeout) >= 0);
    ttrue(wsMessages == 1);
    urlFree(up);
}
#endif /* ME_COM_WEBSOCK */

/*
    Without a configured auth type a Basic challenge is answered only over TLS
 */
static void testBasicAutoOverTls()
{
    Url  *up;
    char url[128];

    up = urlAlloc(0);
    urlSetCerts(up, CA, KEY, CERT, NULL);
    urlSetAuth(up, "bob", "password", NULL);
    ttrue(urlFetch(up, "GET", SFMT(url, "%s/basic/secret.html", HTTPS), NULL, 0, NULL) == 200);
    tcontains(urlGetResponse(up), "Basic Authentication Success");
    ttrue(up->offeredBasic == 1);
    ttrue(up->authType == NULL);
    urlFree(up);
}

static void testFiber()
{
    if (setup(&HTTP, &HTTPS)) {
        testHandshake();
        testClientCertRequired();
        testIssuerNotTrusted();
        testVerifyOff();
        testCiphers();
        testSchemeMismatch();
        testSettingsSurviveClose();
        testSettingsSurviveSchemeChange();
        testVerifyOffSurvivesReconnect();
        testTlsKeepAlive();
        testBasicAutoOverTls();
#if ME_COM_WEBSOCK
        testWebSocketOverTls();
#endif
    }
    rFree(HTTP);
    rFree(HTTPS);
    rStop();
}

int main(void)
{
    rInit(0, 0);
    rSpawnFiber("test", (RFiberProc) testFiber, NULL);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
