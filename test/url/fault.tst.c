/*
    fault.tst.c - Unit tests for client resilience against a malformed or hostile server

    The web test server is well behaved, so nothing else in the suite reaches the client's response
    parsing and read error paths. These tests stand up a raw TCP listener in-process and answer with
    crafted bytes: truncated status lines, impossible status codes, malformed headers, a body shorter
    than its Content-Length, broken chunk framing and forged WebSocket handshakes.

    A device that trusts whatever a remote server sends it is the case that matters here -- each of
    these must produce a clean error, never a crash, a hang or a silently truncated response.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static RSocket *listenSock;
static RSocket *listenSock6;          /* On ::1, when IPv6 loopback is available */
static int     faultPort;
static int     faultPort6;

/*
    The bytes the fault server answers with, and how it ends the connection.
 */
static cchar *reply;
static size_t replyLen;
static char   lastRequest[4096];      /* The request bytes the fault server received */
static char   firstRequest[4096];     /* The first request of a sequenced exchange */
static cchar *replies[8];             /* Successive replies, one per request */
static int    replyIndex;
static int    dribble;                /* Send the reply one byte at a time */
static Ticks  dribbleDelay;           /* Pause between dribbled bytes, so the reply arrives slowly */
static int    closeOnAccept;          /* Close the connection without reading or answering */
static int    stall;                  /* Read the request and never answer it */
static ssize  requestTotal;           /* Every byte of the last request, including any beyond lastRequest */

#define FAULT_TIMEOUT (5 * TPS)

/************************************ Code ************************************/

/*
    Answer one request with the crafted reply, then close. Runs on its own fiber.
 */
static void faultHandler(void *data, RSocket *sp)
{
    char   buf[4096], drain[4096], tail[8];
    char   *copy, *cp, *end, *dest;
    cchar  *answer;
    size_t i, len, space;
    ssize  nbytes, total, expected;
    bool   chunkedRequest;

    if (closeOnAccept) {
        rCloseSocket(sp);
        rFreeSocket(sp);
        return;
    }
    /*
        Answer requests until the client stops asking. A scripted exchange (replies[]) keeps the
        connection open between requests, as a real keep-alive server does -- closing after each
        reply would make the client retry on a dead socket and never reach the second reply.
     */
    do {
        /*
            Read the whole request, not just the first segment. The client writes headers and body
            separately, so a single read can return only the headers -- which would make any test
            that asserts on the request body flap. The first 4K is kept for the tests to inspect;
            a larger body is drained and counted so the client is never left blocked on a write.
         */
        memset(buf, 0, sizeof(buf));
        memset(tail, 0, sizeof(tail));
        total = 0;
        requestTotal = 0;
        expected = -1;
        chunkedRequest = 0;
        end = 0;
        while (1) {
            if (total < (ssize) sizeof(buf) - 1) {
                dest = &buf[total];
                space = sizeof(buf) - 1 - (size_t) total;
            } else {
                dest = drain;
                space = sizeof(drain);
            }
            nbytes = rReadSocket(sp, dest, space, rGetTicks() + FAULT_TIMEOUT);
            if (nbytes <= 0) {
                break;
            }
            if (dest != drain) {
                total += nbytes;
            }
            requestTotal += nbytes;
            /*
                Keep the last few bytes across reads, so a terminating chunk split by a read
                boundary is still recognised
             */
            if (nbytes >= (ssize) sizeof(tail) - 1) {
                memcpy(tail, &dest[nbytes - (sizeof(tail) - 1)], sizeof(tail) - 1);
            } else {
                memmove(tail, &tail[nbytes], sizeof(tail) - 1 - (size_t) nbytes);
                memcpy(&tail[sizeof(tail) - 1 - (size_t) nbytes], dest, (size_t) nbytes);
            }
            //  Stop when the request is complete, so no read waits out the timeout
            if (end == 0 && (end = scontains(buf, "\r\n\r\n")) != 0) {
                if ((cp = sncaselesscontains(buf, "content-length:", (size_t) (end - buf))) != 0) {
                    expected = (end - buf) + 4 + (ssize) stoi(&cp[slen("content-length:")]);
                } else if (sncaselesscontains(buf, "transfer-encoding:", (size_t) (end - buf)) != 0) {
                    chunkedRequest = 1;
                } else {
                    expected = (end - buf) + 4;
                }
            }
            if (end && (chunkedRequest ? smatch(tail, "\r\n0\r\n\r\n") : requestTotal >= expected)) {
                break;
            }
        }
        if (total <= 0) {
            break;
        }
        if (replyIndex == 0) {
            scopy(firstRequest, sizeof(firstRequest), buf);
        }
        scopy(lastRequest, sizeof(lastRequest), buf);

        if (stall) {
            //  Hold the connection open without a word until the client gives up
            while (rReadSocket(sp, drain, sizeof(drain), rGetTicks() + 120 * TPS) > 0) {}
            break;
        }
        if (replies[0]) {
            answer = replies[replyIndex] ? replies[replyIndex] : replies[0];
            replyIndex++;
        } else {
            answer = reply;
        }
        if (!answer) {
            break;
        }
        //  replyLen lets a single reply carry an embedded NUL
        len = (answer == reply && replyLen) ? replyLen : slen(answer);
        /*
            Keep a private response copy because the client may return and free the original while this fiber writes.
         */
        copy = rAlloc(len + 1);
        memcpy(copy, answer, len);
        if (dribble) {
            //  One byte per write, so the client must cope with the response arriving in pieces
            for (i = 0; i < len; i++) {
                if (rWriteSocket(sp, &copy[i], 1, rGetTicks() + FAULT_TIMEOUT) < 0) {
                    break;
                }
                if (dribbleDelay) {
                    rSleep(dribbleDelay);
                }
            }
        } else {
            rWriteSocket(sp, copy, len, rGetTicks() + FAULT_TIMEOUT);
        }
        rFree(copy);
        //  Only a scripted exchange stays open for a further request
    } while (replies[0] && replies[replyIndex]);

    rCloseSocket(sp);
    rFreeSocket(sp);
}

static bool startFaultServer()
{
    char ipbuf[64];

    listenSock = rAllocSocket();
    //  Port 0 lets the system choose, so the test cannot collide with the web test server
    if (rListenSocket(listenSock, "127.0.0.1", 0, (RSocketProc) faultHandler, NULL) < 0) {
        tfail("Cannot listen on 127.0.0.1");
        return 0;
    }
    if (rGetSocketAddr(listenSock, ipbuf, sizeof(ipbuf), &faultPort) < 0 || faultPort <= 0) {
        tfail("Cannot determine the fault server port");
        return 0;
    }
    tinfo("Fault server listening on 127.0.0.1:%d", faultPort);

    //  A second listener on the IPv6 loopback, so the Host header for a literal can be observed
    listenSock6 = rAllocSocket();
    if (rListenSocket(listenSock6, "::1", 0, (RSocketProc) faultHandler, NULL) == 0 &&
        rGetSocketAddr(listenSock6, ipbuf, sizeof(ipbuf), &faultPort6) == 0) {
        tinfo("Fault server listening on [::1]:%d", faultPort6);
    } else {
        rFreeSocket(listenSock6);
        listenSock6 = 0;
        faultPort6 = 0;
    }
    return 1;
}

static void stopFaultServer()
{
    rFreeSocket(listenSock);
    listenSock = 0;
    rFreeSocket(listenSock6);
    listenSock6 = 0;
}

/*
    Set the next response and issue a request against it. Returns the status or error code.
 */
static int faultRequest(Url *up, cchar *method, cchar *response)
{
    char url[128];

    reply = response;
    replyLen = response ? slen(response) : 0;
    return urlFetch(up, method, SFMT(url, "http://127.0.0.1:%d/fault", faultPort), NULL, 0, NULL);
}

static int faultFetch(Url *up, cchar *response)
{
    return faultRequest(up, "GET", response);
}

/*
    A header block too short to be a status line
 */
static void testShortHeader()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HI\r\n\r\n") < 0);
    tcontains(urlGetError(up), "Bad response header");
    urlFree(up);
}

/*
    A status line with no space, so no status code can be found
 */
static void testNoStatusCode()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1-200-OK-PADDING\r\n\r\n") < 0);
    urlFree(up);
}

/*
    Status codes outside the 100-599 range are not a response we can act on
 */
static void testBadStatusCode()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 999 Nonsense\r\n\r\n") < 0);
    tcontains(urlGetError(up), "Bad response status");
    urlFree(up);

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 42 Nonsense\r\n\r\n") < 0);
    tcontains(urlGetError(up), "Bad response status");
    urlFree(up);

    //  A non-numeric status parses as zero, which is equally out of range
    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 OK Nonsense\r\n\r\n") < 0);
    tcontains(urlGetError(up), "Bad response status");
    urlFree(up);
}

