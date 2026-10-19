/*
    websocket-origin.tst.c - WebSocket Origin policy and sub-protocol selection

    A browser will open a cross-origin WebSocket and attach the target's cookies to the
    handshake, with no preflight and no response header that can stop it, so the server has to
    refuse the upgrade itself (RFC 6455 4.2.2).

    The shared test server configures no "web.webSockets.origins" list, which selects the
    same-origin default. The allowlist branch therefore needs its own configuration, so the
    origin cases start hosts in process. Ports 4293-4294 are used by this test alone.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

#define PORT_LIST 4293
#define PORT_ANY  4294

//  An explicit allowlist. Only these origins may open a socket.
#define CONFIG_LIST                                                          \
    "{web: {documents: './site', listen: ['http://127.0.0.1:4293'],"         \
    " routes: [{match: '/ws/', methods: ['GET'], handler: 'action'}],"       \
    " webSockets: {enable: true, protocol: 'chat',"                          \
    "  origins: ['https://allowed.example.com', 'http://other.example.com']}}}"

//  "*" is the documented way to opt out of the check entirely
#define CONFIG_ANY                                                           \
    "{web: {documents: './site', listen: ['http://127.0.0.1:4294'],"         \
    " routes: [{match: '/ws/', methods: ['GET'], handler: 'action'}],"       \
    " webSockets: {enable: true, protocol: 'chat', origins: ['*']}}}"

#define UPGRADE_HEADERS                                                      \
    "Upgrade: websocket\r\n"                                                 \
    "Connection: Upgrade\r\n"                                                \
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"                        \
    "Sec-WebSocket-Version: 13\r\n"

/************************************ Code ************************************/

static WebHost *startHost(cchar *text)
{
    WebHost *host;
    Json    *config;

    if ((config = jsonParse(text, 0)) == 0) {
        tfail("Cannot parse websocket configuration");
        return 0;
    }
    if ((host = webAllocHost(config, 0)) == 0) {
        tfail("Cannot allocate host");
        jsonFree(config);
        return 0;
    }
    if (webStartHost(host) < 0) {
        tfail("Cannot start host - is the port already in use?");
        webFreeHost(host);
        return 0;
    }
    return host;
}

/*
    Attempt a WebSocket upgrade with the given extra headers. Returns the status.
 */
static int upgrade(cchar *base, cchar *extra)
{
    Url  *up;
    char url[160], headers[1024];
    int  status;

    up = urlAlloc(0);
    SFMT(headers, "%s%s", UPGRADE_HEADERS, extra ? extra : "");
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", base), NULL, 0, headers);
    urlFree(up);
    return status;
}

static int upgradePort(int port, cchar *extra)
{
    char base[64];

    SFMT(base, "http://127.0.0.1:%d", port);
    return upgrade(base, extra);
}

/*
    Attempt an upgrade and assert its status. The request must be made exactly once: teqi
    evaluates its first argument twice, once to compare and once to report.
 */
static void checkUpgrade(cchar *base, cchar *extra, int expect)
{
    int status;

    status = upgrade(base, extra);
    teqi(status, expect);
}

static void checkUpgradePort(int port, cchar *extra, int expect)
{
    int status;

    status = upgradePort(port, extra);
    teqi(status, expect);
}

/*
    With no origins list the server requires the Origin to match the request Host.
 */
static void testSameOriginDefault(void)
{
    char headers[256];

    //  No Origin at all - a non-browser client, allowed
    checkUpgrade(HTTP, NULL, 101);

    //  Origin matching the host we connected to
    SFMT(headers, "Origin: %s\r\n", HTTP);
    checkUpgrade(HTTP, headers, 101);

    //  A foreign origin is refused
    checkUpgrade(HTTP, "Origin: https://evil.example.com\r\n", 403);

    //  Same host and port, different scheme is still a different origin
    SFMT(headers, "Origin: https%s\r\n", &HTTP[4]);
    checkUpgrade(HTTP, headers, 403);

    //  An empty Origin is treated as absent
    checkUpgrade(HTTP, "Origin: \r\n", 101);
}

/*
    A configured allowlist replaces the same-origin rule.
 */
static void testOriginAllowlist(void)
{
    //  Listed origins are accepted even though they are cross-origin
    checkUpgradePort(PORT_LIST, "Origin: https://allowed.example.com\r\n", 101);
    checkUpgradePort(PORT_LIST, "Origin: http://other.example.com\r\n", 101);

    //  An unlisted origin is refused
    checkUpgradePort(PORT_LIST, "Origin: https://evil.example.com\r\n", 403);

    /*
        The allowlist replaces the same-origin rule rather than adding to it, so the server's own
        address is refused unless it is listed.
     */
    checkUpgradePort(PORT_LIST, "Origin: http://127.0.0.1:4293\r\n", 403);

    //  An absent Origin is still allowed - the check only applies to browser-attached origins
    checkUpgradePort(PORT_LIST, NULL, 101);

    //  Matching is exact, not by prefix or suffix
    checkUpgradePort(PORT_LIST, "Origin: https://allowed.example.com.evil.net\r\n", 403);
    checkUpgradePort(PORT_LIST, "Origin: https://sub.allowed.example.com\r\n", 403);
}

/*
    "*" opts out of the origin check entirely.
 */
static void testOriginWildcard(void)
{
    checkUpgradePort(PORT_ANY, "Origin: https://anything.example.com\r\n", 101);
    checkUpgradePort(PORT_ANY, "Origin: null\r\n", 101);
    checkUpgradePort(PORT_ANY, NULL, 101);
}

/*
    The server advertises one protocol. A client offering none, or offering a list containing it,
    upgrades; a list without it is refused rather than silently upgraded with no protocol.
 */
static void testProtocolSelection(void)
{
    //  No protocol offered at all
    checkUpgrade(HTTP, NULL, 101);

    //  A list containing the configured protocol
    checkUpgrade(HTTP, "Sec-WebSocket-Protocol: chat\r\n", 101);
    checkUpgrade(HTTP, "Sec-WebSocket-Protocol: chat, superchat\r\n", 101);

    //  A list with no acceptable protocol
    checkUpgrade(HTTP, "Sec-WebSocket-Protocol: foo, bar\r\n", 400);
    checkUpgrade(HTTP, "Sec-WebSocket-Protocol: superchat\r\n", 400);
}

/*
    An unbounded protocol list is a cheap way to make the server do work, so it is capped.
 */
static void testTooManyProtocols(void)
{
    RBuf *buf;
    char *headers;
    int  i;

    buf = rAllocBuf(512);
    rPutStringToBuf(buf, "Sec-WebSocket-Protocol: ");
    for (i = 0; i < 20; i++) {
        if (i) {
            rPutStringToBuf(buf, ", ");
        }
        rPutToBuf(buf, "proto%d", i);
    }
    rPutStringToBuf(buf, "\r\n");
    headers = rBufToStringAndFree(buf);

    checkUpgrade(HTTP, headers, 400);
    rFree(headers);
}

static void fiberMain(void *data)
{
    WebHost *list, *any;

    if (!setup(&HTTP, &HTTPS)) {
        rStop();
        return;
    }
    testSameOriginDefault();
    testProtocolSelection();
    testTooManyProtocols();

    if ((list = startHost(CONFIG_LIST)) != 0) {
        testOriginAllowlist();
        webStopHost(list);
        webFreeHost(list);
    }
    if ((any = startHost(CONFIG_ANY)) != 0) {
        testOriginWildcard();
        webStopHost(any);
        webFreeHost(any);
    }
    rFree(HTTP);
    rFree(HTTPS);
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
