/*
    item-lifetime.tst.c - webWriteItem and webWriteItems send the item as it was serialized

    A database item's string form is cached in the item, and the next serialization of that item
    frees the previous one. A response helper that writes that cached string can yield on a full
    socket, and a second request serializing the same item then frees the buffer the first is
    still sending. The first client then receives freed memory.

    Client A shrinks its receive buffer and pipelines requests without reading, so the agent's
    write blocks and yields. Client B then requests the same item. Every one of A's bodies must
    still equal the item's JSON.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "../utils/scenario.h"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/*********************************** Locals ***********************************/

#define PIPELINED 10

/************************************ Code ************************************/
/*
    GET a document and return its body, or NULL
 */
static char *fetchBody(cchar *url)
{
    Url  *up;
    char *body;

    up = urlAlloc(0);
    body = NULL;
    if (urlFetch(up, "GET", url, NULL, 0, NULL) == 200) {
        body = sclone(urlGetResponse(up));
    }
    urlFree(up);
    return body;
}

/*
    Open a TCP connection to the HTTP endpoint with a small receive buffer
 */
static int openStalled(void)
{
    struct addrinfo hints, *res;
    struct timeval  tv;
    char            host[128], *port;
    int             fd, size;

    scopy(host, sizeof(host), &HTTP[7]);
    if ((port = schr(host, ':')) == 0) {
        return -1;
    }
    *port++ = '\0';
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        return -1;
    }
    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    size = 2048;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

/*
    Read until the peer closes or the read times out
 */
static RBuf *readAll(int fd)
{
    RBuf    *buf;
    char    chunk[64 * 1024];
    ssize_t n;

    buf = rAllocBuf(0);
    while ((n = recv(fd, chunk, sizeof(chunk), 0)) > 0) {
        rPutBlockToBuf(buf, chunk, (size_t) n);
    }
    rAddNullToBuf(buf);
    return buf;
}

/*
    Parse one HTTP/1.1 response from *pp, returning its body (chunked or Content-Length) and
    advancing *pp past it. Returns NULL on a malformed or truncated response.
 */
static char *parseResponse(cchar **pp, cchar *end)
{
    RBuf  *body;
    cchar *cp, *hdrEnd, *te, *cl;
    char  *result;
    ssize len;

    cp = *pp;
    if ((hdrEnd = sncontains(cp, "\r\n\r\n", (size_t) (end - cp))) == 0) {
        return NULL;
    }
    body = rAllocBuf(0);
    te = sncaselesscontains(cp, "Transfer-Encoding: chunked", (size_t) (hdrEnd - cp));
    cl = sncaselesscontains(cp, "Content-Length:", (size_t) (hdrEnd - cp));
    cp = hdrEnd + 4;
    if (te) {
        while (cp < end) {
            len = (ssize) strtol(cp, NULL, 16);
            if ((cp = sncontains(cp, "\r\n", (size_t) (end - cp))) == 0) {
                break;
            }
            cp += 2;
            if (len == 0) {
                cp += 2;
                break;
            }
            if (cp + len + 2 > end) {
                cp = 0;
                break;
            }
            rPutBlockToBuf(body, cp, (size_t) len);
            cp += len + 2;
        }
    } else if (cl) {
        len = (ssize) stoi(cl + 15);
        if (cp + len > end) {
            cp = 0;
        } else {
            rPutBlockToBuf(body, cp, (size_t) len);
            cp += len;
        }
    } else {
        cp = 0;
    }
    if (!cp || cp > end) {
        rFreeBuf(body);
        return NULL;
    }
    *pp = cp;
    rAddNullToBuf(body);
    result = sclone(rGetBufStart(body));
    rFreeBuf(body);
    return result;
}

static void testLifetime(cchar *path)
{
    RBuf  *buf;
    cchar *cp, *end;
    char  url[256], request[256], last[256], *expected, *body, *other;
    int   fd, i, matched;

    SFMT(url, "%s%s", HTTP, path);
    expected = fetchBody(url);
    ttrue(expected && slen(expected) > 200 * 1024);
    if (!expected) {
        return;
    }

    //  Client A: pipeline requests and do not read, so the agent's write yields on a full socket
    fd = openStalled();
    ttrue(fd >= 0);
    if (fd < 0) {
        rFree(expected);
        return;
    }
    SFMT(request, "GET %s HTTP/1.1\r\nHost: localhost\r\n\r\n", path);
    SFMT(last, "GET %s HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n", path);
    for (i = 0; i < PIPELINED - 1; i++) {
        send(fd, request, slen(request), 0);
    }
    send(fd, last, slen(last), 0);
    rSleep(1000);

    //  Client B: serialize the same item while A's response is suspended mid-write
    other = fetchBody(url);
    ttrue(smatch(other, expected));
    rFree(other);

    //  Every one of A's bodies is the item's JSON
    buf = readAll(fd);
    close(fd);
    cp = rGetBufStart(buf);
    end = cp + rGetBufLength(buf);
    for (i = 0, matched = 0; i < PIPELINED; i++) {
        if ((body = parseResponse(&cp, end)) == 0) {
            break;
        }
        if (smatch(body, expected)) {
            matched++;
        }
        rFree(body);
    }
    if (matched != PIPELINED) {
        tinfo("  %s: %d of %d pipelined bodies matched the item", path, matched, PIPELINED);
    }
    teqi(matched, PIPELINED, "Every pipelined body matches the item");
    rFreeBuf(buf);
    rFree(expected);
}

static void fiberMain(void *data)
{
    if (scenarioSetup()) {
        testLifetime("/api/public/item");
        testLifetime("/api/public/items");
    }
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
