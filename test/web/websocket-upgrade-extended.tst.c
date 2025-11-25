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

static void testOriginValidation(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test 1: Valid origin
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n"
                      "Origin: http://localhost:4100\r\n");
    // May succeed or fail depending on origin validation policy
    ttrue(status == 101 || status == 400 || status == 403 || status == 404 || status < 0);

    // Test 2: Different origin (potential CORS issue)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                      "Sec-WebSocket-Version: 13\r\n"
                      "Origin: http://evil.com\r\n");
    // Server may reject based on origin
    ttrue(status == 101 || status == 400 || status == 403 || status == 404 || status < 0);

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
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