/*
    Check HTTP version, status digit count, range, and separators.
 */
static void testResponseLineSyntax()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "JUNK 200 OK\r\n\r\n") < 0);
    urlFree(up);

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/9.9 200 OK\r\n\r\n") < 0);
    urlFree(up);

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 +200 OK\r\n\r\n") < 0);
    urlFree(up);

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200x OK\r\n\r\n") < 0);
    urlFree(up);

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 99 OK\r\n\r\n") < 0);
    urlFree(up);

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\n\r\n") == 200);
    urlFree(up);

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200\r\n\r\n") == 200);
    urlFree(up);

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.0 200 OK\r\n\r\n") == 200);
    urlFree(up);

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.0 200\r\n\r\n") == 200);
    urlFree(up);
}

/*
    A header line with no colon, and one whose key holds characters a token may not contain
 */
static void testMalformedHeader()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nNoColonHere\r\n\r\n") < 0);
    urlFree(up);

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nBad(Key): value\r\n\r\n") < 0);
    urlFree(up);

    //  An empty header key is equally not a token
    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\n: value\r\n\r\n") < 0);
    urlFree(up);
}

/*
    Empty values must not underflow the whitespace trim.
 */
static void testEmptyHeaderValue()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nX:\r\nConnection: \r\n\r\n") == 200);
    tmatch(urlGetHeader(up, "X"), "");
    tmatch(urlGetHeader(up, "Connection"), "");
    urlFree(up);
}

/*
    A negative Content-Length must be rejected rather than wrapped into a huge unsigned length
 */
static void testNegativeContentLength()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nContent-Length: -5\r\n\r\n") < 0);
    tcontains(urlGetError(up), "Invalid Content-Length");
    urlFree(up);
}

/*
    A Content-Length beyond the response buffer limit must be refused before anything is read
 */
static void testOversizedContentLength()
{
    Url   *up;
    cchar *response;

    up = urlAlloc(0);
    urlSetBufLimit(up, 1024);
    response = "HTTP/1.1 200 OK\r\nContent-Length: 1048576\r\n\r\n";
    ttrue(faultFetch(up, response) == 200);

    //  The status parses, but the body is refused
    tmatch(urlGetResponse(up), "");
    tcontains(urlGetError(up), "Invalid Content-Length");
    urlFree(up);
}

/*
    A body shorter than its declared Content-Length is a truncated response, not a short one
 */
static void testTruncatedBody()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\nonly-a-few") == 200);
    //  The client must not present the partial body as if it were complete
    ttrue(slen(urlGetResponse(up)) < 100);
    ttrue(urlGetError(up) != NULL);
    urlFree(up);
}

/*
    Chunk framing the client cannot parse
 */
static void testBadChunkSpecification()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nZZZZ\r\nbody\r\n") == 200);
    tmatch(urlGetResponse(up), "");
    tcontains(urlGetError(up), "chunk");
    urlFree(up);
}

/*
    A chunk that declares more data than it delivers before the connection closes
 */
static void testTruncatedChunk()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n100\r\nshort") == 200);
    //  The body is read lazily, so the framing error surfaces on the first read, not on the fetch
    ttrue(slen(urlGetResponse(up)) < 0x100);
    ttrue(urlGetError(up) != NULL);
    urlFree(up);
}

/*
    A well formed chunked response still works through the same path
 */
static void testGoodChunks()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                         "5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n") == 200);
    tmatch(urlGetResponse(up), "Hello World");
    urlFree(up);
}

/*
    A server that accepts the connection and closes without answering
 */
static void testNoResponse()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, NULL) < 0);
    ttrue(urlGetError(up) != NULL);
    urlFree(up);
}

/*
    A response delivered one byte at a time must parse identically. This drives the read loops
    through every partial-buffer state, which a single-segment response never exercises.
 */
static void testDribbledResponse()
{
    Url *up;

    dribble = 1;
    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\nHello World") == 200);
    tmatch(urlGetResponse(up), "Hello World");
    urlFree(up);

    //  And the same for chunked framing
    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                         "5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n") == 200);
    tmatch(urlGetResponse(up), "Hello World");
    urlFree(up);
    dribble = 0;
}

/*
    A 1xx is interim. The client must discard it and keep reading for the final response.
 */
static void testInterimResponse()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 100 Continue\r\n\r\n"
                         "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHELLO") == 200);
    tmatch(urlGetResponse(up), "HELLO");
    ttrue(urlGetError(up) == NULL);
    urlFree(up);

    //  Several interim responses in a row are all skipped
    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 100 Continue\r\n\r\n"
                         "HTTP/1.1 103 Early Hints\r\nLink: </s.css>\r\n\r\n"
                         "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi") == 200);
    tmatch(urlGetResponse(up), "hi");
    urlFree(up);

    //  Headers from the final response are the ones the caller sees
    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 100 Continue\r\nX-Interim: yes\r\n\r\n"
                         "HTTP/1.1 200 OK\r\nX-Final: yes\r\nContent-Length: 0\r\n\r\n") == 200);
    ttrue(urlGetHeader(up, "X-Final") != NULL);
    ttrue(urlGetHeader(up, "X-Interim") == NULL);
    urlFree(up);
}

/*
    RFC 7230 3.3: a 204 or 304 never has a body, whatever Content-Length claims. Bytes that follow
    one are the next response, and returning them as the body is a response-smuggling read.
 */
static void testBodilessStatus()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 204 No Content\r\nContent-Length: 44\r\n\r\n"
                         "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nBAD") == 204);
    tmatch(urlGetResponse(up), "");
    ttrue(urlGetError(up) == NULL);
    //  The trailing bytes are not ours, so the connection must not be reused
    ttrue(up->close);
    urlFree(up);

    //  A 304 is the normal answer to a conditional GET and must not report an error
    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 304 Not Modified\r\nContent-Length: 5\r\n\r\nHELLO") == 304);
    tmatch(urlGetResponse(up), "");
    ttrue(urlGetError(up) == NULL);
    urlFree(up);

    //  A 304 with no body at all is the common case and must be clean too
    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 304 Not Modified\r\nETag: \"abc\"\r\n\r\n") == 304);
    tmatch(urlGetResponse(up), "");
    ttrue(urlGetError(up) == NULL);
    urlFree(up);
}

/*
    RFC 7230 3.3.3: Content-Length headers that disagree make the message unframable
 */
static void testConflictingContentLength()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: 100\r\n\r\nHELLO") < 0);
    tcontains(urlGetError(up), "Conflicting Content-Length");
    urlFree(up);

    //  Repeated headers that agree are harmless
    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\nHELLO") == 200);
    tmatch(urlGetResponse(up), "HELLO");
    urlFree(up);
}

/*
    Transfer-Encoding overrides Content-Length, and an ordinary single Content-Length is unaffected
 */
static void testFramingPrecedence()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n"
                         "5\r\nHELLO\r\n0\r\n\r\n") == 200);
    tmatch(urlGetResponse(up), "HELLO");
    urlFree(up);


    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n"
                         "A\r\n0123456789\r\n0\r\n\r\n") == 200);
    tmatch(urlGetResponse(up), "0123456789");
    ttrue(up->close);
    urlFree(up);

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHELLO") == 200);
    tmatch(urlGetResponse(up), "HELLO");
    urlFree(up);
}

/*
    RFC 7230 3.3.3 case 7: with neither Content-Length nor a transfer coding the body runs to the
    connection close. That is how an HTTP/1.0 server frames a response of unknown length, and the
    client used to report "Cannot read response" and discard the body when it arrived.
 */
