/*
    body-max-buf.tst.c - A body limit at or above the buffer ceiling must not admit a body it cannot hold

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4856"

#define CONFIG                                                              \
    "{web: {documents: './site', listen: ['" ENDPOINT "'],"                 \
    " limits: {body: '16MB'}, timeouts: {receive: '60secs'},"               \
    " routes: [{match: '/test/', handler: 'action'}, {}]}}"

/************************************ Code ************************************/

/* Send headers alone to isolate Content-Length rejection from body-write races. */
static int postBody(size_t len, bool send)
{
    cchar  *segments[2];
    char   *headers, *body, buf[4096];
    int    status;

    headers = sfmt("POST /test/success HTTP/1.1\r\nHost: x\r\nContent-Type: application/x-www-form-urlencoded\r\n"
                   "Content-Length: %lld\r\nConnection: close\r\n\r\n", (long long) len);
    body = rAlloc(len + 1);
    memset(body, 'a', len);
    body[1] = '=';
    body[len] = '\0';
    segments[0] = headers;
    segments[1] = body;
    status = rawRequest(ENDPOINT, segments, send ? 2 : 1, 0, 1, buf, sizeof(buf));
    rFree(headers);
    rFree(body);
    return status;
}

static void testBodyAtBufferCeiling(void)
{
    WebHost *host;

    if ((host = startTestHost(CONFIG)) == 0) {
        return;
    }
    teqi(host->maxBody, ME_R_MAX_BUF - 1, "web.limits.body must be clamped below the buffer ceiling");

    teqi(postBody(ME_R_MAX_BUF - 1, 1), 200, "A body that fits the buffer must be accepted");
    teqi(postBody(ME_R_MAX_BUF, 0), 413, "A body that fills the buffer must be refused");
    teqi(postBody(ME_R_MAX_BUF + 1024 * 1024, 0), 413, "A body past the buffer ceiling must be refused, not truncated");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (setup(NULL, NULL)) {
        testBodyAtBufferCeiling();
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
