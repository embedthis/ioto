/*
    websocket-messaging.tst.c - WebSocket messaging and packet I/O testing

    Tests actual WebSocket packet sending and receiving including text messages,
    binary messages, message echoing, and close handshake sequences.

    Uses the /test/ws endpoint which echoes back received messages.

    Coverage:
    - Text message sending and receiving
    - Binary message sending and receiving
    - Multiple sequential messages
    - Larger messages (multi-frame if needed)
    - Message echo verification
    - Close handshake sequence
    - WebSocket async callback handling

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;
static char *WS;

typedef struct {
    char *receivedMessage;
    size_t receivedLength;
    size_t expectedLength;
    int messagesSent;
    int messagesReceived;
    int totalMessages;
    bool verified;
    bool failed;
} TestWebSocketData;

/************************************ Code ************************************/

/*
    WebSocket callback for text message echo test
 */
static void textMessageCallback(WebSocket *ws, int event, cchar *data, size_t len, void *arg)
{
    TestWebSocketData *testData = (TestWebSocketData*) arg;

    switch (event) {
    case WS_EVENT_OPEN:
        // Send initial message
        webSocketSend(ws, "Hello WebSocket!");
        testData->messagesSent = 1;
        testData->totalMessages = 1;
        testData->expectedLength = slen("Hello WebSocket!");
        break;

    case WS_EVENT_MESSAGE:
        // Verify echoed message
        testData->messagesReceived++;
        if (len == testData->expectedLength && smatch(data, "Hello WebSocket!")) {
            testData->verified = true;
        } else {
            testData->failed = true;
        }
        // Close after receiving echo
        webSocketSendClose(ws, WS_STATUS_OK, "Test complete");
        break;

    case WS_EVENT_ERROR:
        testData->failed = true;
        break;

    case WS_EVENT_CLOSE:
        break;
    }
}

/*
    Test basic text message echo
 */
static void testTextMessageEcho(void)
{
    TestWebSocketData testData;
    Url               *up;
    char              url[128];
    int               rc;

    memset(&testData, 0, sizeof(testData));
    up = urlAlloc(0);

    // Perform WebSocket upgrade and run
    rc = urlWebSocket(SFMT(url, "%s/test/ws/", WS), (WebSocketProc) textMessageCallback, &testData, NULL);

    teqi(rc, 0);
    teqi(testData.messagesSent, 1);
    teqi(testData.messagesReceived, 1);
    ttrue(testData.verified);
    tfalse(testData.failed);

    urlFree(up);
}

/*
    WebSocket callback for binary message test
 */
static void binaryMessageCallback(WebSocket *ws, int event, cchar *data, size_t len, void *arg)
{
    TestWebSocketData *testData = (TestWebSocketData*) arg;
    uchar             binaryData[256];
    int               i;

    switch (event) {
    case WS_EVENT_OPEN:
        // Create and send binary data
        for (i = 0; i < (int) sizeof(binaryData); i++) {
            binaryData[i] = (uchar) i;
        }
        webSocketSendBlock(ws, WS_MSG_BINARY, (cchar*) binaryData, sizeof(binaryData));
        testData->messagesSent = 1;
        testData->expectedLength = sizeof(binaryData);
        break;

    case WS_EVENT_MESSAGE:
        // Verify echoed binary data
        testData->messagesReceived++;
        testData->receivedLength = len;

        // Re-create expected binary data for comparison
        for (i = 0; i < 256; i++) {
            binaryData[i] = (uchar) i;
        }
        if (len == sizeof(binaryData) && memcmp(data, binaryData, sizeof(binaryData)) == 0) {
            testData->verified = true;
        } else {
            testData->failed = true;
        }
        webSocketSendClose(ws, WS_STATUS_OK, "Test complete");
        break;

    case WS_EVENT_ERROR:
        testData->failed = true;
        break;

    case WS_EVENT_CLOSE:
        break;
    }
}