static void testCloseDelimitedBody()
{
    Url *up;

    up = urlAlloc(0);
    teqi(faultFetch(up, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello World"), 200);
    tmatch(urlGetResponse(up), "Hello World");
    tnull(urlGetError(up), "A close-delimited body is a complete body, not a read error");
    urlFree(up);

    up = urlAlloc(0);
    teqi(faultFetch(up, "HTTP/1.0 200 OK\r\n\r\nHello World"), 200);
    tmatch(urlGetResponse(up), "Hello World");
    urlFree(up);

    //  An empty close-delimited body is still a complete body
    up = urlAlloc(0);
    teqi(faultFetch(up, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n"), 200);
    tmatch(urlGetResponse(up), "");
    tnull(urlGetError(up), "An empty close-delimited body is not a read error");
    urlFree(up);
}

#if URL_SSE
/*
    SSE streams. The live test server only ever sends id/event/data, so the tolerance the protocol
    requires for comments, retry and unknown fields is only reachable from a crafted stream.
 */
static int  sseCount;
static ssize sseId;
static char sseEvent[64];
static char sseData[256];

static void sseProc(Url *up, ssize id, cchar *event, cchar *data, void *arg)
{
    sseCount++;
    sseId = id;
    scopy(sseEvent, sizeof(sseEvent), event ? event : "");
    scopy(sseData, sizeof(sseData), data ? data : "");
}

/*
    Serve an event-stream body and run it to completion. Returns the Url so the caller can
    inspect the error; the caller frees it.
 */
static Url *faultSse(cchar *body)
{
    Url  *up;
    char *response;
    char url[128];

    response = sfmt("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n\r\n%s", body);
    reply = response;
    replyLen = slen(response);

    sseCount = 0;
    sseId = -999;
    sseEvent[0] = sseData[0] = '\0';

    up = urlAlloc(0);
    urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:%d/events", faultPort), NULL, 0, NULL);
    urlSseRun(up, sseProc, NULL, up->rx, rGetTicks() + FAULT_TIMEOUT);
    rFree(response);
    return up;
}

/*
    The fields the parser already understands
 */
static void testSseFields()
{
    Url *up;

    up = faultSse("data: hello\n\n");
    ttrue(sseCount == 1);
    tmatch(sseData, "hello");
    urlFree(up);

    //  Multiple data fields concatenate with a newline
    up = faultSse("data: one\ndata: two\n\n");
    ttrue(sseCount == 1);
    tmatch(sseData, "one\ntwo");
    urlFree(up);

    up = faultSse("id: 42\nevent: tick\ndata: payload\n\n");
    ttrue(sseCount == 1);
    ttrue(sseId == 42);
    tmatch(sseEvent, "tick");
    tmatch(sseData, "payload");
    urlFree(up);

    //  A repeated id or event overwrites the earlier value
    up = faultSse("id: 1\nid: 7\nevent: a\nevent: b\ndata: x\n\n");
    ttrue(sseCount == 1);
    ttrue(sseId == 7);
    tmatch(sseEvent, "b");
    urlFree(up);

    //  Consecutive events are delivered in order
    up = faultSse("data: a\n\ndata: b\n\n");
    ttrue(sseCount == 2);
    tmatch(sseData, "b");
    urlFree(up);
}

/*
    A comment line is the standard SSE keep-alive. It must be ignored, not end the stream.
 */
static void testSseComment()
{
    Url *up;

    up = faultSse(": keep-alive\n\ndata: x\n\n");
    ttrue(sseCount == 1);
    tmatch(sseData, "x");
    urlFree(up);

    //  A comment inside an event is equally ignorable
    up = faultSse(": ping\ndata: y\n\n");
    ttrue(sseCount == 1);
    tmatch(sseData, "y");
    urlFree(up);
}

/*
    retry is a defined SSE field, and an unrecognised field name must be ignored per the spec
 */
static void testSseUnknownFields()
{
    Url *up;

    up = faultSse("retry: 5000\ndata: x\n\n");
    ttrue(sseCount == 1);
    tmatch(sseData, "x");
    urlFree(up);

    up = faultSse("foo: bar\ndata: x\n\n");
    ttrue(sseCount == 1);
    tmatch(sseData, "x");
    urlFree(up);
}

/*
    An event carrying no data is not dispatched, but it must not end the stream either
 */
static void testSseNoData()
{
    Url *up;

    up = faultSse("event: ping\n\ndata: x\n\n");
    ttrue(sseCount == 1);
    tmatch(sseData, "x");
    urlFree(up);
}

/*
    An SSE request must ask for an event stream. A server that content-negotiates returns an
    ordinary document otherwise, and the client has no way to tell it was given the wrong thing.
 */
static void testSseRequestHeaders()
{
    Url  *up;
    char url[128];

    lastRequest[0] = '\0';
    reply = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n\r\ndata: x\n\n";
    replyLen = slen(reply);
    sseCount = 0;
    urlGetEvents(SFMT(url, "http://127.0.0.1:%d/events", faultPort), sseProc, NULL, NULL);

    ttrue(sseCount == 1);
    tcontains(lastRequest, "Accept: text/event-stream");

    //  Caller supplied headers must survive alongside it
    lastRequest[0] = '\0';
    sseCount = 0;
    urlGetEvents(SFMT(url, "http://127.0.0.1:%d/events", faultPort), sseProc, NULL,
                 "X-Caller: mine\r\n");
    ttrue(sseCount == 1);
    tcontains(lastRequest, "Accept: text/event-stream");
    tcontains(lastRequest, "X-Caller: mine");

    //  An ordinary request on a fresh object must not carry the SSE headers
    lastRequest[0] = '\0';
    up = urlAlloc(0);
    faultFetch(up, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi");
    ttrue(scontains(lastRequest, "Accept: text/event-stream") == 0);
    urlFree(up);
}

/*
    A line that cannot be tokenised at all is a framing failure and stays fatal
 */
static void testSseMalformed()
{
    Url *up;

    up = faultSse("data x\n\n");
    ttrue(sseCount == 0);
    ttrue(urlGetError(up) != NULL);
    urlFree(up);
}

static int sseFreeCount;

static void sseFreeProc(Url *up, ssize id, cchar *event, cchar *data, void *arg)
{
    sseFreeCount++;
    urlFree(up);
}

static void testSseFreeInCallback()
{
    Url  *up;
    char url[128];
    int  rc;

    reply = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n\r\ndata: one\n\ndata: two\n\n";
    replyLen = slen(reply);
    SFMT(url, "http://127.0.0.1:%d/events", faultPort);

    sseFreeCount = 0;
    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) == 200);
    //  Outside the assertion, as a macro that re-evaluates it would run again on the freed Url
    rc = urlSseRun(up, sseFreeProc, NULL, up->rx, rGetTicks() + FAULT_TIMEOUT);
    teqi(rc, R_ERR_DELETED);
    teqi(sseFreeCount, 1);

    //  urlGetEvents owns its Url and must not free it a second time
    sseFreeCount = 0;
    ttrue(urlGetEvents(url, sseFreeProc, NULL, NULL) == 0);
    teqi(sseFreeCount, 1);
}
#endif /* URL_SSE */

#if ME_COM_WEBSOCK
/*
    Forged WebSocket handshakes. Each must be refused: accepting one would leave the client
    speaking the WebSocket framing protocol to a server that never agreed to it.
 */
static void wsCallback(WebSocket *ws, int event, cchar *data, size_t len, void *arg)
{
    tfail("WebSocket callback must not run for a refused handshake");
}

static int faultWebSocket(cchar *response)
{
    char url[128];

    reply = response;
    replyLen = slen(response);
    return urlWebSocket(SFMT(url, "ws://127.0.0.1:%d/fault", faultPort), (WebSocketProc) wsCallback, NULL, NULL);
}

static void testForgedWebSocketHandshake()
{
    //  101 but no Connection: Upgrade
    ttrue(faultWebSocket("HTTP/1.1 101 Switching Protocols\r\n"
                         "Upgrade: websocket\r\n"
                         "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n") < 0);

    //  101 but no Upgrade: websocket
    ttrue(faultWebSocket("HTTP/1.1 101 Switching Protocols\r\n"
                         "Connection: Upgrade\r\n"
                         "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n") < 0);

    //  101 with the right shape but an accept key that does not match the client nonce
    ttrue(faultWebSocket("HTTP/1.1 101 Switching Protocols\r\n"
                         "Connection: Upgrade\r\n"
                         "Upgrade: websocket\r\n"
                         "Sec-WebSocket-Accept: AAAAAAAAAAAAAAAAAAAAAAAAAAA=\r\n\r\n") < 0);

    //  101 with no accept key at all
    ttrue(faultWebSocket("HTTP/1.1 101 Switching Protocols\r\n"
                         "Connection: Upgrade\r\n"
                         "Upgrade: websocket\r\n\r\n") < 0);

    //  A plain 200 is not an upgrade
    ttrue(faultWebSocket("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n") < 0);
}

static void testWebSocketClientKey()
{
    Url  *up;
    char key[201], url[128];

    reply = "HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
    replyLen = slen(reply);
    memset(key, 'k', sizeof(key) - 1);
    key[sizeof(key) - 1] = '\0';

    up = urlAlloc(0);
    ttrue(urlStart(up, "GET", SFMT(url, "ws://127.0.0.1:%d/ws", faultPort)) == 0);
    ttrue(urlWriteHeaders(up, NULL) == 0);
    webSocketSetClientKey(urlGetWebSocket(up), key);
    ttrue(urlFinalize(up) < 0);
    tcontains(urlGetError(up), "client key");
    urlFree(up);
}
#endif /* ME_COM_WEBSOCK */

#if URL_AUTH
/*
    What the client puts on the wire for digest auth. Every other digest test in the suite proves
    only that the test server accepted the header; none of them can see it, because the server
    tokenises the Authorization value in place while parsing it. Reading the raw request here is
    the only way to hold the header itself to RFC 7616.
 */
static void testDigestAuthorizationHeader()
{
    Url  *up;
    char url[128];

    replyIndex = 0;
    replies[0] = "HTTP/1.1 401 Unauthorized\r\n"
                 "WWW-Authenticate: Digest realm=\"Test Realm\", qop=\"auth\", "
                 "nonce=\"abc123nonce\", opaque=\"op-value\", algorithm=SHA-256\r\n"
                 "Content-Length: 0\r\n\r\n";
    replies[1] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replies[2] = NULL;

    up = urlAlloc(0);
    urlSetAuth(up, "alice", "secret", "digest");
    ttrue(urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:%d/protected?a=1&b=2", faultPort),
                   NULL, 0, NULL) == 200);

    //  The first attempt is unauthenticated; the challenge drives the retry
    ttrue(scontains(firstRequest, "Authorization:") == 0);

    //  RFC 7616 3.4: the digest uri is the Request-URI, so it carries the query
    tcontains(lastRequest, "Authorization: Digest ");
    tcontains(lastRequest, "uri=\"/protected?a=1&b=2\"");
    tcontains(lastRequest, "username=\"alice\"");
    tcontains(lastRequest, "realm=\"Test Realm\"");
    tcontains(lastRequest, "nonce=\"abc123nonce\"");
    tcontains(lastRequest, "opaque=\"op-value\"");
    tcontains(lastRequest, "algorithm=SHA-256");
    tcontains(lastRequest, "qop=auth");
    //  The nonce count is the replay defence and must start at 1 and be 8 hex digits
    tcontains(lastRequest, "nc=00000001");
    tcontains(lastRequest, "cnonce=\"");
    tcontains(lastRequest, "response=\"");
    //  The password must never appear on the wire
    ttrue(scontains(lastRequest, "secret") == 0);

    replies[0] = NULL;
    urlFree(up);
}

/*
    Basic auth puts the credentials in a base64 blob, not in the clear
 */
static void testBasicAuthorizationHeader()
{
    Url  *up;
    char url[128];

    replyIndex = 0;
    replies[0] = "HTTP/1.1 401 Unauthorized\r\n"
                 "WWW-Authenticate: Basic realm=\"Test Realm\"\r\n"
                 "Content-Length: 0\r\n\r\n";
    replies[1] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replies[2] = NULL;

    up = urlAlloc(0);
    urlSetAuth(up, "alice", "secret", "basic");
    ttrue(urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:%d/protected", faultPort),
                   NULL, 0, NULL) == 200);

    tcontains(lastRequest, "Authorization: Basic ");
    //  YWxpY2U6c2VjcmV0 is base64 of "alice:secret"
    tcontains(lastRequest, "YWxpY2U6c2VjcmV0");
    ttrue(scontains(lastRequest, ":secret") == 0);

    replies[0] = NULL;
    urlFree(up);
}

static void testAuthOrigin()
{
    RSocket *other;
    Url     *up;
    char    ip[64], url[128];
    int     otherPort;

    other = rAllocSocket();
    if (rListenSocket(other, "127.0.0.1", 0, (RSocketProc) faultHandler, NULL) < 0 ||
        rGetSocketAddr(other, ip, sizeof(ip), &otherPort) < 0) {
        tfail("Cannot listen for a second origin");
        rFreeSocket(other);
        return;
    }
    replyIndex = 0;
    replies[0] = "HTTP/1.1 401 Unauthorized\r\n"
                 "WWW-Authenticate: Digest realm=\"r\", nonce=\"n1\", qop=\"auth\"\r\nContent-Length: 0\r\n\r\n";
    replies[1] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replies[2] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replies[3] = NULL;

    up = urlAlloc(0);
    urlSetAuth(up, "alice", "secret", NULL);
    ttrue(urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:%d/protected", faultPort), NULL, 0, NULL) == 200);
    tcontains(lastRequest, "Authorization: Digest ");

    lastRequest[0] = '\0';
    ttrue(urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:%d/other", otherPort), NULL, 0, NULL) == 200);
    tcontains(lastRequest, "GET /other ");
    ttrue(scontains(lastRequest, "Authorization:") == 0);
    ttrue(up->nonce == NULL);

    replies[0] = NULL;
    urlFree(up);
    rFreeSocket(other);
}


static void testDigestNonceRenewal()
{
    Url  *up;
    char url[128];

    replyIndex = 0;
    replies[0] = "HTTP/1.1 401 Unauthorized\r\n"
                 "WWW-Authenticate: Digest realm=\"r\", nonce=\"n1\", qop=\"auth\"\r\nContent-Length: 0\r\n\r\n";
    replies[1] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replies[2] = "HTTP/1.1 401 Unauthorized\r\n"
                 "WWW-Authenticate: Digest realm=\"r\", nonce=\"n2\", qop=\"auth\"\r\nContent-Length: 0\r\n\r\n";
    replies[3] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replies[4] = "HTTP/1.1 401 Unauthorized\r\n"
                 "WWW-Authenticate: Digest realm=\"r\", nonce=\"n3\", qop=\"auth\", stale=true\r\n"
                 "Content-Length: 0\r\n\r\n";
    replies[5] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replies[6] = NULL;

    up = urlAlloc(0);
    urlSetAuth(up, "alice", "secret", "digest");
    SFMT(url, "http://127.0.0.1:%d/protected", faultPort);

    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) == 200);
    tcontains(lastRequest, "nonce=\"n1\"");
    tcontains(lastRequest, "nc=00000001");

    //  The server moved on to a new nonce: its first use is nc=1
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) == 200);
    tcontains(lastRequest, "nonce=\"n2\"");
    tcontains(lastRequest, "nc=00000001");

    //  A stale nonce: the retry is authenticated with the new nonce
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) == 200);
    tcontains(lastRequest, "Authorization: Digest ");
    tcontains(lastRequest, "nonce=\"n3\"");
    tcontains(lastRequest, "nc=00000001");

    replies[0] = NULL;
    urlFree(up);
}

