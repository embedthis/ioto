/*
    websock.tst.c - Unit tests for the WebSocket client upgrade

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

#if ME_COM_WEBSOCK

/*********************************** Locals ***********************************/

#define WS_MESSAGES 5

static char *HTTP;
static char *HTTPS;
static int  opened;
static int  messages;
static int  closed;

/************************************ Code ************************************/

/*
    Echo client. The test server at /test/ws echoes back whatever it receives.
 */
static void echoCallback(WebSocket *ws, int event, cchar *data, size_t len, void *arg)
{
    char expected[64];

    if (event == WS_EVENT_OPEN) {
        opened++;
        webSocketSend(ws, "Message %d", messages);

    } else if (event == WS_EVENT_MESSAGE) {
        SFMT(expected, "Message %d", messages);
        ttrue(len == slen(expected));
        ttrue(sncmp(data, expected, len) == 0);
        if (++messages < WS_MESSAGES) {
            webSocketSend(ws, "Message %d", messages);
        } else {
            webSocketSendClose(ws, WS_STATUS_OK, "OK");
        }

    } else if (event == WS_EVENT_CLOSE) {
        closed++;
    }
}

/*
    High level urlWebSocket API over ws://
 */
static void testWebSocketEcho()
{
    char url[128];
    int  rc;

    opened = messages = closed = 0;
    rc = urlWebSocket(SFMT(url, "ws%s/test/ws", &HTTP[4]), (WebSocketProc) echoCallback, NULL, NULL);
    ttrue(rc == 0);
    ttrue(opened == 1);
    ttrue(messages == WS_MESSAGES);
    ttrue(closed == 1);
}

/*
    Low level upgrade using urlStart/urlFinalize and webSocketRun.
    Verifies the handshake status, the upgrade headers and urlGetWebSocket.
 */
static void testWebSocketLowLevel()
{
    Url       *up;
    WebSocket *ws;
    char      url[128];
    int       rc;

    opened = messages = closed = 0;
    up = urlAlloc(0);

    rc = urlStart(up, "GET", SFMT(url, "ws%s/test/ws", &HTTP[4]));
    ttrue(rc == 0);

    rc = urlFinalize(up);
    ttrue(rc == 0);

    //  A successful upgrade is a 101 with the Upgrade/Connection headers echoed back
    ttrue(urlGetStatus(up) == 101);
    ttrue(scaselessmatch(urlGetHeader(up, "upgrade"), "WebSocket"));
    ttrue(scaselessmatch(urlGetHeader(up, "connection"), "Upgrade"));
    ttrue(urlGetHeader(up, "sec-websocket-accept") != NULL);

    ws = urlGetWebSocket(up);
    ttrue(ws != NULL);

    rc = webSocketRun(ws, (WebSocketProc) echoCallback, up, up->rx, up->timeout);
    ttrue(rc >= 0);
    ttrue(opened == 1);
    ttrue(messages == WS_MESSAGES);

    urlFree(up);
}

/*
    Binary messages exercise the same path with a non-text opcode
 */
static void binaryCallback(WebSocket *ws, int event, cchar *data, size_t len, void *arg)
{
    static char block[1024];

    if (event == WS_EVENT_OPEN) {
        memset(block, 'B', sizeof(block));
        webSocketSendBlock(ws, WS_MSG_BINARY, block, sizeof(block));

    } else if (event == WS_EVENT_MESSAGE) {
        messages++;
        ttrue(len == sizeof(block));
        ttrue(data && memcmp(data, block, len) == 0);
        webSocketSendClose(ws, WS_STATUS_OK, "OK");
    }
}

static void testWebSocketBinary()
{
    char url[128];
    int  rc;

    messages = 0;
    rc = urlWebSocket(SFMT(url, "ws%s/test/ws", &HTTP[4]), (WebSocketProc) binaryCallback, NULL, NULL);
    ttrue(rc == 0);
    ttrue(messages == 1);
}

/*
    A server that answers the upgrade with anything but a 101 must fail handshake verification.
    The /basic/ route requires credentials, so an unauthenticated upgrade is refused with a 401.
 */
static void testWebSocketBadHandshake()
{
    Url  *up;
    char url[128];

    up = urlAlloc(0);
    urlStart(up, "GET", SFMT(url, "ws%s/basic/secret.html", &HTTP[4]));

    //  Handshake verification runs in urlFinalize and must reject the 401 response
    ttrue(urlFinalize(up) < 0);
    ttrue(up->error != NULL);
    tcontains(up->error, "WebSocket");

    urlFree(up);

    //  The same failure through the high level API must report an error and release its Url
    messages = 0;
    ttrue(urlWebSocket(SFMT(url, "ws%s/basic/secret.html", &HTTP[4]),
                       (WebSocketProc) echoCallback, NULL, NULL) < 0);
    ttrue(messages == 0);
}


static void testWebSocketHeaders()
{
    char url[128];

    opened = messages = closed = 0;
    ttrue(urlWebSocket(SFMT(url, "ws%s/test/ws", &HTTP[4]), (WebSocketProc) echoCallback, NULL,
                       "Sec-WebSocket-Protocol: bogus\r\n") < 0);
    ttrue(opened == 0);

    opened = messages = closed = 0;
    ttrue(urlWebSocket(SFMT(url, "ws%s/test/ws", &HTTP[4]), (WebSocketProc) echoCallback, NULL,
                       "Sec-WebSocket-Protocol: chat\r\n") == 0);
    ttrue(opened == 1);
    ttrue(messages == WS_MESSAGES);
}

/*
    A ws:// scheme with an unroutable port must fail to connect, not crash
 */
static void testWebSocketBadConnect()
{
    int rc;

    messages = 0;
    rc = urlWebSocket("ws://localhost:1/test/ws", (WebSocketProc) echoCallback, NULL, NULL);
    ttrue(rc < 0);
    ttrue(messages == 0);
}

/*
    Null tolerance
 */
static void testWebSocketNullArgs()
{
    ttrue(urlWebSocket(NULL, (WebSocketProc) echoCallback, NULL, NULL) == R_ERR_BAD_ARGS);
    ttrue(urlWebSocket("ws://localhost/test/ws", NULL, NULL, NULL) == R_ERR_BAD_ARGS);
    ttrue(urlGetWebSocket(NULL) == NULL);
}

/*
    A plain HTTP request must not allocate a WebSocket
 */
static void testNoWebSocketForHttp()
{
    Url  *up;
    char url[128];

    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL) == 200);
    ttrue(urlGetWebSocket(up) == NULL);
    urlFree(up);
}

static void testFiber()
{
    if (setup(&HTTP, &HTTPS)) {
        testWebSocketEcho();
        testWebSocketLowLevel();
        testWebSocketBinary();
        testWebSocketBadHandshake();
        testWebSocketHeaders();
        testWebSocketBadConnect();
        testWebSocketNullArgs();
        testNoWebSocketForHttp();
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

#else

int main(void)
{
    tskip("ME_COM_WEBSOCK is not enabled");
    return 0;
}

#endif /* ME_COM_WEBSOCK */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