/*
    Test binary message echo
 */
static void testBinaryMessageEcho(void)
{
    TestWebSocketData testData;
    char              url[128];
    int               rc;

    memset(&testData, 0, sizeof(testData));

    rc = urlWebSocket(SFMT(url, "%s/test/ws/", WS), (WebSocketProc) binaryMessageCallback, &testData, NULL);

    teqi(rc, 0);
    teqi(testData.messagesSent, 1);
    teqi(testData.messagesReceived, 1);
    teqz(testData.receivedLength, 256);
    ttrue(testData.verified);
    tfalse(testData.failed);
}

/*
    WebSocket callback for multiple messages test
 */
static void multipleMessagesCallback(WebSocket *ws, int event, cchar *data, size_t len, void *arg)
{
    TestWebSocketData *testData = (TestWebSocketData*) arg;
    char              expected[64];

    switch (event) {
    case WS_EVENT_OPEN:
        // Send first message
        webSocketSend(ws, "Message %d", testData->messagesSent);
        testData->messagesSent++;
        testData->totalMessages = 10;
        break;

    case WS_EVENT_MESSAGE:
        // Verify echoed message
        SFMT(expected, "Message %d", testData->messagesReceived);
        if (smatch(data, expected)) {
            testData->messagesReceived++;

            // Send next message or close
            if (testData->messagesSent < testData->totalMessages) {
                webSocketSend(ws, "Message %d", testData->messagesSent);
                testData->messagesSent++;
            } else {
                testData->verified = true;
                webSocketSendClose(ws, WS_STATUS_OK, "Test complete");
            }
        } else {
            testData->failed = true;
            webSocketSendClose(ws, WS_STATUS_OK, "Verification failed");
        }
        break;

    case WS_EVENT_ERROR:
        testData->failed = true;
        break;

    case WS_EVENT_CLOSE:
        break;
    }
}

/*
    Test multiple sequential messages
 */
static void testMultipleMessages(void)
{
    TestWebSocketData testData;
    char              url[128];
    int               rc;

    memset(&testData, 0, sizeof(testData));

    rc = urlWebSocket(SFMT(url, "%s/test/ws/", WS), (WebSocketProc) multipleMessagesCallback, &testData, NULL);

    teqi(rc, 0);
    teqi(testData.messagesSent, 10);
    teqi(testData.messagesReceived, 10);
    ttrue(testData.verified);
    tfalse(testData.failed);
}

/*
    WebSocket callback for large message test
 */
static void largeMessageCallback(WebSocket *ws, int event, cchar *data, size_t len, void *arg)
{
    TestWebSocketData *testData = (TestWebSocketData*) arg;
    char              *largeMessage;
    size_t            messageSize;
    int               i;

    switch (event) {
    case WS_EVENT_OPEN:
        // Create large message (32KB)
        messageSize = 32 * 1024;
        largeMessage = rAlloc(messageSize + 1);
        for (i = 0; i < (int) messageSize; i++) {
            largeMessage[i] = 'A' + (i % 26);
        }
        largeMessage[messageSize] = '\0';

        webSocketSendBlock(ws, WS_MSG_TEXT, largeMessage, messageSize);
        testData->messagesSent = 1;
        testData->expectedLength = messageSize;
        rFree(largeMessage);
        break;

    case WS_EVENT_MESSAGE:
        // Verify large message was echoed
        testData->messagesReceived++;
        testData->receivedLength = len;

        if (len == testData->expectedLength) {
            // Verify content pattern
            testData->verified = true;
            for (i = 0; i < (int) len; i++) {
                if (data[i] != 'A' + (i % 26)) {
                    testData->verified = false;
                    testData->failed = true;
                    break;
                }
            }
        } else {
            testData->failed = true;
        }
        webSocketSendClose(ws, WS_STATUS_OK, "Test complete");
        break;

    case WS_EVENT_ERROR:
        testData->failed = true;
        break;

    case WS_EVENT_CLOSE:
        break;
    }
}