/* Refuse no-qop challenges, which lack client nonces, and clear state before later challenges. */
static void testDigestNoQop()
{
    Url  *up;
    char url[128];
    int  status;

    SFMT(url, "http://127.0.0.1:%d/protected", faultPort);

    replyIndex = 0;
    replies[0] = "HTTP/1.1 401 Unauthorized\r\n"
                 "WWW-Authenticate: Digest realm=\"r\", nonce=\"fixed\"\r\nContent-Length: 0\r\n\r\n";
    replies[1] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replies[2] = NULL;
    lastRequest[0] = '\0';
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "secret", NULL);
    status = urlFetch(up, "GET", url, NULL, 0, NULL);
    teqi(status, 401);
    ttrue(scontains(lastRequest, "Authorization:") == 0);
    urlFree(up);

    //  A later challenge that omits qop, opaque and algorithm does not inherit them
    replyIndex = 0;
    replies[0] = "HTTP/1.1 401 Unauthorized\r\n"
                 "WWW-Authenticate: Digest realm=\"r\", nonce=\"n1\", qop=\"auth\", opaque=\"o\", algorithm=SHA-256\r\n"
                 "Content-Length: 0\r\n\r\n";
    replies[1] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replies[2] = "HTTP/1.1 401 Unauthorized\r\n"
                 "WWW-Authenticate: Digest realm=\"r\", nonce=\"n2\"\r\nContent-Length: 0\r\n\r\n";
    replies[3] = NULL;
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "secret", "digest");
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) == 200);
    status = urlFetch(up, "GET", url, NULL, 0, NULL);
    teqi(status, 401);
    ttrue(up->qop == NULL);
    ttrue(up->opaque == NULL);
    ttrue(up->algorithm == NULL);
    replies[0] = NULL;
    urlFree(up);
}


static void testAuthControlChars()
{
    Url  *up;
    char url[128];

    SFMT(url, "http://127.0.0.1:%d/protected", faultPort);
    reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replyLen = slen(reply);

    lastRequest[0] = '\0';
    up = urlAlloc(0);
    urlSetAuth(up, "a\r\nX: y", "secret", "basic");
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) < 0);
    tcontains(urlGetError(up), "Invalid character");
    ttrue(lastRequest[0] == '\0');
    urlFree(up);

    //  A server supplied realm with a control character is refused too
    replyIndex = 0;
    replies[0] = "HTTP/1.1 401 Unauthorized\r\n"
                 "WWW-Authenticate: Digest realm=\"r\x01x\", nonce=\"n1\", qop=\"auth\"\r\nContent-Length: 0\r\n\r\n";
    replies[1] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replies[2] = NULL;
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "secret", "digest");
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) < 0);
    tcontains(urlGetError(up), "Invalid character");
    ttrue(scontains(lastRequest, "Authorization:") == 0);
    replies[0] = NULL;
    urlFree(up);
}
#endif /* URL_AUTH */

