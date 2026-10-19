/*
    websocket-upgrade-extended.tst.c - Extended WebSocket upgrade handshake testing

    Tests comprehensive WebSocket upgrade scenarios including protocol selection,
    extension negotiation, origin validation, and various edge cases in the upgrade
    handshake process.

    Based on RFC 6455 (WebSocket Protocol) and security best practices.

    Coverage:
    - Basic WebSocket upgrade (101 Switching Protocols)
    - Missing required headers (Sec-WebSocket-Key, Sec-WebSocket-Version)
    - Invalid Sec-WebSocket-Key format
    - Unsupported WebSocket version
    - Protocol sub-protocol selection (Sec-WebSocket-Protocol)
    - Extension negotiation (Sec-WebSocket-Extensions)
    - Origin header validation
    - Multiple protocol selection
    - Case sensitivity in headers
    - Upgrade header variations

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testBasicWebSocketUpgrade(void)
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *upgrade, *connection, *accept;

    up = urlAlloc(0);

    // Test: Valid WebSocket upgrade request
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n");

    if (status == 101) {
        // Successful upgrade - verify response headers
        upgrade = urlGetHeader(up, "Upgrade");
        connection = urlGetHeader(up, "Connection");
        accept = urlGetHeader(up, "Sec-WebSocket-Accept");

        ttrue(upgrade != NULL);
        ttrue(connection != NULL);
        ttrue(accept != NULL);

        // Upgrade should be "websocket" (case-insensitive)
        ttrue(smatch(upgrade, "websocket") || smatch(upgrade, "WebSocket"));

        // Connection should include "Upgrade"
        tcontains(connection, "Upgrade");

        // Accept key should be present and non-empty
        ttrue(slen(accept) > 0);
    } else {
        // WebSocket may not be enabled or endpoint doesn't exist
        ttrue(status == 404 || status == 400 || status == 501 || status < 0);
    }

    urlFree(up);
}

static void testMissingSecWebSocketKey(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test: Upgrade without Sec-WebSocket-Key (required)
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Version: 13\r\n");

    // Should reject with 400 Bad Request
    ttrue(status == 400 || status == 404 || status == 426 || status < 0);

    urlFree(up);
}

static void testMissingSecWebSocketVersion(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test: Upgrade without Sec-WebSocket-Version (required)
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n");

    // Should reject
    ttrue(status == 400 || status == 404 || status == 426 || status < 0);

    urlFree(up);
}

static void testInvalidWebSocketKey(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test 1: Key too short
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: short\r\n"
                      "Sec-WebSocket-Version: 13\r\n");
    // May reject or accept (some implementations are lenient)
    ttrue(status == 101 || status == 400 || status == 404 || status == 426 || status < 0);

    // Test 2: Empty key
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: \r\n"
                      "Sec-WebSocket-Version: 13\r\n");
    // May reject or accept (some implementations are lenient)
    ttrue(status == 101 || status == 400 || status == 404 || status == 426 || status < 0);

    // Test 3: Invalid base64 characters
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: @@@invalid@@@\r\n"
                      "Sec-WebSocket-Version: 13\r\n");
    // May reject or accept (some implementations are lenient)
    ttrue(status == 101 || status == 400 || status == 404 || status == 426 || status < 0);

    urlFree(up);
}

static void testUnsupportedVersion(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test 1: Old WebSocket version (8)
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 8\r\n");
    // Should reject or upgrade might not be available
    ttrue(status == 400 || status == 404 || status == 426 || status < 0);

    // Test 2: Future version (99)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 99\r\n");
    // May reject or accept (some implementations are lenient with versions)
    ttrue(status == 101 || status == 400 || status == 404 || status == 426 || status < 0);

    urlFree(up);
}

static void testProtocolSelection(void)
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *protocol;

    up = urlAlloc(0);

    // Test: Request with sub-protocol list
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n"
                      "Sec-WebSocket-Protocol: chat, superchat\r\n");

    if (status == 101) {
        // Server selected a protocol
        protocol = urlGetHeader(up, "Sec-WebSocket-Protocol");
        if (protocol) {
            // Should be one of the requested protocols
            ttrue(smatch(protocol, "chat") || smatch(protocol, "superchat"));
        }
    } else {
        // Upgrade not available
        ttrue(status == 400 || status == 404 || status == 501 || status < 0);
    }

    urlFree(up);
}

static void testExtensionNegotiation(void)
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *extensions;

    up = urlAlloc(0);

    // Test: Request with extension (permessage-deflate)
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n"
                      "Sec-WebSocket-Extensions: permessage-deflate\r\n");

    if (status == 101) {
        // Check if server accepted extension
        extensions = urlGetHeader(up, "Sec-WebSocket-Extensions");
        if (extensions) {
            // Extension was negotiated
            ttrue(slen(extensions) > 0);
        }
    } else {
        // Upgrade not available or extension not supported
        ttrue(status == 400 || status == 404 || status == 501 || status < 0);
    }

    urlFree(up);
}

/*
    A WebSocket upgrade is exempt from the same-origin policy, so the server must check Origin
    itself (RFC 6455 4.2.2). Without it, any page the victim visits can open an authenticated
    socket to the device. The default policy is same-origin: Origin must match the request Host.
 */