/*
    Test large message (may be fragmented)
 */
static void testLargeMessage(void)
{
    TestWebSocketData testData;
    char              url[128];
    int               rc;

    memset(&testData, 0, sizeof(testData));

    rc = urlWebSocket(SFMT(url, "%s/test/ws/", WS), (WebSocketProc) largeMessageCallback, &testData, NULL);

    teqi(rc, 0);
    teqi(testData.messagesSent, 1);
    teqi(testData.messagesReceived, 1);
    teqz(testData.receivedLength, 32 * 1024);
    ttrue(testData.verified);
    tfalse(testData.failed);
}

/*
    WebSocket callback for close handshake test
 */
static void closeHandshakeCallback(WebSocket *ws, int event, cchar *data, size_t len, void *arg)
{
    TestWebSocketData *testData = (TestWebSocketData*) arg;

    switch (event) {
    case WS_EVENT_OPEN:
        // Send one message then close immediately
        webSocketSend(ws, "Closing soon");
        testData->messagesSent = 1;
        break;

    case WS_EVENT_MESSAGE:
        // Received echo, initiate close handshake
        testData->messagesReceived++;
        testData->verified = true;
        webSocketSendClose(ws, WS_STATUS_OK, "Normal closure");
        break;

    case WS_EVENT_ERROR:
        testData->failed = true;
        break;

    case WS_EVENT_CLOSE:
        // Close acknowledged
        break;
    }
}

/*
    Test proper close handshake
 */
static void testCloseHandshake(void)
{
    TestWebSocketData testData;
    char              url[128];
    int               rc;

    memset(&testData, 0, sizeof(testData));

    rc = urlWebSocket(SFMT(url, "%s/test/ws/", WS), (WebSocketProc) closeHandshakeCallback, &testData, NULL);

    teqi(rc, 0);
    teqi(testData.messagesSent, 1);
    teqi(testData.messagesReceived, 1);
    ttrue(testData.verified);
    tfalse(testData.failed);
}

/*
    WebSocket callback for empty message test
 */
static void emptyMessageCallback(WebSocket *ws, int event, cchar *data, size_t len, void *arg)
{
    TestWebSocketData *testData = (TestWebSocketData*) arg;

    switch (event) {
    case WS_EVENT_OPEN:
        // Send empty message
        webSocketSendBlock(ws, WS_MSG_TEXT, "", 0);
        testData->messagesSent = 1;
        testData->expectedLength = 0;
        break;

    case WS_EVENT_MESSAGE:
        // Verify empty message was echoed
        testData->messagesReceived++;
        testData->receivedLength = len;
        if (len == 0) {
            testData->verified = true;
        } else {
            testData->failed = true;
        }
        webSocketSendClose(ws, WS_STATUS_OK, "Test complete");
        break;

    case WS_EVENT_ERROR:
        testData->failed = true;
        break;

    case WS_EVENT_CLOSE:
        break;
    }
}

/*
    Test sending empty message
 */
static void testEmptyMessage(void)
{
    TestWebSocketData testData;
    char              url[128];
    int               rc;

    memset(&testData, 0, sizeof(testData));

    rc = urlWebSocket(SFMT(url, "%s/test/ws/", WS), (WebSocketProc) emptyMessageCallback, &testData, NULL);

    teqi(rc, 0);
    teqi(testData.messagesSent, 1);
    teqi(testData.messagesReceived, 1);
    teqz(testData.receivedLength, 0);
    ttrue(testData.verified);
    tfalse(testData.failed);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        WS = sreplace(HTTP, "http", "ws");
        testTextMessageEcho();
        testBinaryMessageEcho();
        testMultipleMessages();
        testLargeMessage();
        testCloseHandshake();
        testEmptyMessage();
    }
    rFree(HTTP);
    rFree(HTTPS);
    rFree(WS);
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