/*
    What the client sends for a chunked request body
 */
static void testChunkedRequestFraming()
{
    Url  *up;
    char url[128];

    lastRequest[0] = '\0';
    reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replyLen = slen(reply);

    up = urlAlloc(0);
    ttrue(urlStart(up, "POST", SFMT(url, "http://127.0.0.1:%d/upload", faultPort)) == 0);
    //  No Content-Length is declared, so the body must be chunk framed
    ttrue(urlWrite(up, "Hello", 5) == 5);
    ttrue(urlWrite(up, " World", 6) == 6);
    ttrue(urlFinalize(up) == 0);

    tcontains(lastRequest, "Transfer-Encoding: chunked");
    tcontains(lastRequest, "\r\n5\r\nHello\r\n");
    tcontains(lastRequest, "\r\n6\r\n World\r\n");
    //  The terminating zero chunk closes the body
    tcontains(lastRequest, "\r\n0\r\n\r\n");
    urlFree(up);
}

/*
    A declared Content-Length means no chunk framing at all
 */
static void testContentLengthRequestFraming()
{
    Url  *up;
    char url[128];

    lastRequest[0] = '\0';
    reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replyLen = slen(reply);

    up = urlAlloc(0);
    ttrue(urlFetch(up, "POST", SFMT(url, "http://127.0.0.1:%d/upload", faultPort),
                   "Hello", 5, NULL) == 200);

    tcontains(lastRequest, "Content-Length: 5");
    ttrue(scontains(lastRequest, "Transfer-Encoding") == 0);
    tcontains(lastRequest, "\r\n\r\nHello");
    urlFree(up);
}

/*
    A server closes an idle keep-alive connection whenever it likes, and the client cannot tell
    until a read fails. A second request on the same Url must reconnect rather than fail.
 */
static void testStaleConnectionRetry()
{
    Url  *up;
    char url[128];

    //  The unscripted fault server answers one request per connection and then closes
    reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replyLen = slen(reply);

    up = urlAlloc(0);
    SFMT(url, "http://127.0.0.1:%d/x", faultPort);

    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) == 200);
    tmatch(urlGetResponse(up), "ok");

    //  The server has closed by now. This must reconnect, not fail.
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) == 200);
    tmatch(urlGetResponse(up), "ok");
    ttrue(urlGetError(up) == NULL);

    //  And again, to show the retry is not a one-off that leaves the object wedged
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) == 200);
    tmatch(urlGetResponse(up), "ok");
    urlFree(up);
}

/*
    The retry must not paper over a real failure. A host that is not listening fails on a fresh
    connection, and a response that begins and then breaks off is not resent.
 */
static void testStaleRetryLimits()
{
    Url   *up;
    Ticks start;
    char  url[128];

    //  Nothing is listening on port 1, so there is no connection to reuse and no retry
    up = urlAlloc(0);
    start = rGetTicks();
    ttrue(urlFetch(up, "GET", "http://127.0.0.1:1/x", NULL, 0, NULL) < 0);
    ttrue((rGetTicks() - start) < 5 * TPS);
    urlFree(up);

    //  A response that starts and then breaks off is a real error, not a stale connection
    up = urlAlloc(0);
    SFMT(url, "http://127.0.0.1:%d/x", faultPort);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\npartial") == 200);
    ttrue(slen(urlGetResponse(up)) < 100);
    ttrue(urlGetError(up) != NULL);
    urlFree(up);
}

/*
    What a multipart upload puts on the wire. Every other upload test asserts only that the
    module's own server accepted the request; none has ever looked at the bytes.
 */
static void testUploadFraming()
{
    Url   *up;
    RList *files;
    RHash *forms;
    cchar *req;
    char  url[128];

    lastRequest[0] = '\0';
    reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replyLen = slen(reply);

    up = urlAlloc(0);
    ttrue(urlStart(up, "POST", SFMT(url, "http://127.0.0.1:%d/upload", faultPort)) == 0);

    files = rAllocList(0, 0);
    rAddItem(files, "site/data/test1.txt");
    rAddItem(files, "site/data/test2.txt");
    forms = rAllocHash(0, 0);
    rAddName(forms, "colour", "blue", 0);

    ttrue(urlUpload(up, files, forms, NULL) == 0);
    req = lastRequest;

    //  RFC 7230 3.3.3: without a framing header a request body is treated as absent
    tcontains(req, "Transfer-Encoding: chunked");
    tcontains(req, "Content-Type: multipart/form-data; boundary=");
    //  The headers must be terminated before the body, and the first chunk size must follow
    tcontains(req, "Transfer-Encoding: chunked\r\n\r\n");

    //  File parts are numbered from zero, in list order
    tcontains(req, "name=\"file0\"; filename=\"test1.txt\"");
    tcontains(req, "name=\"file1\"; filename=\"test2.txt\"");
    ttrue(scontains(req, "name=\"file-1\"") == 0);

    //  The form field and the file content are both carried
    tcontains(req, "name=\"colour\"");
    tcontains(req, "blue");
    //  And the multipart closes with the terminating delimiter
    tcontains(req, "--\r\n");

    rFreeList(files);
    rFreeHash(forms);
    urlFree(up);
}

/*
    The boundary the header declares must be the delimiter the body actually uses
 */
static void testUploadBoundaryAgrees()
{
    Url   *up;
    RList *files;
    char  *declared, *cp;
    char  url[128], delim[128];

    lastRequest[0] = '\0';
    reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replyLen = slen(reply);

    up = urlAlloc(0);
    ttrue(urlStart(up, "POST", SFMT(url, "http://127.0.0.1:%d/upload", faultPort)) == 0);
    files = rAllocList(0, 0);
    rAddItem(files, "site/data/test1.txt");
    ttrue(urlUpload(up, files, NULL, NULL) == 0);

    //  Read the boundary out of the Content-Type header, then look for "--" + it in the body
    declared = scontains(lastRequest, "boundary=");
    ttrue(declared != NULL);
    if (declared) {
        declared += slen("boundary=");
        if ((cp = scontains(declared, "\r\n")) != 0) {
            SFMT(delim, "--%.*s", (int) (cp - declared), declared);
            tcontains(lastRequest, delim);
        }
    }
    rFreeList(files);
    urlFree(up);
}

/*
    Set-Cookie parsing against the forms a real server sends. The live test server sets one
    well-formed cookie per request, so none of these variants had ever been exercised.
 */
static char *fetchCookie(cchar *setCookieLines, cchar *name)
{
    Url  *up;
    char *response, *value;

    response = sfmt("HTTP/1.1 200 OK\r\n%sContent-Length: 2\r\n\r\nok", setCookieLines);
    up = urlAlloc(0);
    faultFetch(up, response);
    value = urlGetCookie(up, name);
    urlFree(up);
    rFree(response);
    return value;
}

static void expectCookie(cchar *setCookieLines, cchar *name, cchar *expected)
{
    char *value;

    value = fetchCookie(setCookieLines, name);
    if (expected) {
        tmatch(value, expected);
    } else {
        ttrue(value == NULL);
    }
    rFree(value);
}

static void testCookieParsing()
{
    expectCookie("Set-Cookie: sid=abc123\r\n", "sid", "abc123");
    expectCookie("Set-Cookie: sid=abc123; Path=/; HttpOnly; Secure\r\n", "sid", "abc123");
    expectCookie("Set-Cookie: a=1; Path=/\r\nSet-Cookie: sid=abc123; Path=/\r\n", "sid", "abc123");
    expectCookie("set-cookie: sid=abc123\r\n", "sid", "abc123");

    //  A value may itself contain "=", and a quoted value keeps its quotes (RFC 6265 does not strip them)
    expectCookie("Set-Cookie: tok=a=b=c; Path=/\r\n", "tok", "a=b=c");
    expectCookie("Set-Cookie: sid=\"abc 123\"; Path=/\r\n", "sid", "\"abc 123\"");

    /*
        The whole name must match. A requested name that begins with a shorter cookie's name must
        not be answered with that cookie's value, in either order of the headers.
     */
    expectCookie("Set-Cookie: sid=RIGHT\r\nSet-Cookie: sidx=WRONG\r\n", "sidx", "WRONG");
    expectCookie("Set-Cookie: sidx=WRONG\r\nSet-Cookie: sid=RIGHT\r\n", "sid", "RIGHT");
    expectCookie("Set-Cookie: sid=RIGHT\r\nSet-Cookie: sidx=WRONG\r\n", "sid", "RIGHT");
    expectCookie("Set-Cookie: session=A\r\nSet-Cookie: session_id=B\r\n", "session_id", "B");
    expectCookie("Set-Cookie: sid=abc\r\n", "sidx", NULL);

    //  An empty value is "", whether or not attributes follow
    expectCookie("Set-Cookie: sid=; Path=/\r\n", "sid", "");
    expectCookie("Set-Cookie: sid=\r\n", "sid", "");

    //  RFC 6265 5.2 step 3: white space around the name is trimmed
    expectCookie("Set-Cookie: sid = abc123\r\n", "sid", "abc123");

    //  No "=" is not a cookie; an absent name is absent
    expectCookie("Set-Cookie: sid\r\n", "sid", NULL);
    expectCookie("Set-Cookie: sid\r\nX-After: hello\r\n", "sid", NULL);
    expectCookie("Set-Cookie: other=1\r\n", "sid", NULL);
    expectCookie("Set-Cookie: =orphan\r\n", "", NULL);
}