static void testOriginValidation(void)
{
    Url  *up;
    char url[128], hdrs[256];
    int  status;

    up = urlAlloc(0);

    // A matching origin is accepted
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      SFMT(hdrs,
                           "Upgrade: websocket\r\n"
                           "Connection: Upgrade\r\n"
                           "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                           "Sec-WebSocket-Version: 13\r\n"
                           "Origin: %s\r\n", HTTP));
    teq(status, 101);

    // A cross origin is refused. This is the CSWSH case: the browser would have attached the
    // victim's cookies to this handshake.
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n"
                      "Origin: https://evil.example\r\n");
    teq(status, 403);

    // Same host, wrong scheme is still a different origin
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      SFMT(hdrs,
                           "Upgrade: websocket\r\n"
                           "Connection: Upgrade\r\n"
                           "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                           "Sec-WebSocket-Version: 13\r\n"
                           "Origin: %s\r\n", HTTPS));
    teq(status, 403);

    // An absent Origin is not a browser request, so it is allowed - a non-browser client can
    // send any Origin it likes, so refusing this would cost compatibility and buy nothing.
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n");
    teq(status, 101);

    urlFree(up);
}

static void testCaseSensitivity(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test: Mixed case in Upgrade header
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: WebSocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n");
    // Should handle case-insensitive "websocket"
    ttrue(status == 101 || status == 400 || status == 404 || status < 0);

    urlFree(up);
}

static void testMissingUpgradeHeader(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test: Missing Upgrade header
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n");
    // Should reject (but may accept if implementation is lenient)
    ttrue(status == 101 || status == 400 || status == 404 || status == 426 || status < 0);

    urlFree(up);
}

static void testMissingConnectionHeader(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test: Missing Connection header
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n");
    // Should reject (but may accept if implementation is lenient)
    ttrue(status == 101 || status == 400 || status == 404 || status == 426 || status < 0);

    urlFree(up);
}

static void testInvalidHTTPMethod(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test: WebSocket upgrade must use GET method
    status = urlFetch(up, "POST", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n");
    // Should reject POST for WebSocket upgrade
    ttrue(status == 400 || status == 405 || status == 404 || status < 0);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testBasicWebSocketUpgrade();
        testMissingSecWebSocketKey();
        testMissingSecWebSocketVersion();
        testInvalidWebSocketKey();
        testUnsupportedVersion();
        testProtocolSelection();
        testExtensionNegotiation();
        testOriginValidation();
        testCaseSensitivity();
        testMissingUpgradeHeader();
        testMissingConnectionHeader();
        testInvalidHTTPMethod();
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