/*
    A caller's Cookie header must reach the wire exactly as given
 */
static void testCookieOnTheWire()
{
    Url  *up;
    char url[128];

    lastRequest[0] = '\0';
    reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replyLen = slen(reply);

    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:%d/x", faultPort), NULL, 0,
                   "Cookie: sid=abc123; theme=dark\r\n") == 200);
    tcontains(lastRequest, "Cookie: sid=abc123; theme=dark\r\n");
    urlFree(up);
}

/*
    The request-target is the path and query only. RFC 3986 3.5: a fragment is never sent to the
    server, though it stays parsed for the caller.
 */
static void testRequestTarget()
{
    Url  *up;
    char url[128];

    reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replyLen = slen(reply);

    lastRequest[0] = '\0';
    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:%d/path#section", faultPort), NULL, 0, NULL) == 200);
    tcontains(lastRequest, "GET /path HTTP/1.1\r\n");
    ttrue(scontains(lastRequest, "#") == 0);
    tmatch(up->hash, "section");
    urlFree(up);

    //  The query survives; the fragment does not
    lastRequest[0] = '\0';
    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:%d/path?a=1&b=2#section", faultPort), NULL, 0, NULL) == 200);
    tcontains(lastRequest, "GET /path?a=1&b=2 HTTP/1.1\r\n");
    ttrue(scontains(lastRequest, "#") == 0);
    urlFree(up);

    //  An empty path is "/"
    lastRequest[0] = '\0';
    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:%d", faultPort), NULL, 0, NULL) == 200);
    tcontains(lastRequest, "GET / HTTP/1.1\r\n");
    urlFree(up);
}

/*
    RFC 7230 5.4: the Host header carries an IPv6 literal in brackets, so the port is unambiguous.
    Hostnames and IPv4 addresses are sent as they are.
 */
static void testHostHeader()
{
    Url  *up;
    char url[128], expected[128];

    reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replyLen = slen(reply);

    lastRequest[0] = '\0';
    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:%d/", faultPort), NULL, 0, NULL) == 200);
    tcontains(lastRequest, SFMT(expected, "Host: 127.0.0.1:%d\r\n", faultPort));
    urlFree(up);

    if (faultPort6 == 0) {
        tinfo("No IPv6 loopback listener; skipping the IPv6 Host header case");
        return;
    }
    lastRequest[0] = '\0';
    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", SFMT(url, "http://[::1]:%d/", faultPort6), NULL, 0, NULL) == 200);
    tcontains(lastRequest, SFMT(expected, "Host: [::1]:%d\r\n", faultPort6));
    urlFree(up);
}

/*
    RFC 7230 4.1.1: a chunk-size may carry extensions after a ";" and a recipient MUST ignore any
    it does not understand. No server in the suite sends one.
 */
static void testChunkExtension()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                         "5;name=value\r\nHello\r\n6 ; quoted=\"a;b\"\r\n World\r\n0;last\r\n\r\n") == 200);
    tmatch(urlGetResponse(up), "Hello World");
    ttrue(urlGetError(up) == NULL);
    urlFree(up);
}

/*
    RFC 7230 4.1: chunk-size is 1*HEXDIG. A sign, leading space or radix prefix is not a chunk size,
    and a peer that parses strictly frames the body differently from one that does not.
 */
static void expectBadChunk(cchar *sizeLine)
{
    Url  *up;
    char *response;

    response = sfmt("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n%s\r\nHello\r\n0\r\n\r\n", sizeLine);
    up = urlAlloc(0);
    ttrue(faultFetch(up, response) == 200);
    ttrue(!smatch(urlGetResponse(up), "Hello"), "chunk size \"%s\" was accepted", sizeLine);
    tcontains(urlGetError(up), "chunk");
    urlFree(up);
    rFree(response);
}

static void testChunkSizeSyntax()
{
    char longLine[256];

    expectBadChunk("+5");
    expectBadChunk(" 5");
    expectBadChunk("0x5");
    expectBadChunk("5 5");
    expectBadChunk("5 ");
    expectBadChunk("5\t");
    expectBadChunk("");
    //  A chunk-size line longer than the client will hold must be rejected, not partly consumed
    SFMT(longLine, "5;%0200d", 0);
    expectBadChunk(longLine);
}

/*
    RFC 7230 4.1.2: a chunked body may end with trailer fields before the final blank line. A
    client that does not read them leaves them on the connection as the start of the next response.
 */
static void testChunkTrailers()
{
    Url  *up;
    char url[128];

    replyIndex = 0;
    replies[0] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                 "5\r\nHello\r\n0\r\nExpires: Thu, 01 Jan 2026 00:00:00 GMT\r\nX-Checksum: abc\r\n\r\n";
    replies[1] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nyes";
    replies[2] = NULL;

    up = urlAlloc(0);
    SFMT(url, "http://127.0.0.1:%d/fault", faultPort);
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) == 200);
    tmatch(urlGetResponse(up), "Hello");
    ttrue(urlGetError(up) == NULL);
    //  The whole message, trailers included, has been consumed
    ttrue(rGetBufLength(up->rx) == 0);

    //  So the next response on the same connection is read cleanly
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) == 200);
    tmatch(urlGetResponse(up), "yes");
    ttrue(urlGetError(up) == NULL);

    replies[0] = NULL;
    urlFree(up);
}

/*
    Send one byte at a time to test NUL rejection across CRLF scan boundaries.
 */
static void testChunkSizeNul()
{
    static cchar response[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\0x\r\nHello\r\n0\r\n\r\n";
    Url  *up;
    char url[128];

    reply = response;
    replyLen = sizeof(response) - 1;
    dribble = 1;
    dribbleDelay = 5;
    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:%d/fault", faultPort), NULL, 0, NULL) == 200);
    ttrue(!smatch(urlGetResponse(up), "Hello"));
    tcontains(urlGetError(up), "chunk");
    urlFree(up);
    dribble = 0;
    dribbleDelay = 0;
}

/*
    Use short trailer lines to test the aggregate limit independently of per-line limits.
 */
static void testTrailerLimit()
{
    Url  *up;
    RBuf *buf;
    int  i;

    buf = rAllocBuf(0);
    rPutStringToBuf(buf, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n");
    for (i = 0; i < 4000; i++) {
        rPutStringToBuf(buf, "X-Trailer: 0123456789\r\n");
    }
    rPutStringToBuf(buf, "\r\n");

    up = urlAlloc(0);
    ttrue(faultFetch(up, rBufToString(buf)) == 200);
    urlGetResponse(up);
    tcontains(urlGetError(up), "trailers too big");
    urlFree(up);
    rFreeBuf(buf);
}

/*
    An interim response must not leave framing or Location state for the final response.
 */
static void testInterimFramingReset()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 100 Continue\r\nTransfer-Encoding: chunked\r\nLocation: /interim\r\n\r\n"
                         "HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\n0123456789") == 200);
    tmatch(urlGetResponse(up), "0123456789");
    ttrue(up->redirect == NULL);
    ttrue(urlGetError(up) == NULL);
    urlFree(up);
}


static void testObsFold()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nX-Note: a\r\n Content-Length: 3\r\n\r\nabcdef") < 0);
    tcontains(urlGetError(up), "Bad header");
    urlFree(up);

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nX-Note: a\r\n\tmore\r\nContent-Length: 3\r\n\r\nabc") < 0);
    urlFree(up);
}


static void testUnlimitedBufLimit()
{
    Url  *up;
    RBuf *buf;
    int  i;

    buf = rAllocBuf(0);
    rPutStringToBuf(buf, "HTTP/1.1 200 OK\r\nContent-Length: 9000000\r\n\r\n");
    for (i = 0; i < 1024; i++) {
        rPutStringToBuf(buf, "0123456789abcdef");
    }
    up = urlAlloc(0);
    urlSetBufLimit(up, 0);
    ttrue(faultFetch(up, rBufToString(buf)) == 200);
    urlGetResponse(up);
    tcontains(urlGetError(up), "too big");
    urlFree(up);
    rFreeBuf(buf);

    //  A response that fits is unaffected
    up = urlAlloc(0);
    urlSetBufLimit(up, 0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHELLO") == 200);
    tmatch(urlGetResponse(up), "HELLO");
    ttrue(urlGetError(up) == NULL);
    urlFree(up);
}


static void testLongHostReuse()
{
    Url  *up;
    char host[301], *url;

    memset(host, 'a', sizeof(host) - 1);
    host[sizeof(host) - 1] = '\0';
    url = sfmt("http://%s/x", host);

    up = urlAlloc(0);
    ttrue(urlStart(up, "GET", url) < 0);
    ttrue(urlStart(up, "GET", url) < 0);
    ttrue(urlGetError(up) != NULL);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok") == 200);
    tmatch(urlGetResponse(up), "ok");
    urlFree(up);
    rFree(url);
}

/*
    Wait for the runtime's default timeout; no overall timeout is configured.
 */
static void testStalledServer()
{
    Url   *up;
    Ticks start, elapsed;

    up = urlAlloc(0);
    ttrue(up->deadline == 0);
    stall = 1;
    start = rGetTicks();
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok") < 0);
    elapsed = rGetTicks() - start;
    tinfo("Stalled server failed after %lld ms", (long long) elapsed);
    ttrue(elapsed < 45 * TPS);
    ttrue(urlGetError(up) != NULL);
    stall = 0;
    urlFree(up);


    up = urlAlloc(0);
    urlSetTimeout(up, -1);
    ttrue(up->deadline == MAXINT64);
    urlSetTimeout(up, 0);
    ttrue(up->deadline == 0);
    urlFree(up);
}

/*
    RFC 7230 3.3.2: Content-Length is 1*DIGIT and anything else is invalid. A lenient parse that
    takes the leading digits, or reads garbage as zero, frames the body differently from a peer
    that rejects it, and a zero is silently an empty response.
 */
static void expectBadContentLength(cchar *value)
{
    Url  *up;
    char *response;

    response = sfmt("HTTP/1.1 200 OK\r\nContent-Length: %s\r\n\r\nHello", value);
    up = urlAlloc(0);
    ttrue(faultFetch(up, response) < 0, "Content-Length \"%s\" was accepted", value);
    tcontains(urlGetError(up), "Content-Length");
    urlFree(up);
    rFree(response);
}

static void testContentLengthSyntax()
{
    expectBadContentLength("5abc");
    expectBadContentLength("+5");
    expectBadContentLength("0x5");
    expectBadContentLength("5 5");
    expectBadContentLength("abc");
    expectBadContentLength("");
}

/*
    urlWriteFile must report what it did: the bytes written on success, an error on failure.
    Every caller in the suite writes a file the module's own server accepts, so neither the count
    nor a failed write had ever been checked.
 */
static void testWriteFileResult()
{
    Url   *up;
    cchar *end;
    ssize headerLen;
    char  url[128];

    reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replyLen = slen(reply);
    SFMT(url, "http://127.0.0.1:%d/upload", faultPort);

    //  A body of unknown length is chunked, and the count is the file size, not the framing
    up = urlAlloc(0);
    ttrue(urlStart(up, "POST", url) == 0);
    ttrue(urlWriteFile(up, "site/size/10K.txt") == 10266);
    ttrue(urlFinalize(up) == 0);
    ttrue(urlGetStatus(up) == 200);
    tcontains(lastRequest, "Transfer-Encoding: chunked");
    ttrue(requestTotal > 10266);
    urlFree(up);

    //  With a declared Content-Length the body goes unframed and the server sees exactly the file
    up = urlAlloc(0);
    ttrue(urlStart(up, "POST", url) == 0);
    ttrue(urlWriteHeaders(up, "Content-Length: 102516\r\n") == 0);
    ttrue(urlWriteFile(up, "site/size/100K.txt") == 102516);
    ttrue(urlFinalize(up) == 0);
    ttrue(urlGetStatus(up) == 200);
    end = scontains(lastRequest, "\r\n\r\n");
    ttrue(end != NULL);
    if (end) {
        headerLen = (end - lastRequest) + 4;
        ttrue(requestTotal == headerLen + 102516);
    }
    urlFree(up);

    //  A server that drops the connection mid-body makes the write fail, and that must be reported
    closeOnAccept = 1;
    up = urlAlloc(0);
    ttrue(urlStart(up, "POST", url) == 0);
    ttrue(urlWriteFile(up, "site/size/10M.txt") < 0);
    ttrue(urlGetError(up) != NULL);
    closeOnAccept = 0;
    urlFree(up);
}

/*
    HEAD on both sides of the wire. Sent through urlStart and urlFinalize with no body, the request
    must not be chunk framed. Received, the response has no body whatever Content-Length says, and
    the connection stays usable for the next request.
 */
static void testHeadFraming()
{
    Url  *up;
    char url[128];

    reply = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    replyLen = slen(reply);
    SFMT(url, "http://127.0.0.1:%d/fault", faultPort);

    lastRequest[0] = '\0';
    up = urlAlloc(0);
    ttrue(urlStart(up, "HEAD", url) == 0);
    ttrue(urlFinalize(up) == 0);
    ttrue(urlGetStatus(up) == 200);
    tcontains(lastRequest, "HEAD /fault HTTP/1.1\r\n");
    ttrue(scontains(lastRequest, "Transfer-Encoding") == 0);
    ttrue(scontains(lastRequest, "\r\n0\r\n\r\n") == 0);
    urlFree(up);

    replyIndex = 0;
    replies[0] = "HTTP/1.1 200 OK\r\nContent-Length: 1234\r\nContent-Type: text/plain\r\n\r\n";
    replies[1] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nyes";
    replies[2] = NULL;

    up = urlAlloc(0);
    ttrue(urlFetch(up, "HEAD", url, NULL, 0, NULL) == 200);
    tmatch(urlGetHeader(up, "Content-Length"), "1234");
    tmatch(urlGetResponse(up), "");
    ttrue(urlGetError(up) == NULL);

    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) == 200);
    tmatch(urlGetResponse(up), "yes");
    ttrue(urlGetError(up) == NULL);

    replies[0] = NULL;
    urlFree(up);
}

/*
    Redirects are surfaced, not followed: the status and Location reach the caller and nothing
    is fetched on their behalf. The convenience calls treat a redirect as a failure.
 */
static void testRedirectSurfaced()
{
    Url  *up;
    char *response;
    char url[128];

    reply = "HTTP/1.1 301 Moved Permanently\r\nLocation: http://example.com/new\r\nContent-Length: 5\r\n\r\nmoved";
    replyLen = slen(reply);
    SFMT(url, "http://127.0.0.1:%d/old", faultPort);

    lastRequest[0] = '\0';
    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) == 301);
    tmatch(up->redirect, "http://example.com/new");
    tmatch(urlGetHeader(up, "Location"), "http://example.com/new");
    tmatch(urlGetResponse(up), "moved");
    ttrue(urlGetError(up) == NULL);
    tcontains(lastRequest, "GET /old HTTP/1.1\r\n");
    urlFree(up);

    response = urlGet(url, NULL);
    ttrue(response == NULL);
    tcontains(lastRequest, "GET /old HTTP/1.1\r\n");
    rFree(response);
}

/*
    The path and query are sent as given. The caller is responsible for percent-encoding, and an
    encoded path must not be encoded again.
 */
static void testEncodedPathVerbatim()
{
    Url  *up;
    char url[128];

    reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replyLen = slen(reply);

    lastRequest[0] = '\0';
    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:%d/a%%20b/c%%2Fd?q=%%26x&y=%%3D", faultPort),
                   NULL, 0, NULL) == 200);
    tcontains(lastRequest, "GET /a%20b/c%2Fd?q=%26x&y=%3D HTTP/1.1\r\n");
    urlFree(up);
}

/*
    A caller that asks for Expect: 100-continue gets the interim response skipped and the final
    one delivered. The client sends the body without waiting, which RFC 7231 5.1.1 permits.
 */
static void testExpectContinue()
{
    Url  *up;
    char url[128];

    reply = "HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replyLen = slen(reply);

    lastRequest[0] = '\0';
    up = urlAlloc(0);
    ttrue(urlFetch(up, "POST", SFMT(url, "http://127.0.0.1:%d/fault", faultPort), "Hello", 5,
                   "Expect: 100-continue\r\n") == 200);
    tmatch(urlGetResponse(up), "ok");
    tcontains(lastRequest, "Expect: 100-continue\r\n");
    tcontains(lastRequest, "\r\n\r\nHello");
    urlFree(up);
}

/*
    A header block that never ends must not be buffered without limit. The response body has a
    limit; the headers had none, so a hostile server could grow the client until the device ran
    out of memory.
 */
static void testOversizedHeaders()
{
    Url    *up;
    char   *response;
    size_t size;

    size = 2 * 1024 * 1024;
    response = rAlloc(size + 64);
    memset(response, 'a', size + 64);
    memcpy(response, "HTTP/1.1 200 OK\r\nX-Pad: ", 24);
    response[size + 63] = '\0';

    up = urlAlloc(0);
    ttrue(faultFetch(up, response) < 0);
    ttrue(urlGetError(up) != NULL);
    //  The client gave up well before it had swallowed the whole block
    ttrue(rGetBufLength(up->rx) < size / 2);
    urlFree(up);
    rFree(response);
}

/*
    The configured auth type is a floor. A server cannot talk a Digest client down to Basic, a
    client with no configured type sends nothing until challenged and answers a Basic challenge
    only over TLS, and a Basic client answers a Digest challenge with Digest.
 */
static void testAuthFloor()
{
    Url  *up;
    char url[128];

    SFMT(url, "http://127.0.0.1:%d/protected", faultPort);

    //  Digest configured, server offers only Basic: the password must not go out
    replyIndex = 0;
    replies[0] = "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"r\"\r\nContent-Length: 0\r\n\r\n";
    replies[1] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replies[2] = NULL;
    lastRequest[0] = '\0';
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "secret", "digest");
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) < 0);
    tcontains(urlGetError(up), "Basic");
    ttrue(scontains(lastRequest, "Authorization:") == 0);
    urlFree(up);

    //  No configured type: nothing is sent before a challenge
    replies[0] = NULL;
    reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replyLen = slen(reply);
    lastRequest[0] = '\0';
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "secret", NULL);
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) == 200);
    ttrue(scontains(lastRequest, "Authorization:") == 0);
    urlFree(up);

    //  No configured type, Basic challenge over plain http: refused rather than disclose the password
    replyIndex = 0;
    replies[0] = "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"r\"\r\nContent-Length: 0\r\n\r\n";
    replies[1] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replies[2] = NULL;
    lastRequest[0] = '\0';
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "secret", NULL);
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) < 0);
    tcontains(urlGetError(up), "insecure");
    ttrue(scontains(lastRequest, "Authorization:") == 0);
    ttrue(scontains(lastRequest, "c2VjcmV0") == 0);
    urlFree(up);

    //  Basic configured, Digest challenge: the retry is Digest, which is stronger, not Basic again
    replyIndex = 0;
    replies[0] = "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Digest realm=\"r\", nonce=\"n1\", qop=\"auth\"\r\n"
                 "Content-Length: 0\r\n\r\n";
    replies[1] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replies[2] = NULL;
    lastRequest[0] = '\0';
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "secret", "basic");
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) == 200);
    tcontains(firstRequest, "Authorization: Basic ");
    tcontains(lastRequest, "Authorization: Digest ");
    tmatch(up->authType, "basic");
    urlFree(up);
    replies[0] = NULL;
}

/*
    A server that streams interim responses without end must be cut off, not followed forever
 */
static void testInterimFlood()
{
    Url  *up;
    RBuf *buf;
    int  i;

    buf = rAllocBuf(0);
    for (i = 0; i < 100; i++) {
        rPutStringToBuf(buf, "HTTP/1.1 100 Continue\r\n\r\n");
    }
    rPutStringToBuf(buf, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");

    up = urlAlloc(0);
    ttrue(faultFetch(up, rBufToString(buf)) < 0);
    tcontains(urlGetError(up), "interim");
    urlFree(up);
    rFreeBuf(buf);
}

/*
    RFC 7230 3.3.3: only "chunked" as the final transfer coding delimits the message. Any other
    coding cannot be decoded here and must not be mistaken for chunked.
 */
static void testTransferEncodingValue()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nTransfer-Encoding: identity\r\nContent-Length: 5\r\n\r\nHello") < 0);
    tcontains(urlGetError(up), "Transfer-Encoding");
    urlFree(up);

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip\r\n\r\n5\r\nHello\r\n0\r\n\r\n") < 0);
    tcontains(urlGetError(up), "Transfer-Encoding");
    urlFree(up);

    //  Chunked as the final coding of a list, and in any case, is still chunked
    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip, chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n") == 200);
    tmatch(urlGetResponse(up), "Hello");
    urlFree(up);

    up = urlAlloc(0);
    ttrue(faultFetch(up, "HTTP/1.1 200 OK\r\nTransfer-Encoding: Chunked \r\n\r\n5\r\nHello\r\n0\r\n\r\n") == 200);
    tmatch(urlGetResponse(up), "Hello");
    urlFree(up);
}

/*
    A URL from a remote party, such as a redirect or an upload location, must not be able to put
    CR LF or a space into the request line and so inject headers or a second request
 */
static void testRequestSplitting()
{
    Url  *up;
    char url[256];

    reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    replyLen = slen(reply);

    lastRequest[0] = '\0';
    up = urlAlloc(0);
    SFMT(url, "http://127.0.0.1:%d/x HTTP/1.1\r\nX-Injected: yes\r\n\r\nDELETE /victim", faultPort);
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) < 0);
    tcontains(urlGetError(up), "Invalid character");
    ttrue(lastRequest[0] == '\0');
    urlFree(up);

    lastRequest[0] = '\0';
    up = urlAlloc(0);
    SFMT(url, "http://127.0.0.1:%d/x?a=b%%0d%%0aX: y", faultPort);
    ttrue(urlFetch(up, "GET", url, NULL, 0, NULL) < 0);
    ttrue(lastRequest[0] == '\0');
    urlFree(up);
}

/*
    A timeout must hold against a server that keeps the connection alive by sending a byte at a
    time. The socket enforces the deadline only while waiting, and such a server never makes the
    client wait.
 */
static void testSlowResponseTimeout()
{
    Url   *up;
    Ticks start;
    char  *response;

    response = rAlloc(2048);
    memset(response, 'a', 2047);
    memcpy(response, "HTTP/1.1 200 OK\r\nX-Pad: ", 24);
    response[2047] = '\0';

    dribble = 1;
    dribbleDelay = 20;
    up = urlAlloc(0);
    urlSetTimeout(up, TPS);
    start = rGetTicks();
    ttrue(faultFetch(up, response) < 0);
    ttrue(urlGetError(up) != NULL);
    ttrue((rGetTicks() - start) < 3 * TPS);
    urlFree(up);
    dribble = 0;
    dribbleDelay = 0;
    rFree(response);
}

static void testFiber()
{
    if (!startFaultServer()) {
        rStop();
        return;
    }
    testShortHeader();
    testNoStatusCode();
    testBadStatusCode();
    testResponseLineSyntax();
    testMalformedHeader();
    testEmptyHeaderValue();
    testNegativeContentLength();
    testOversizedContentLength();
    testTruncatedBody();
    testBadChunkSpecification();
    testTruncatedChunk();
    testGoodChunks();
    testNoResponse();
    testDribbledResponse();
    testInterimResponse();
    testBodilessStatus();
    testConflictingContentLength();
    testFramingPrecedence();
    testCloseDelimitedBody();
    testChunkedRequestFraming();
    testContentLengthRequestFraming();
    testStaleConnectionRetry();
    testStaleRetryLimits();
    testUploadFraming();
    testUploadBoundaryAgrees();
    testCookieParsing();
    testCookieOnTheWire();
    testRequestTarget();
    testHostHeader();
    testChunkExtension();
    testChunkSizeSyntax();
    testChunkTrailers();
    testChunkSizeNul();
    testTrailerLimit();
    testInterimFramingReset();
    testObsFold();
    testUnlimitedBufLimit();
    testLongHostReuse();
    testContentLengthSyntax();
    testWriteFileResult();
    testHeadFraming();
    testRedirectSurfaced();
    testEncodedPathVerbatim();
    testExpectContinue();
    testOversizedHeaders();
    testInterimFlood();
    testTransferEncodingValue();
    testRequestSplitting();
    testSlowResponseTimeout();
    testStalledServer();
#if URL_AUTH
    testDigestAuthorizationHeader();
    testBasicAuthorizationHeader();
    testAuthFloor();
    testAuthOrigin();
    testDigestNonceRenewal();
    testDigestNoQop();
    testAuthControlChars();
#endif
#if URL_SSE
    testSseFields();
    testSseComment();
    testSseUnknownFields();
    testSseNoData();
    testSseMalformed();
    testSseRequestHeaders();
    testSseFreeInCallback();
#endif
#if ME_COM_WEBSOCK
    testForgedWebSocketHandshake();
    testWebSocketClientKey();
#endif
    stopFaultServer();
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
