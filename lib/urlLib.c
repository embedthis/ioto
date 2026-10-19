/*
    urlLib.c -- URL HTTP Client Library Source

    This file is a catenation of all the source code. Amalgamating into a
    single file makes embedding simpler and the resulting application faster,
    by using compiler optimization within the URL library.

    Prepared by: buildLib.sh
 */

#include "url.h"

#if ME_COM_URL


/********* Start of file src/urlLib.c ************/

/**
    url.c - URL client HTTP library.

    This is a pragmatic, compact http client. It does not attempt to be fully HTTP/1.1 compliant.
    It supports HTTP/1 keep-alive and transfer-chunking encoding.
    This module uses fiber coroutines to permit parallel execution with other application fibers.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "crypt.h"
#include    "websock.h"

#if ME_COM_URL
/*********************************** Locals ***********************************/

#define URL_CHUNK_START      1                 /**< Start of a new chunk */
#define URL_CHUNK_DATA       2                 /**< Start of chunk data */
#define URL_CHUNK_EOF        4                 /**< End of all chunk data */

#ifndef ME_URL_TIMEOUT
    #define ME_URL_TIMEOUT   0                 /**< Default timeout (inactivity bounded by the runtime) */
#endif

#ifndef URL_MAX_RESPONSE
    #define URL_MAX_RESPONSE (1024 * 1024)     /**< Maximum response size */
#endif

#ifndef URL_MAX_RETRIES
    #define URL_MAX_RETRIES  0                 /**< Maximum number of retries */
#endif

#ifndef URL_MAX_HEADERS
    #define URL_MAX_HEADERS  (64 * 1024)       /**< Maximum response header block, and chunk trailer, size */
#endif

#define URL_BUFSIZE          4096              /**< Buffer size */
#define URL_CHUNK_LINE       128               /**< Maximum chunk-size line, extensions included */
#define URL_MAX_INTERIM      8                 /**< Maximum 1xx interim responses before the final one */
#define URL_UNLIMITED        MAXINT            /**< Unlimited size */
#define MAX_DIGEST_PARAM_LEN 8192              /**< Max length for digest auth parameters (DoS prevention) */

#define HDR_HTTP             (1 << 0)          /**< HTTP header */
#define HDR_SSE              (1 << 1)          /**< SSE header */

#define isWhite(c) ((c) == ' ' || (c) == '\t') /**< Is white space */

static cchar validHeaderChars[128] = {
    ['A'] = 1, ['B'] = 1, ['C'] = 1, ['D'] = 1, ['E'] = 1, ['F'] = 1, ['G'] = 1,
    ['H'] = 1, ['I'] = 1, ['J'] = 1, ['K'] = 1, ['L'] = 1, ['M'] = 1, ['N'] = 1,
    ['O'] = 1, ['P'] = 1, ['Q'] = 1, ['R'] = 1, ['S'] = 1, ['T'] = 1, ['U'] = 1,
    ['V'] = 1, ['W'] = 1, ['X'] = 1, ['Y'] = 1, ['Z'] = 1,
    ['a'] = 1, ['b'] = 1, ['c'] = 1, ['d'] = 1, ['e'] = 1, ['f'] = 1, ['g'] = 1,
    ['h'] = 1, ['i'] = 1, ['j'] = 1, ['k'] = 1, ['l'] = 1, ['m'] = 1, ['n'] = 1,
    ['o'] = 1, ['p'] = 1, ['q'] = 1, ['r'] = 1, ['s'] = 1, ['t'] = 1, ['u'] = 1,
    ['v'] = 1, ['w'] = 1, ['x'] = 1, ['y'] = 1, ['z'] = 1,
    ['0'] = 1, ['1'] = 1, ['2'] = 1, ['3'] = 1, ['4'] = 1, ['5'] = 1, ['6'] = 1, ['7'] = 1, ['8'] = 1, ['9'] = 1,
    ['!'] = 1, ['#'] = 1, ['$'] = 1, ['%'] = 1, ['&'] = 1, ['\''] = 1, ['*'] = 1,
    ['+'] = 1, ['-'] = 1, ['.'] = 1, ['^'] = 1, ['_'] = 1, ['`'] = 1, ['|'] = 1, ['~'] = 1
};

static Ticks timeout = ME_URL_TIMEOUT;

/*********************************** Forwards *********************************/

static int connectHost(Url *up);
static int fetch(Url *up, cchar *method, cchar *uri, cvoid *data, size_t len, cchar *headers);
static ssize getContentLength(cchar *headers, size_t length);
static char *getHeader(char *line, char **key, char **value, int flags);
static bool isprintable(cchar *s, size_t len);
static bool isWireSafe(cchar *s);
static void parseEvents(Url *up);
static bool parseHeaders(Url *up);
static int parseResponse(Url *up, size_t size);
static ssize readBlock(Url *up, char *buf, size_t bufsize);
static ssize readChunk(Url *up, char *buf, size_t bufsize);
static int readHeaders(Url *up);
static ssize readSocket(Url *up, size_t bufsize);
static ssize readUntil(Url *up, cchar *until, size_t limit);
static void resetState(Url *up);
static void resetSocket(Url *up);
static bool isExpired(Url *up);
static void setDeadline(Url *up);
static int writeChunkDivider(Url *up, size_t len);
#if ME_COM_WEBSOCK
static int verifyWebSocket(Url *up);
static ssize addWebSocketHeaders(Url *up, RBuf *buf);
#endif
#if ME_COM_SSL
static void applyTls(Url *up);
#endif

#if URL_AUTH
static char *buildAuthHeader(Url *up);
static char *buildBasicAuthHeader(Url *up);
static char *buildDigestAuthHeader(Url *up);
static void clearChallenge(Url *up);
static char *getOrigin(Url *up);
static bool hasControlChars(cchar *s);
#endif

/************************************ Code ************************************/

PUBLIC Url *urlAlloc(int flags)
{
    Url   *up;
    cchar *show;

    up = rAllocType(Url);
    up->rx = rAllocBuf(URL_BUFSIZE);
    up->sock = rAllocSocket();
    up->protocol = 1;
    up->sse = 0;
    up->timeout = timeout;
    up->deadline = 0;
    up->bufLimit = URL_MAX_RESPONSE;
    up->verifyPeer = -1;
    up->verifyIssuer = -1;

    if ((flags & URL_SHOW_FLAGS) == 0) {
        /*
            SECURITY Acceptable: acceptable risk to use getenv here to modify log level
            These flags are only used at development time for debugging purposes.
            On embedded systems, the environment is controlled by the developer.
         */
        if ((show = getenv("URL_SHOW")) != 0) {
            if (schr(show, 'H')) {
                flags |= URL_SHOW_REQ_HEADERS;
            }
            if (schr(show, 'B')) {
                flags |= URL_SHOW_REQ_BODY;
            }
            if (schr(show, 'h')) {
                flags |= URL_SHOW_RESP_HEADERS;
            }
            if (schr(show, 'b')) {
                flags |= URL_SHOW_RESP_BODY;
            }
            if (flags == 0) {
                flags = URL_SHOW_NONE;
            }
        }
    }
    up->flags = flags;
    return up;
}

PUBLIC void urlFree(Url *up)
{
    if (!up) {
        return;
    }
    if (up->inCallback) {
        up->needFree = 1;
        return;
    }
    urlClose(up);
    rFreeBuf(up->rx);
    rFreeBuf(up->responseBuf);
    rFreeBuf(up->rxHeaders);
    rFreeBuf(up->txHeaders);
    rFree(up->error);
    rFree(up->response);
    rFree(up->method);
    rFree(up->url);
    rFree(up->urlbuf);
    rFree(up->boundary);
    rFree(up->caFile);
    rFree(up->keyFile);
    rFree(up->certFile);
    rFree(up->revokeFile);
    rFree(up->ciphers);
#if URL_AUTH
    rFree(up->authType);
    rFree(up->username);
    rFree(up->password);
    rFree(up->realm);
    rFree(up->nonce);
    rFree(up->qop);
    rFree(up->opaque);
    rFree(up->algorithm);
    rFree(up->authOrigin);
#endif /* URL_AUTH */
#if ME_COM_WEBSOCK
    if (up->webSocket) {
        webSocketFree(up->webSocket);
    }
#endif /* ME_COM_WEBSOCK */
    memset(up, 0, sizeof(Url));
    rFree(up);
}

static void resetState(Url *up)
{
    if (!up) {
        return;
    }
    resetSocket(up);
    rFree(up->boundary);
    up->boundary = 0;
    up->chunked = 0;
    up->closeFrame = 0;
    up->finalized = 0;
    up->gotResponse = 0;
    up->redirect = 0;
    up->rxLen = -1;
    up->rxRemaining = URL_UNLIMITED;
    up->sse = 0;
    up->status = 0;
    up->txLen = -1;
    up->wroteHeaders = 0;
    rFree(up->error);
    up->error = 0;
    rFlushBuf(up->rx);
    rFlushBuf(up->responseBuf);
    rFreeBuf(up->rxHeaders);
    up->rxHeaders = 0;
    rFreeBuf(up->txHeaders);
    up->txHeaders = rAllocBuf(0);
    rFree(up->response);
    up->response = 0;
}

static void resetSocket(Url *up)
{
    if (up->sock) {
        if (up->rxRemaining > 0 || up->close) {
            //  Last response not fully read
            rCloseSocket(up->sock);
        }
        if (rIsSocketClosed(up->sock) || rIsSocketEof(up->sock)) {
            rResetSocket(up->sock);
        }
    }
    if (up->sock == 0) {
        up->sock = rAllocSocket();
    }
}

PUBLIC void urlSetFlags(Url *up, int flags)
{
    if (!up) {
        return;
    }
    up->flags = flags;
}

PUBLIC void urlSetBufLimit(Url *up, size_t limit)
{
    if (!up) {
        return;
    }
    up->bufLimit = limit;
}

PUBLIC void urlClose(Url *up)
{
    if (!up) {
        return;
    }
    if (up->flags & URL_SHOW_REQ_HEADERS && up->sock) {
        rLog("raw", "url", "Disconnect: %s://%s:%d\n", up->sock->tls ? "https" : "http", up->host, up->port);
    }
    rFreeSocket(up->sock);
    up->sock = 0;
}

PUBLIC int urlStart(Url *up, cchar *method, cchar *url)
{
    if (!up || !method || !url) {
        return R_ERR_BAD_ARGS;
    }
    rFree(up->method);
    rFree(up->url);
    up->method = supper(sclone(method));
    up->url = sclone(url);

    resetState(up);
    setDeadline(up);
    if (connectHost(up) < 0) {
        return R_ERR_CANT_CONNECT;
    }
    return up->error ? R_ERR_CANT_CONNECT : 0;
}

/*
    Establish a connection to the host on the required port.
    This will reuse an existing connection if possible.
 */
static int connectHost(Url *up)
{
    char host[256];
    int  port;

    if (!up) {
        return R_ERR_BAD_ARGS;
    }

    if (scopy(host, sizeof(host), up->host ? up->host : "") < 0) {
        host[0] = '\0';
    }
    port = up->port;

    if (urlParse(up, up->url) < 0) {
        urlError(up, "Bad URL");
        return R_ERR_BAD_ARGS;
    }
    //  Validate new host length to prevent buffer overflow in future comparisons
    if (up->host && slen(up->host) >= sizeof(host)) {
        return urlError(up, "Host name too long");
    }

    if (up->sock) {
        if (((smatch(up->scheme, "https") || smatch(up->scheme, "wss")) != rIsSocketSecure(up->sock)) ||
            (*host && !smatch(up->host, host)) || (port && up->port != port) || up->close) {
            rFreeSocket(up->sock);
            up->sock = 0;
        }
    }
    //  An HTTP/1.0 request has no persistent connection: the server closes after the response
    up->close = up->protocol == 0 ? 1 : 0;

    /*
        Remember whether this request goes out on a connection we already had. A server closes an
        idle keep-alive connection whenever it likes, and we cannot tell it has until a read fails.
     */
    up->reused = (up->sock && up->sock->fd != INVALID_SOCKET) ? 1 : 0;

    if (!up->sock) {
        up->sock = rAllocSocket();
    }
    if (smatch(up->scheme, "https") || smatch(up->scheme, "wss")) {
#if ME_COM_SSL
        if (!rIsSocketSecure(up->sock)) {
            rSetTls(up->sock);
        }
        //  Only a fresh socket needs the settings; touching a live TLS session would break it
        if (up->sock->fd == INVALID_SOCKET) {
            applyTls(up);
        }
#else
        return urlError(up, "TLS is not supported in this build");
#endif
    }
    if (up->sock->fd == INVALID_SOCKET) {
        if (up->flags & URL_SHOW_REQ_HEADERS) {
            rLog("raw", "url", "\nConnect: %s://%s:%d\n", up->sock->tls ? "https" : "http", up->host, up->port);
        }
        if (up->flags & URL_NO_LINGER) {
            rSetSocketLinger(up->sock, 0);
        }
        if (rConnectSocket(up->sock, up->host, up->port, up->deadline) < 0) {
            urlError(up, "%s", rGetSocketError(up->sock));
            return R_ERR_CANT_CONNECT;
        }
    }
    return 0;
}

/*
    Convenience function used by most higher level functions.
    If the content length is not specified, and the data is provided, use the length of the data to set
    the content length. If data buffer provided and length is zero, get the string length.
 */
static int fetch(Url *up, cchar *method, cchar *uri, cvoid *data, size_t len, cchar *headers)
{
    int authRetried, staleRetried;

    if (!up || !method || !uri) {
        return R_ERR_BAD_ARGS;
    }
    // LEGACY when len was a ssize, but now is a size_t
    if ((ssize) len < 0) {
        len = 0;
    }
    authRetried = 0;
    staleRetried = 0;

    do {
        if (urlStart(up, method, uri) == 0) {
            // Successful connection
            if (getContentLength(headers, slen(headers)) < 0) {
                // No content length specified, so use the length of the data if provided
                if (data && len == 0) {
                    // Buffer but zero length, so get the string length
                    len = slen(data);
                }
                if (len >= SSIZE_MAX) {
                    urlError(up, "Request body too large");
                    break;
                }
                up->txLen = (ssize) len;
            }
            if (urlWriteHeaders(up, headers) < 0) {
                urlError(up, "Cannot write headers");
                break;
            }
            if (data && len > 0) {
                if (urlWrite(up, data, len) < 0) {
                    urlError(up, "Cannot write body");
                    break;
                }
            }
            if (urlFinalize(up) < 0) {
                /*
                    A reused connection that yielded no response at all was closed by the server
                    while it sat idle. Reconnect and send the request once more. This is only safe
                    because nothing came back: had any of the response arrived, the server has
                    acted on the request and resending could repeat a side effect.
                 */
                if (up->reused && !staleRetried && up->status == 0) {
                    staleRetried = 1;
                    rFree(up->error);
                    up->error = 0;
                    up->close = 1;
                    continue;
                }
                return urlError(up, "Cannot finalize");
            }
#if URL_AUTH
            /*
                Transparent digest challenge handling
             */
            if (up->status == URL_CODE_UNAUTHORIZED && up->username && up->password && !authRetried) {
                if (urlParseAuthChallenge(up)) {
                    // Retry the request with authentication (resetState will be called by urlStart)
                    authRetried = 1;
                    continue;
                }
            }
#endif
            // Complete
            break;
        }
        if (!up->error) {
            urlError(up, "Cannot run \"%s\" %s", method, uri);
        }
    } while (!up->error);

    if (up->error) {
        return R_ERR_CANT_CONNECT;
    }
    return up->status;
}

PUBLIC char *urlGet(cchar *uri, cchar *headersFmt, ...)
{
    va_list args;
    Url     *up;
    cchar   *response;
    char    *headers, *result;

    if (!uri) {
        return NULL;
    }
    va_start(args, headersFmt);
    headers = headersFmt ? sfmtv(headersFmt, args) : 0;
    va_end(args);

    up = urlAlloc(0);
    if (fetch(up, "GET", uri, 0, 0, headers) != URL_CODE_OK) {
        urlFree(up);
        rFree(headers);
        return NULL;
    }
    if ((response = urlGetResponse(up)) == 0) {
        urlFree(up);
        rFree(headers);
        return NULL;
    }
    result = sclone(response);
    urlFree(up);
    rFree(headers);
    return result;
}

PUBLIC Json *urlGetJson(cchar *uri, cchar *headersFmt, ...)
{
    va_list args;
    Json    *json;
    Url     *up;
    char    *headers;

    if (!uri) {
        return NULL;
    }
    va_start(args, headersFmt);
    headers = sfmtv(headersFmt, args);
    va_end(args);

    up = urlAlloc(0);
    //  headers is already formatted - pass as a value, never as a format
    json = urlJson(up, "GET", uri, NULL, 0, "%s", headers);
    urlFree(up);
    rFree(headers);
    return json;
}

PUBLIC char *urlPost(cchar *uri, cvoid *data, size_t len, cchar *headersFmt, ...)
{
    va_list args;
    Url     *up;
    cchar   *response;
    char    *headers, *result;

    if (!uri) {
        return NULL;
    }
    va_start(args, headersFmt);
    headers = headersFmt ? sfmtv(headersFmt, args) : 0;
    va_end(args);

    up = urlAlloc(0);
    if (fetch(up, "POST", uri, data, len, headers) != URL_CODE_OK) {
        urlFree(up);
        rFree(headers);
        return NULL;
    }
    if ((response = urlGetResponse(up)) == 0) {
        urlFree(up);
        rFree(headers);
        return NULL;
    }
    result = sclone(response);
    urlFree(up);
    rFree(headers);
    return result;
}

PUBLIC Json *urlPostJson(cchar *uri, cvoid *data, size_t len, cchar *headersFmt, ...)
{
    va_list args;
    Json    *json;
    Url     *up;
    char    *headers;

    if (!uri) {
        return NULL;
    }
    va_start(args, headersFmt);
    headers = headersFmt ? sfmtv(headersFmt, args) : sclone("Content-Type: application/json\r\n");
    va_end(args);

    up = urlAlloc(0);
    //  headers is already formatted - pass as a value, never as a format
    json = urlJson(up, "POST", uri, data, len, "%s", headers);
    urlFree(up);
    rFree(headers);
    return json;
}

PUBLIC int urlFetch(Url *up, cchar *method, cchar *uri, cvoid *data, size_t len, cchar *headersFmt, ...)
{
    va_list args;
    Url     *tmpUp;
    char    *headers;
    int     status;

    tmpUp = 0;
    if (!up) {
        up = tmpUp = urlAlloc(0);
    }
    va_start(args, headersFmt);
    headers = headersFmt ? sfmtv(headersFmt, args) : 0;
    va_end(args);

    status = fetch(up, method, uri, data, len, headers);

    rFree(headers);
    if (tmpUp) {
        urlFree(tmpUp);
    }
    return status;
}

PUBLIC Json *urlJson(Url *up, cchar *method, cchar *uri, cvoid *data, size_t len, cchar *headersFmt, ...)
{
    va_list args;
    Url     *tmpUp;
    Json    *json;
    char    *errorMsg, *headers;

    tmpUp = 0;
    if (!up) {
        up = tmpUp = urlAlloc(0);
    }
    va_start(args, headersFmt);
    headers = headersFmt ? sfmtv(headersFmt, args) : sclone("Content-Type: application/json\r\n");
    va_end(args);

    if (fetch(up, method, uri, data, len, headers) == URL_CODE_OK) {
        if ((json = jsonParseString(urlGetResponse(up), &errorMsg, 0)) == 0) {
            urlError(up, "Cannot parse json. \"%s\"", errorMsg);
        }
        rFree(errorMsg);
    } else {
        if (up->error) {
            rTrace("url", "Cannot fetch %s. Error: %s", uri, urlGetError(up));
        } else {
            rTrace("url", "Cannot fetch %s. Bad status %d", uri, up->status);
        }
        rTrace("url", "%s", urlGetResponse(up));
        json = 0;
    }
    rFree(headers);
    if (tmpUp) {
        urlFree(tmpUp);
    }
    return json;
}

/* Positive timeout bounds the whole request; zero uses runtime socket-wait limits; negative disables deadlines. */
static void setDeadline(Url *up)
{
    if (!up) {
        return;
    }
    if (up->timeout > 0) {
        up->deadline = rGetTicks() + up->timeout;
    } else if (up->timeout < 0) {
        up->deadline = MAXINT64;
    } else {
        up->deadline = 0;
    }
}

static bool isExpired(Url *up)
{
    return up->deadline > 0 && rGetTicks() > up->deadline;
}

/*
    Finalize the normal request body and reads the response line and response headers.
    For WebSockets, this will also verify the WebSocket handshake and should be called before
    normal WebSocket processing commences.
 */
PUBLIC int urlFinalize(Url *up)
{
    if (!up) {
        return R_ERR_BAD_ARGS;
    }
    if (up->finalized) {
        return 0;
    }
    up->finalized = 1;
    if (urlWrite(up, 0, 0) < 0) {
        return urlError(up, "Cannot finalize request");
    }
#if ME_COM_WEBSOCK
    if (up->webSocket && verifyWebSocket(up) < 0) {
        return urlError(up, "Cannot verify WebSockets connection");
    }
#endif
    return up->error ? R_ERR_CANT_WRITE: 0;
}

/*

    Write body data. Set bufsize to zero to signify end of body if the content length is not defined.
    If finalizing, read the response headers after the last write packet.
    This routine will handle chunking transparently and will trace request data if required.
    May close the socket if required.
 */
PUBLIC ssize urlWrite(Url *up, cvoid *buf, size_t bufsize)
{
    if (!up) {
        return R_ERR_BAD_ARGS;
    }
    if (up->finalized) {
        if (buf && bufsize > 0) {
            return urlError(up, "Cannot write after finalize");
        }
    }
    // LEGACY
    if ((ssize) bufsize == -1) {
        bufsize = 0;
    }
    if (buf == NULL) {
        bufsize = 0;
    } else if (bufsize == 0) {
        bufsize = slen(buf);
    }
    if (!up->wroteHeaders) {
        /*
            Finalizing before anything was written means the body is empty. Declare it so rather
            than chunk framing nothing, which some servers reject on a HEAD.
         */
        if (buf == NULL && up->txLen < 0) {
            up->txLen = 0;
        }
        if (urlWriteHeaders(up, NULL) < 0) {
            //  Already closed
            return R_ERR_CANT_WRITE;
        }
    }
    if (writeChunkDivider(up, bufsize) < 0) {
        //  Already closed
        return R_ERR_CANT_WRITE;
    }
    if (bufsize > 0) {
        if (up->wroteHeaders && up->flags & URL_SHOW_REQ_BODY) {
            rLog("raw", "url", "%.*s\n\n", (int) bufsize, (char*) buf);
        }
        if (rWriteSocket(up->sock, buf, bufsize, up->deadline) != (ssize) bufsize) {
            return urlError(up, "Cannot write to socket");
        }
    }
    /*
        If all data written, finalize and read the response headers.
     */
    if (bufsize == 0 || (ssize) bufsize == up->txLen) {
        if (!up->rxHeaders && readHeaders(up) < 0) {
            //  Already closed
            return R_ERR_CANT_READ;
        }
    }
    //  Close the socket if required once we have read all the required response data
    if (up->close && up->rxRemaining == 0) {
        rCloseSocket(up->sock);
    }
    return (ssize) bufsize;
}

PUBLIC ssize urlWriteFmt(Url *up, cchar *fmt, ...)
{
    va_list ap;
    char    *buf;
    ssize   r;

    if (!up || fmt == NULL) {
        return R_ERR_BAD_ARGS;
    }
    va_start(ap, fmt);
    buf = sfmtv(fmt, ap);
    va_end(ap);
    r = urlWrite(up, buf, slen(buf));
    rFree(buf);
    return r;
}

PUBLIC ssize urlWriteFile(Url *up, cchar *path)
{
    char  buf[ME_BUFSIZE];
    ssize nbytes, total;
    int   fd;

    if (!up || path == NULL) {
        return R_ERR_BAD_ARGS;
    }
    if ((fd = open(path, O_RDONLY, 0)) < 0) {
        return urlError(up, "Cannot open %s", path);
    }
    total = 0;
    while ((nbytes = read(fd, buf, sizeof(buf))) > 0) {
        if (urlWrite(up, buf, (size_t) nbytes) < 0) {
            close(fd);
            return urlError(up, "Cannot write to socket");
        }
        total += nbytes;
    }
    close(fd);
    if (nbytes < 0) {
        return urlError(up, "Cannot read from %s", path);
    }
    return total;
}

/*
    Write a chunked transfer encoding header for a given length.
    If len is zero, write the chunked trailer.
 */
static int writeChunkDivider(Url *up, size_t len)
{
    char chunk[24];

    if (!up) {
        return R_ERR_BAD_ARGS;
    }
    if (up->txLen >= 0) {
        //  Content-Length is known, so the body needs no chunk framing
        return 0;
    }
    /*
        If chunking, we don't write the \r\n after the headers. This permits us to write
        the \r\n after the prior item (header or body), the length and the chunk trailer in one write.
     */
    if (len == 0) {
        scopy(chunk, sizeof(chunk), "\r\n0\r\n\r\n");
    } else {
        sfmtbuf(chunk, sizeof(chunk), "\r\n%zx\r\n", len);
    }
    if (rWriteSocket(up->sock, chunk, slen(chunk), up->deadline) < 0) {
        return urlError(up, "Cannot write to socket");
    }
    return 0;
}

/*
    Read the response headers.
 */
static int readHeaders(Url *up)
{
    ssize size;
    uint  close;
    int   interim;

    /*
        RFC 7230 3.3: a 1xx is interim. Discard it and keep reading until the final response
        arrives. 101 is excluded -- it is the final response to a WebSocket upgrade. A server
        that never stops sending interim responses is a CPU sink, so their number is bounded.
     */
    interim = 0;
    close = up->close;
    do {
        if (interim++ > URL_MAX_INTERIM) {
            return urlError(up, "Too many interim responses");
        }
        // Discard interim-response metadata before parsing the final response.
        up->chunked = 0;
        up->close = close;
        up->closeFrame = 0;
        up->redirect = 0;
        up->rxLen = -1;
        up->rxRemaining = URL_UNLIMITED;
        rFreeBuf(up->rxHeaders);
        up->rxHeaders = 0;
        if ((size = readUntil(up, "\r\n\r\n", URL_MAX_HEADERS)) < 0) {
            urlError(up, "Response headers too big");
            return R_ERR_CANT_READ;
        }
        if (parseResponse(up, (size_t) size) < 0) {
            return R_ERR_CANT_READ;
        }
    } while (URL_CODE_CONTINUE <= up->status && up->status < URL_CODE_OK &&
             up->status != URL_CODE_SWITCHING);
    return up->status;
}

static int parseResponse(Url *up, size_t headerSize)
{
    RBuf   *buf;
    char   *end, *tok;
    size_t len;
    int    status;

    buf = up->rx;
    if (headerSize <= 10) {
        return urlError(up, "Bad response header");
    }
    end = buf->start + headerSize;
    end[-2] = '\0';

    if (up->flags & URL_SHOW_RESP_HEADERS) {
        rLog("raw", "url", "%s\n", rBufToString(buf));
    }
    tok = buf->start;
    if ((sncmp(tok, "HTTP/1.0", 8) != 0 && sncmp(tok, "HTTP/1.1", 8) != 0) || tok[8] != ' ') {
        return urlError(up, "Bad response protocol");
    }
    tok += 9;
    if (tok[0] < '0' || tok[0] > '9' || tok[1] < '0' || tok[1] > '9' || tok[2] < '0' || tok[2] > '9' ||
        (tok[3] != ' ' && tok[3] != '\r' && tok[3] != '\n')) {
        return urlError(up, "Bad response status");
    }
    status = (tok[0] - '0') * 100 + (tok[1] - '0') * 10 + (tok[2] - '0');
    if (status < 100 || status > 599) {
        return urlError(up, "Bad response status");
    }
    up->status = status;

    //  An HTTP/1.0 server closes after the response, whether or not it says so
    if (sncmp(buf->start, "HTTP/1.0", 8) == 0) {
        up->close = 1;
    }
    rAdjustBufStart(buf, end - buf->start);

    if ((tok = strchr(tok, '\n')) != 0) {
        len = (size_t) (end - 2 - ++tok);
        if (len < 0) {
            urlError(up, "Bad headers");
            return R_ERR_BAD_STATE;
        }
        assert(!up->rxHeaders);
        up->rxHeaders = rAllocBuf(len + 1);
        rPutBlockToBuf(up->rxHeaders, tok, len);

        if (!parseHeaders(up)) {
            return R_ERR_BAD_STATE;
        }
    } else {
        return R_ERR_BAD_STATE;
    }
    return 0;
}

/*
    Read data into the supplied buffer up to bufsize and return the number of bytes read.
    Will return negative error code if there is an error or the socket is closed.
    After reading the headers, the headers
    are copied to the rxHeaders. User body data is read through the rx buffer into the
    user supplied buffer. rxRemaining is the amount of remaining data for this request that must
    be read into the low level rx buffer. If chunked, rxRemaining may be set to unlimited before
    reading a chunk and the chunk length.
 */
PUBLIC ssize urlRead(Url *up, char *buf, size_t bufsize)
{
    ssize nbytes;

    if (!up || buf == NULL || bufsize < 0) {
        return R_ERR_BAD_ARGS;
    }
    if (up->gotResponse) {
        return urlError(up, "Should not call urlRead after urlGetResponse");
    }
    if (urlFinalize(up) < 0) {
        urlError(up, "Cannot finalize request");
        return R_ERR_CANT_READ;
    }
    if (bufsize == 0) {
        return 0;
    }
    if (up->rxRemaining == 0 && rGetBufLength(up->rx) == 0) {
        return 0;
    }
    //  This may read from the rx buffer or may read from the socket
    if (up->chunked) {
        nbytes = readChunk(up, buf, bufsize);
    } else {
        nbytes = readBlock(up, buf, bufsize);
    }
    if (nbytes < 0) {
        if (up->rxRemaining) {
            return urlError(up, "Cannot read from socket");
        }
        up->close = 1;
        return 0;
    }
    return nbytes;
}

/*
    Read a chunked transfer segment and return the number of user bytes read.
    Will return negative error code if there is an error or the socket is closed.
 */
static ssize readChunk(Url *up, char *buf, size_t bufsize)
{
    RBuf   *bp;
    int64  chunkSize;
    ssize  lineLen, nbytes;
    size_t trailers;
    char   *end, *line;

    bp = up->rx;
    nbytes = 0;

    if (up->chunked == URL_CHUNK_START) {
        if ((lineLen = readUntil(up, "\r\n", URL_CHUNK_LINE)) < 0) {
            return urlError(up, "Bad chunk specification");
        }
        /*
            RFC 7230 4.1: chunk-size is 1*HEXDIG, optionally followed by extensions after a ";"
            that a recipient must ignore. A sign, leading space or radix prefix is not a size, and
            a peer that parses strictly would frame the body differently from one that accepts it.
         */
        line = bp->start;
        line[lineLen - 2] = '\0';
        //  A NUL would end the parse early and hide the rest of the line
        if (slen(line) != (size_t) lineLen - 2 || sparseUint(line, 16, SSIZE_MAX, &end, &chunkSize) < 0) {
            return urlError(up, "Bad chunk specification");
        }
        //  RFC 9112 7.1.1: white space after the size is permitted only before an extension
        if (*end != '\0') {
            while (isWhite(*end)) end++;
            if (*end != ';') {
                return urlError(up, "Bad chunk specification");
            }
        }
        rAdjustBufStart(bp, lineLen);

        if (chunkSize) {
            up->rxRemaining = (size_t) chunkSize;
            up->chunked = URL_CHUNK_DATA;

        } else {
            /*
                RFC 7230 4.1.2: the last chunk may be followed by trailer fields, ended by an
                empty line. Nothing here needs them, but they must be consumed or they are read
                as the start of the next response on this connection.
             */
            trailers = 0;
            do {
                if ((lineLen = readUntil(up, "\r\n", URL_MAX_HEADERS)) < 0) {
                    return urlError(up, "Bad chunk trailer");
                }
                //  The trailer section is bounded as a whole, like the header block
                if ((trailers += (size_t) lineLen) > URL_MAX_HEADERS) {
                    return urlError(up, "Response trailers too big");
                }
                rAdjustBufStart(bp, lineLen);
            } while (lineLen > 2);
            up->chunked = URL_CHUNK_EOF;
            up->rxRemaining = 0;
        }
    }
    if (up->chunked == URL_CHUNK_DATA) {
        if ((nbytes = readBlock(up, buf, min(bufsize, up->rxRemaining))) <= 0) {
            return urlError(up, "Cannot read chunk data");
        }
        up->rxRemaining -= (size_t) nbytes;
        if (up->rxRemaining == 0) {
            //  Move onto the next chunk. Set rxRemaining high until we know the chunk length.
            up->chunked = URL_CHUNK_START;
            up->rxRemaining = URL_UNLIMITED;
            //  Chunk data is followed by a bare CRLF and nothing else
            if (readUntil(up, "\r\n", 2) != 2) {
                return urlError(up, "Bad chunk data");
            }
            rAdjustBufStart(bp, 2);
        }
    }
    return nbytes;
}

/*
    Low-level read data from the socket into the web->rx buffer.
    Read up to bufsize bytes.
    Return the number of bytes read or a negative error code.
 */
static ssize readSocket(Url *up, size_t bufsize)
{
    RBuf   *bp;
    size_t space, toRead;
    ssize  nbytes;

    bp = up->rx;
    if (isExpired(up)) {
        return urlError(up, "Request timed out");
    }
    rCompactBuf(bp);
    space = min(bufsize <= ME_BUFSIZE ? ME_BUFSIZE : URL_BUF_BOOST_2X, up->rxRemaining);
    rReserveBufSpace(bp, space);
    toRead = min(rGetBufSpace(bp), up->rxRemaining);
    if ((nbytes = rReadSocket(up->sock, bp->end, toRead, up->deadline)) < 0) {
        if (up->closeFrame && rIsSocketEof(up->sock)) {
            //  The close is the end of the body, not a truncated read
            up->rxRemaining = 0;
            return (ssize) rGetBufLength(bp);
        }
        return urlError(up, "Cannot read from socket");
    }
    rAdjustBufEnd(bp, nbytes);
    if (!up->chunked && up->rxRemaining > 0) {
        up->rxRemaining -= (size_t) nbytes;
    }
    return (ssize) rGetBufLength(bp);
}

/*
    Read a block of data into the supplied buffer up to bufsize.
    This reads data through the rx buffer into the user supplied buffer.
 */
static ssize readBlock(Url *up, char *buf, size_t bufsize)
{
    RBuf   *bp;
    size_t nbytes;

    bp = up->rx;

    if (rGetBufLength(bp) == 0 && readSocket(up, bufsize) < 0) {
        return R_ERR_CANT_READ;
    }
    nbytes = min(rGetBufLength(bp), bufsize);
    if (buf && nbytes > 0) {
        memcpy(buf, bp->start, nbytes);
        rAdjustBufStart(bp, (ssize) nbytes);
    }
    return (ssize) nbytes;
}

/*
    Read response data into the rx buffer until the designated pattern is present, and return the
    length of the data up to and including the pattern. Nothing is consumed: the caller parses in
    place and advances the buffer. May overread, and what follows the pattern stays buffered for
    the next read. Fails without setting an error if the pattern is not found within limit bytes,
    so a server can never grow the buffer without bound.
 */
static ssize readUntil(Url *up, cchar *until, size_t limit)
{
    RBuf   *bp;
    char   *end;
    size_t scanned, toRead, ulen;
    ssize  nbytes;

    bp = up->rx;
    rAddNullToBuf(bp);
    ulen = slen(until);
    scanned = 0;

    /*
        Search only what a previous pass has not seen, less the length of a pattern that may
        straddle the boundary, so a server dribbling bytes cannot make each read rescan the whole
        buffer. The deadline is checked here as well because a read that always finds a byte
        waiting never waits, and the wait is where the socket enforces it.
     */
    while ((end = strstr(&bp->start[scanned], until)) == 0) {
        scanned = rGetBufLength(bp) > ulen ? rGetBufLength(bp) - ulen + 1 : 0;
        if (rGetBufLength(bp) >= limit) {
            return R_ERR_WONT_FIT;
        }
        if (isExpired(up)) {
            return urlError(up, "Request timed out");
        }
        rCompactBuf(bp);
        rReserveBufSpace(bp, ME_BUFSIZE);
        toRead = min(rGetBufSpace(bp), up->rxRemaining);
        if ((nbytes = rReadSocket(up->sock, bp->end, toRead, up->deadline)) < 0) {
            if (!up->rxHeaders || up->rxRemaining) {
                return urlError(up, "Cannot read response from site");
            }
            return R_ERR_CANT_READ;
        }
        rAdjustBufEnd(bp, nbytes);
        rAddNullToBuf(bp);
        if (!up->chunked && up->rxRemaining > 0) {
            up->rxRemaining -= (size_t) nbytes;
        }
    }
    nbytes = (ssize) ((size_t) (end - bp->start) + slen(until));
    if ((size_t) nbytes > limit) {
        return R_ERR_WONT_FIT;
    }
    return nbytes;
}

/*
    Push the settings this object owns onto the socket about to connect. The setters only record
    them: the socket may be replaced any time before the connection is made, and a socket that is
    already connected must not have its TLS state rewritten.
 */
#if ME_COM_SSL
static void applyTls(Url *up)
{
    if (!up->sock || !rIsSocketSecure(up->sock)) {
        return;
    }
    if (up->caFile || up->keyFile || up->certFile || up->revokeFile) {
        rSetSocketCerts(up->sock, up->caFile, up->keyFile, up->certFile, up->revokeFile);
    }
    if (up->ciphers) {
        rSetSocketCiphers(up->sock, up->ciphers);
    }
    if (up->verifyPeer >= 0 || up->verifyIssuer >= 0) {
        rSetSocketVerify(up->sock, up->verifyPeer, up->verifyIssuer);
    }
}
#endif

static void setTlsFile(char **field, cchar *value)
{
    if (value) {
        rFree(*field);
        *field = sclone(value);
    }
}

PUBLIC void urlSetCerts(Url *up, cchar *ca, cchar *key, cchar *cert, cchar *revoke)
{
    if (!up) {
        return;
    }
    setTlsFile(&up->caFile, ca);
    setTlsFile(&up->keyFile, key);
    setTlsFile(&up->certFile, cert);
    setTlsFile(&up->revokeFile, revoke);
}

PUBLIC void urlSetCiphers(Url *up, cchar *ciphers)
{
    if (!up) {
        return;
    }
    setTlsFile(&up->ciphers, ciphers);
}

PUBLIC void urlSetVerify(Url *up, int verifyPeer, int verifyIssuer)
{
    if (!up) {
        return;
    }
    up->verifyPeer = verifyPeer;
    up->verifyIssuer = verifyIssuer;
}

/*
    Parse the URL and set the up->host, up->port, up->path, up->hash, and up->query fields.
    Return 0 on success or a negative error code.
    The uri is already trimmed of whitespace.
 */
PUBLIC int urlParse(Url *up, cchar *uri)
{
    char  *next, *tok;
    bool  hasScheme;
    char  *end;
    int64 port;

    if (!up || uri == NULL) {
        return R_ERR_BAD_ARGS;
    }
    rFree(up->urlbuf);

    up->urlbuf = sclone(uri);
    up->scheme = "http";
    up->host = "localhost";
    up->port = 80;
    up->path = "";
    up->hash = 0;
    up->query = 0;
    hasScheme = 0;

    tok = up->urlbuf;

    //  hash comes after the query
    if ((next = schr(tok, '#')) != 0) {
        *next++ = '\0';
        up->hash = next;
    }
    if ((next = schr(tok, '?')) != 0) {
        *next++ = '\0';
        up->query = next;
    }
    if ((next = scontains(tok, "://")) != 0) {
        hasScheme = 1;
        if (next > tok) {
            up->scheme = tok;
        }
        *next = 0;
        if (smatch(up->scheme, "https") || smatch(up->scheme, "wss")) {
            up->port = 443;
        }
        tok = &next[3];
    }

    if (*tok == '[' && ((next = strchr(tok, ']')) != 0)) {
        // IPv6  [::]:port/url
        up->host = &tok[1];
        *next++ = 0;
        tok = next;

    } else if (*tok && *tok != '/' && *tok != ':' && (hasScheme || strchr(tok, ':'))) {
        // hostname:port/path
        up->host = tok;
        tok = spbrk(tok, ":/");

    } else if (hasScheme) {
        urlError(up, "Missing host name");
        return R_ERR_BAD_STATE;
    }
    // A port follows the host, or begins a relative ":port/path". Never after the "/" that starts the path.
    if (tok && *tok == ':') {
        *tok++ = 0;
        if (sparseUint(tok, 10, 65535, &end, &port) < 0 || port < 1 || (*end != '\0' && *end != '/')) {
            urlError(up, "Invalid port number");
            return R_ERR_BAD_STATE;
        }
        up->port = (int) port;
        tok = schr(tok, '/');
    }
    /*
        urlWriteHeaders writes "/" before up->path, so consume the separator that begins the path.
        A second one is part of the path.
     */
    if (tok && *tok == '/') {
        *tok++ = 0;
    }
    if (tok && *tok) {
        up->path = tok;
    }
    if (slen(up->host) > 255) {
        urlError(up, "Invalid host name");
        return R_ERR_BAD_STATE;
    }
    /*
        The host, path and query go onto the wire as they are. A control character or space in
        any of them ends the request line early, and CR LF lets a URL supplied by a remote party,
        such as a redirect or an upload location, inject headers or a whole second request.
     */
    if (!isWireSafe(up->host) || !isWireSafe(up->path) || !isWireSafe(up->query)) {
        urlError(up, "Invalid character in URL");
        return R_ERR_BAD_STATE;
    }
    return 0;
}

/*
    True if the string can be written into a request line: no control characters, no space
 */
static bool isWireSafe(cchar *s)
{
    for (; s && *s; s++) {
        if ((uchar) * s <= ' ' || *s == 0x7f) {
            return 0;
        }
    }
    return 1;
}

/*
    Return the entire URL response which is stored in the up->responseBuf held by the URL object.
    No need for the caller to free. User should not call urlRead if using urlGetResponse.
 */
PUBLIC RBuf *urlGetResponseBuf(Url *up)
{
    RBuf   *buf;
    ssize  clen, nbytes;
    size_t len;

    if (!up) {
        return NULL;
    }
    if (urlFinalize(up) < 0) {
        urlError(up, "Cannot finalize request");
        return NULL;
    }
    buf = up->responseBuf;
    if (!buf) {
        buf = up->responseBuf = rAllocBuf(ME_BUFSIZE);
    }
    //  The declared length was validated when the headers were parsed
    clen = up->rxLen;
    if (clen >= 0 && up->bufLimit > 0 && (size_t) clen >= up->bufLimit) {
        urlError(up, "Invalid Content-Length");
        return NULL;
    }
    if (!up->gotResponse && clen != 0) {
        do {
            len = clen >= 0 ? (size_t) clen : min((buf->buflen * 2), URL_BUF_BOOST_1024X);
            if (up->bufLimit > 0) {
                len = min(len, (up->bufLimit - rGetBufLength(buf)));
                if (len <= 0) {
                    urlError(up, "Response too big");
                    break;
                }
            }
            /* A failed reservation at ME_R_MAX_BUF must not increase the readable buffer size. */
            if (rReserveBufSpace(buf, len) < 0) {
                urlError(up, "Response too big");
                break;
            }
            len = min(len, rGetBufSpace(buf));
            if ((nbytes = urlRead(up, rGetBufEnd(buf), len)) < 0) {
                if (up->rxRemaining) {
                    urlError(up, "Cannot read response");
                    return NULL;
                }
            }
            rAdjustBufEnd(buf, nbytes);
            if (clen >= 0) {
                clen -= nbytes;
                if (clen <= 0) {
                    break;
                }
            }
        } while (nbytes > 0);
        up->gotResponse = 1;
    }
    if (up->flags & URL_SHOW_RESP_BODY && isprintable(rGetBufStart(buf), rGetBufLength(buf))) {
        rLog("raw", "url", "Response Body >>>>\n\n%.*s", (int) rGetBufLength(buf), (char*) buf->start);
    }
    return buf;
}

PUBLIC cchar *urlGetResponse(Url *up)
{
    RBuf *buf;

    if (!up) {
        return NULL;
    }
    if (!up->response) {
        if ((buf = urlGetResponseBuf(up)) == 0) {
            return "";
        }
        up->response = snclone(rBufToString(buf), rGetBufLength(buf));
    }
    return up->response;
}

PUBLIC Json *urlGetJsonResponse(Url *up)
{
    Json *json;
    char *errorMsg;

    if (!up) {
        return NULL;
    }
    if ((json = jsonParseString(urlGetResponse(up), &errorMsg, 0)) == 0) {
        urlError(up, "Cannot parse json. %s", errorMsg);
        rFree(errorMsg);
        return 0;
    }
    return json;
}

PUBLIC int urlGetStatus(Url *up)
{
    if (!up) {
        return R_ERR_BAD_ARGS;
    }
    if (urlFinalize(up) < 0) {
        up->status = urlError(up, "Cannot finalize request");
    }
    return up->status;
}

/*
    Headers have been tokenized with a null replacing the ":" and "\r\n"
 */
PUBLIC cchar *urlGetHeader(Url *up, cchar *header)
{
    cchar *cp, *end, *start, *value;

    if (!up) {
        return NULL;
    }
    if (!up->rxHeaders) {
        return 0;
    }
    start = rGetBufStart(up->rxHeaders);
    end = up->rxHeaders->end;
    value = 0;

    for (cp = start; cp < end; cp++) {
        if (scaselessmatch(cp, header)) {
            cp += slen(cp) + 1;
            if (cp > end) {
                //  A name that is the last token has no value
                break;
            }
            while (*cp == ' ') cp++;
            value = cp;
            break;
        }
        //  Step over header
        cp += slen(cp) + 1;
        if (cp < end) {
            //  Step over value, which may be empty
            cp += slen(cp) + 1;
        }
    }
    return value;
}

PUBLIC char *urlGetCookie(Url *up, cchar *name)
{
    cchar  *cp, *end, *ep, *nend, *start, *vp;
    char   *value;
    size_t nameLen;

    if (!up || !name || *name == '\0') {
        return NULL;
    }
    if (!up->rxHeaders) {
        return 0;
    }
    nameLen = slen(name);
    start = rGetBufStart(up->rxHeaders);
    end = up->rxHeaders->end;
    value = 0;

    /*
        Scan with vp so cp remains at the header start for the next step.
     */
    for (cp = start; cp < end; cp++) {
        if (scaselessmatch(cp, "Set-Cookie")) {
            vp = cp + slen(cp) + 1;
            if (vp > end) {
                //  A name that is the last token has no value
                break;
            }
            while (isWhite(*vp)) vp++;
            /*
                RFC 6265 5.2: the name runs to the first "=", trimmed of white space, and a
                Set-Cookie with no "=" is ignored. The whole name must match -- comparing only
                the cookie's own length would let "sid" answer a request for "sidx".
             */
            for (ep = vp; *ep && *ep != '='; ep++) {
            }
            if (*ep == '=') {
                for (nend = ep; nend > vp && isWhite(nend[-1]); nend--) {
                }
                if ((size_t) (nend - vp) == nameLen && sncaselesscmp(vp, name, nameLen) == 0) {
                    for (vp = ep + 1; *vp && isWhite(*vp); vp++) {
                    }
                    //  The value ends at the first ";" and may be empty
                    for (ep = vp; *ep && *ep != ';'; ep++) {
                    }
                    while (ep > vp && isWhite(ep[-1])) ep--;
                    value = snclone(vp, (size_t) (ep - vp));
                    break;
                }
            }
        }
        //  Step over header
        cp += slen(cp) + 1;
        if (cp < end) {
            //  Step over value, which may be empty
            cp += slen(cp) + 1;
        }
    }
    return value;
}

static ssize getContentLength(cchar *headers, size_t size)
{
    cchar  *cp, *end, *header;
    size_t len;

    if (!headers) {
        return -1;
    }
    if (size < 0) {
        return -1;
    }
    end = &headers[size];
    header = "content-length";
    len = slen(header);

    for (cp = headers; cp < end; cp++) {
        if (sncaselesscmp(cp, header, len) == 0) {
            cp += len + 1;
            while (*cp == ' ') cp++;
            return stoi(cp);
        }
        //  Step over header
        cp += len + 1;
        if (cp < end && *cp) {
            //  Step over value
            if ((cp = schr(cp, '\n')) == 0) {
                break;
            }
        }
    }
    return -1;
}

/*
    Get the next header from header line and return the token key and value
    Set flags to HDR_HTTP for HTTP headers or HDR_SSE for SSE headers.
 */
static char *getHeader(char *line, char **key, char **value, int flags)
{
    char *cp, *tok;

    if (!key || !value) {
        return 0;
    }
    if (flags & HDR_HTTP) {
        /* Reject obsolete header folding to preserve response framing (RFC 9112 5.2). */
        if (isWhite(*line)) {
            return 0;
        }
    } else {
        while (*line && isWhite(*line)) line++;
    }
    if ((cp = strchr(line, ':')) == 0) {
        return 0;
    }
    *key = line;
    *cp++ = '\0';

    //  Validate header key
    for (tok = *key; *tok; tok++) {
        if ((uchar) * tok >= (uchar) sizeof(validHeaderChars) || !validHeaderChars[*tok & 0x7f]) {
            return 0;
        }
    }
    if ((*key)[0] == '\0' && !(flags & HDR_SSE)) {
        return 0;
    }

    if (isWhite(*cp)) {
        cp++;
    }
    if (flags & HDR_SSE) {
        while (*cp && isWhite(*cp)) cp++;
    }
    *value = cp;
    while (*cp && (*cp != '\r' && *cp != '\n')) cp++;

    if (!(flags & HDR_SSE)) {
        if (*cp != '\r') {
            return 0;
        }
        *cp++ = '\0';
    }
    if (*cp != '\n') {
        return 0;
    }
    *cp++ = '\0';

    if (flags & HDR_HTTP && (*value)[0]) {
        // Trim white space from end of value
        tok = &(*value)[slen(*value) - 1];
        while (tok >= *value && isWhite(*tok)) {
            *tok-- = '\0';
        }
    }
    return cp;
}

/*
    Parse the headers in-situ. The headers string is modified by tokenizing with '\0'.
 */
static bool parseHeaders(Url *up)
{
    int64 len;
    char  c, *end, *key, *tok, *value;

    for (tok = rGetBufStart(up->rxHeaders); tok < rGetBufEnd(up->rxHeaders); ) {

        if ((tok = getHeader(tok, &key, &value, HDR_HTTP)) == 0) {
            urlError(up, "Bad header");
            return 0;
        }
        c = tolower(*key);
        if (c == 'c') {
            if (scaselessmatch(key, "content-length")) {
                /*
                    RFC 7230 3.3.2: Content-Length is 1*DIGIT. A lenient parse frames the message
                    differently from a peer that parses it correctly, and reads garbage as zero,
                    which silently drops the body.
                 */
                if (sparseUint(value, 10, SSIZE_MAX - 1, &end, &len) < 0 || *end != '\0') {
                    urlError(up, "Invalid Content-Length");
                    return 0;
                }
                /*
                    RFC 7230 3.3.3: a message with Content-Length headers that disagree cannot be
                    framed and must be rejected. Accepting it lets an upstream proxy and this client
                    disagree about where the response ends.
                 */
                if (up->rxLen >= 0 && up->rxLen != (ssize) len) {
                    urlError(up, "Conflicting Content-Length");
                    return 0;
                }
                up->rxLen = (ssize) len;
                up->rxRemaining = smatch(up->method, "HEAD") ? 0 : (size_t) up->rxLen;

            } else if (scaselessmatch(key, "connection")) {
                if (scaselessmatch(value, "close")) {
                    up->close = 1;
                }
            }
        } else if (c == 'l') {
            if (scaselessmatch(key, "location")) {
                up->redirect = value;

#if URL_SSE
            } else if (scaselessmatch(key, "last-event-id")) {
                up->lastEventId = stoi(value);
#endif
            }
        } else if (c == 't' && scaselessmatch(key, "transfer-encoding")) {
            /*
                RFC 7230 3.3.3: only "chunked" as the final encoding delimits the message. Any
                other encoding cannot be decoded here, and treating it as chunked would frame the
                body differently from the server.
             */
            if ((end = srchr(value, ',')) != 0) {
                value = end + 1;
            }
            if (!scaselessmatch(strim(value, " \t", R_TRIM_BOTH), "chunked")) {
                urlError(up, "Unsupported Transfer-Encoding");
                return 0;
            }
            up->chunked = URL_CHUNK_START;
        }
    }
    if (up->status == URL_CODE_NO_CONTENT || up->status == URL_CODE_NOT_MODIFIED ||
        smatch(up->method, "HEAD")) {
        /*
            RFC 7230 3.3: these never have a body, whatever Content-Length claims. Anything still
            buffered is not ours -- on a connection we would otherwise reuse it is the start of the
            next response -- so discard it and do not reuse the connection.
         */
        up->rxRemaining = 0;
        if (rGetBufLength(up->rx) > 0) {
            rFlushBuf(up->rx);
            up->close = 1;
        }
    } else if (up->chunked) {
        /*
            Transfer-Encoding overrides Content-Length. Disable connection reuse for this ambiguous pair (RFC 9112
            6.3).
         */
        if (up->rxLen >= 0) {
            up->rxLen = -1;
            up->close = 1;
        }
        up->rxRemaining = URL_UNLIMITED;
    } else if (up->rxLen < 0) {
        /*
            RFC 7230 3.3.3 case 7: with neither Content-Length nor a transfer coding the body runs
            to the connection close, which is how an HTTP/1.0 server frames a response of unknown
            length. The socket EOF is the end of the body here, not a truncated read.
         */
        up->closeFrame = 1;
        up->close = 1;
        up->rxRemaining = URL_UNLIMITED;
    } else {
        up->rxRemaining -= min(rGetBufLength(up->rx), up->rxRemaining);
    }
    return 1;
}

PUBLIC void urlSetTimeout(Url *up, Ticks timeout)
{
    if (!up) {
        return;
    }
    up->timeout = timeout;
    setDeadline(up);
}

PUBLIC void urlSetDefaultTimeout(Ticks value)
{
    timeout = value;
}

/*
    Write HTTP request headers. Can be called multiple times.
 */
PUBLIC int urlWriteHeaders(Url *up, cchar *headers)
{
    RBuf  *buf;
    cchar *hostFmt, *protocol, *query, *qsep, *startHeaders;

    if (!up) {
        return R_ERR_BAD_ARGS;
    }
    if (up->wroteHeaders) {
        return 0;
    }

    protocol = up->protocol ? "HTTP/1.1" : "HTTP/1.0";
    query = up->query ? up->query : "";
    qsep = up->query ? "?" : "";

    /*
        The request-target is the path and query only. RFC 3986 3.5: a fragment is never sent to
        the server. It stays parsed in up->hash for the caller.
     */
    buf = up->txHeaders;
    rPutToBuf(buf, "%s /%s%s%s %s\r\n", up->method, up->path, qsep, query, protocol);
    if (up->boundary) {
        rPutToBuf(buf, "Content-Type: multipart/form-data; boundary=%s\r\n", &up->boundary[2]);
    }
    startHeaders = rGetBufEnd(buf);
    if (headers) {
        rPutStringToBuf(buf, headers);
    }
    if (!sncaselesscontains(headers, "host:", 0)) {
        /*
            RFC 7230 5.4: an IPv6 literal must be bracketed, or the last group reads as a port.
            urlParse strips the brackets for connecting, so put them back here.
         */
        hostFmt = schr(up->host, ':') ? "Host: [%s]" : "Host: %s";
        rPutToBuf(buf, hostFmt, up->host);
        if (up->port != 80 && up->port != 443) {
            rPutToBuf(buf, ":%d", up->port);
        }
        rPutStringToBuf(buf, "\r\n");
    }
#if URL_AUTH
    // Add authentication header if credentials are set and no Authorization header in provided headers
    if (!sncaselesscontains(headers, "authorization:", 0)) {
        char *authHeader = buildAuthHeader(up);
        if (authHeader) {
            rPutStringToBuf(buf, authHeader);
            rFree(authHeader);
        } else if (up->error) {
            return R_ERR_BAD_ARGS;
        }
    }
#endif
#if URL_SSE
    if (up->sse) {
        rPutStringToBuf(buf, "Accept: text/event-stream\r\n");
        if (up->lastEventId >= 0) {
            rPutToBuf(buf, "Last-Event-ID: %zd\r\n", up->lastEventId);
        }
    }
#endif
    if (smatch(up->scheme, "ws") || smatch(up->scheme, "wss")) {
#if ME_COM_WEBSOCK
        webSocketFree(up->webSocket);
        if ((up->webSocket = webSocketAlloc(up->sock, WS_CLIENT)) == 0) {
            rFatal("sockets", "memory error");
            return R_ERR_MEMORY;
        }
        if (addWebSocketHeaders(up, buf) < 0) {
            return urlError(up, "Cannot upgrade WebSocket");
        }
#else
        return urlError(up, "WebSocket support is not enabled");
#endif
    }
    /*
        See if the caller has specified a content length and intelligently handle chunked encoding.
     */
    if (up->txLen >= 0) {
        // Caller has requested a Content-Length
        rPutToBuf(buf, "Content-Length: %zd\r\n", up->txLen);
    } else {
        up->txLen = getContentLength(rBufToString(buf), rGetBufLength(buf));
    }
    if (up->txLen < 0 && scaselessmatch(up->method, "GET") && !sncaselesscontains(headers, "Transfer-Encoding", 0)) {
        up->txLen = 0;
    }
    if (up->txLen < 0) {
        /*
            RFC 7230 3.3.3: a request with neither Content-Length nor Transfer-Encoding has a body
            length of zero, so a multipart upload sent unframed is discarded by a conforming server.
            The length of an upload is not known before its parts are written, so chunk it.
         */
        rPutToBuf(buf, "Transfer-Encoding: chunked\r\n");
    }
    if (up->txLen >= 0) {
        /*
            If using transfer encoding and not upload, defer adding blank line till writeChunk().
            Saves one write per chunk.
         */
        rPutStringToBuf(buf, "\r\n");
    }
    if (up->flags & URL_SHOW_REQ_HEADERS) {
        rLog("raw", "url", "%s\n", rBufToString(buf));
    }
    if (rWriteSocket(up->sock, rBufToString(buf), rGetBufLength(buf), up->deadline) != (ssize) rGetBufLength(buf)) {
        return urlError(up, "Cannot send request");
    }
    //  Preserve pure headers in the buffer for retries by SSE
    rAdjustBufStart(buf, startHeaders - rGetBufStart(buf));
    if (up->txLen >= 0 || up->boundary) {
        rAdjustBufEnd(buf, -2);
    }
    rCompactBuf(buf);
    up->wroteHeaders = 1;
    return 0;
}

PUBLIC void urlSetStatus(Url *up, int status)
{
    if (!up) {
        return;
    }
    up->status = status;
}

PUBLIC cchar *urlGetError(Url *up)
{
    if (!up) {
        return NULL;
    }
    if (up->error) {
        return up->error;
    }
    return NULL;
}

PUBLIC void urlSetProtocol(Url *up, int protocol)
{
    if (!up) {
        return;
    }
    up->protocol = (uint) protocol;
    up->close = protocol == 0 ? 1 : 0;
}

static bool isprintable(cchar *s, size_t len)
{
    cchar *cp;
    int   c;

    if (!s) {
        return 0;
    }
    if (len == 0) {
        return 1;
    }
    for (cp = s; cp < &s[len] && *cp; cp++) {
        c = *cp;
        if ((c > 126) || (c < 32 && c != 10 && c != 13 && c != 9)) {
            return 0;
        }
    }
    return 1;
}

/*
    Write upload data. This routine blocks. If you need non-blocking ... cut and paste.
 */
PUBLIC int urlUpload(Url *up, RList *files, RHash *forms, cchar *headersFmt, ...)
{
    RName   *field;
    cchar   *name;
    char    *headers, *path;
    int     next;
    va_list args;

    if (!up) {
        return R_ERR_BAD_ARGS;
    }
    if (!up->boundary) {
        up->boundary = sfmt("--BOUNDARY--%lld", rGetTime());
    }
    va_start(args, headersFmt);
    headers = headersFmt ? sfmtv(headersFmt, args) : 0;
    va_end(args);

    if (urlWriteHeaders(up, headers) < 0) {
        rFree(headers);
        return urlError(up, "Cannot write headers");
    }
    rFree(headers);

    if (forms) {
        for (ITERATE_NAMES(forms, field)) {
            if (urlWriteFmt(up, "%s\r\nContent-Disposition: form-data; name=\"%s\";\r\n", up->boundary,
                            field->name) < 0 ||
                urlWriteFmt(up, "Content-Type: application/x-www-form-urlencoded\r\n\r\n%s\r\n",
                            (char*) field->value) < 0) {
                return urlError(up, "Cannot write to socket");
            }
        }
    }
    if (files) {
        for (ITERATE_ITEMS(files, path, next)) {
            /*
                SECURITY Acceptable: - Must allow relative and absolute paths.  Assume caller is trusted.
             */
            if (!rFileExists(path) || scontains(path, "..")) {
                return urlError(up, "Cannot open %s", path);
            }
            name = rBasename(path);
            if (urlWriteFmt(up, "%s\r\nContent-Disposition: form-data; name=\"file%d\"; filename=\"%s\"\r\n",
                            up->boundary, next, name) < 0) {
                return urlError(up, "Cannot write to socket");
            }
            urlWrite(up, "\r\n", 0);
            if (urlWriteFile(up, path) < 0) {
                return urlError(up, "Cannot write file to socket");
            }
            urlWrite(up, "\r\n", 0);
        }
    }
    if (urlWriteFmt(up, "%s--\r\n", up->boundary) < 0) {
        return urlError(up, "Cannot write to socket");
    }
    return (int) urlFinalize(up);
}

#if ME_COM_WEBSOCK
/*
    Return zero when closed and a negative error code on errors.
 */
PUBLIC int urlWebSocket(cchar *uri, WebSocketProc callback, void *arg, cchar *headers)
{
    Url *up;
    int rc;

    if (!uri || !callback) {
        return R_ERR_BAD_ARGS;
    }
    rc = 0;
    up = urlAlloc(0);

    if (urlStart(up, "GET", uri) == 0) {
        // Read response headers and verify handshake
        if (urlWriteHeaders(up, headers) < 0 || urlFinalize(up) < 0) {
            rc = urlError(up, "Cannot finalize request");
        } else if (webSocketRun(up->webSocket, callback, arg, up->rx, up->timeout) < 0) {
            rc = urlError(up, "WebSocket error");
        }
    } else {
        if (up->error) {
            urlError(up, "Cannot fetch %s. Error: %s", uri, urlGetError(up));
        } else {
            urlError(up, "Cannot fetch %s. Bad status %d", uri, up->status);
        }
        rc = R_ERR_CANT_CONNECT;
    }
    urlFree(up);
    return rc;
}

/*
    Upgrade a client socket to use Web Sockets.
    User can set required sub-protocol in the headers via: Sec-WebSocket-Protocol: <sub-protocol>
 */
static ssize addWebSocketHeaders(Url *up, RBuf *buf)
{
    WebSocket *ws;
    uchar     bytes[16];
    char      *key;

    if (!up->webSocket) {
        return 0;
    }
    ws = up->webSocket;
    ws->parent = up;
    up->webSocket = ws;
    up->upgraded = 1;
    up->rxRemaining = URL_UNLIMITED;

    if (cryptGetRandomBytes(bytes, sizeof(bytes), 1) < 0) {
        return urlError(up, "Cannot generate random bytes for WebSocket key");
    }
    key = cryptEncode64Block(bytes, sizeof(bytes));
    if (!key) {
        return urlError(up, "Cannot encode WebSocket key");
    }
    webSocketSetClientKey(ws, key);

    urlSetStatus(up, 101);
    rPutToBuf(buf, "Upgrade: websocket\r\n");
    rPutToBuf(buf, "Connection: Upgrade\r\n");
    rPutToBuf(buf, "Sec-WebSocket-Key: %s\r\n", key);
    rPutToBuf(buf, "Sec-WebSocket-Version: %s\r\n", "13");
    rPutToBuf(buf, "X-Request-Timeout: %lld\r\n", up->timeout / TPS);
    rPutToBuf(buf, "X-Inactivity-Timeout: %lld\r\n", up->timeout / TPS);
    rFree(key);
    return 0;
}

/*
    Client verification of the server WebSockets handshake response.
    Called after reading headers.
 */
static int verifyWebSocket(Url *up)
{
    WebSocket *ws;
    char      *expected;
    cchar     *key;
    char      keybuf[128];

    assert(up->upgraded);
    ws = up->webSocket;
    assert(ws);

    if (up->status != 101) {
        urlError(up, "Bad WebSocket handshake status %d", up->status);
        return R_ERR_BAD_STATE;
    }
    if (!scaselessmatch(urlGetHeader(up, "connection"), "Upgrade")) {
        urlError(up, "Bad WebSocket Connection header");
        return R_ERR_BAD_STATE;
    }
    if (!scaselessmatch(urlGetHeader(up, "upgrade"), "WebSocket")) {
        urlError(up, "Bad WebSocket Upgrade header");
        return R_ERR_BAD_STATE;
    }
    if (sjoinbuf(keybuf, sizeof(keybuf), webSocketGetClientKey(ws), WS_MAGIC) < 0) {
        urlError(up, "Bad WebSocket client key");
        return R_ERR_BAD_STATE;
    }
    expected = cryptGetSha1Base64(keybuf, 0);
    key = urlGetHeader(up, "sec-websocket-accept");
    if (!smatch(key, expected)) {
        urlError(up, "Bad WebSocket handshake key");
        rFree(expected);
        return R_ERR_BAD_STATE;
    }
    rFree(expected);
    return 0;
}

PUBLIC WebSocket *urlGetWebSocket(Url *up)
{
    if (!up) {
        return NULL;
    }
    return up->webSocket;
}
#endif /* ME_COM_WEBSOCK */

#if URL_SSE
PUBLIC void urlSetMaxRetries(Url *up, int maxRetries)
{
    if (!up) {
        return;
    }
    up->maxRetries = (uint) maxRetries;
}

static void invokeCallback(Url *up, ssize id, cchar *event, cchar *data, void *arg)
{
    (void) arg;

    if (up->sseProc) {
        up->inCallback = 1;
        up->sseProc(up, id, event, data, up->sseArg);
        up->inCallback = 0;
    }
}

/**
    Parse buffered SSE events. Each event is separated by a double newline.
    An event can have an ID, event name and multiple data lines. Multiple data lines are concatenated
    with a newline. A comment event of a leading ":" is sometimes used to indicate a keep alive.
 */
static void parseEvents(Url *up)
{
    RBuf  *buf, *dataBuf;
    ssize id;
    cchar *data;
    char  *cp, *end, *event, *key, *start, *value;

    dataBuf = NULL;
    buf = up->responseBuf;
    rAddNullToBuf(buf);

    for (cp = rGetBufStart(buf); cp < rGetBufEnd(buf) &&
         (end = sncontains(cp, "\n\n", (size_t) (rGetBufEnd(buf) - cp))) != NULL; ) {

        id = -1;
        event = NULL;
        data = 0;
        start = cp;
        end[1] = '\0';

        /*
            Loop over all headers in the event. We are tolerant of replicated headers.
            Multiple ID and event fields will overwrite prior values.
            Multiple data fields will concatenate the data with newline separators.
            There is a \n\n at the end of the event, hence end[-1]
         */
        while (cp < &end[-1]) {
            if ((cp = getHeader(cp, &key, &value, HDR_SSE)) == NULL) {
                urlError(up, "Bad header");
                break;
            }
            /*
                An empty field name is a comment, commonly used as a keep-alive, and an unknown
                field name is reserved for future use. The protocol requires both to be ignored.
             */
            if (*key == '\0') {
                continue;
            } else if (scaselessmatch(key, "id")) {
                id = stoi(value);
            } else if (scaselessmatch(key, "event")) {
                event = value;
            } else if (scaselessmatch(key, "data")) {
                if (!dataBuf) {
                    dataBuf = rAllocBuf(0);
                }
                if (rGetBufLength(dataBuf) > 0) {
                    rPutCharToBuf(dataBuf, '\n');
                }
                rPutStringToBuf(dataBuf, value);
                data = rBufToString(dataBuf);
            }
        }
        if (up->error) {
            //  The line could not be tokenized at all, which is a framing failure
            break;
        }
        /*
            An event with no data is not dispatched, but the stream continues with the next event.
         */
        if (data && up->sseProc) {
            invokeCallback(up, id, event, data, up->sseArg);
            if (up->needFree || !up->sock) {
                break;
            }
        }
        rAdjustBufStart(buf, end - start + 2);
        rFlushBuf(dataBuf);
        cp = rGetBufStart(buf);
    }
    rFreeBuf(dataBuf);
}

/*
    Run the SSE event loop until the connection closes.
    Single-fiber model - no coordination with other fibers needed.
    Returns 0 on orderly close, < 0 on error.
 */
PUBLIC int urlSseRun(Url *up, UrlSseProc callback, void *arg, RBuf *buf, Ticks deadline)
{
    RBuf  *responseBuf;
    ssize nbytes;

    (void) buf;

    if (!up || !callback) {
        return R_ERR_BAD_ARGS;
    }

    // Configure SSE mode
    up->sseProc = callback;
    up->sseArg = arg;
    up->sse = 1;
    up->maxRetries = 0;
    up->retries = 0;
    if (deadline) {
        up->deadline = deadline;
    }

    // Initialize response buffer
    responseBuf = up->responseBuf;
    if (!responseBuf) {
        responseBuf = up->responseBuf = rAllocBuf(ME_BUFSIZE);
    }

    /*
        Note: Unlike WebSocket, we do NOT pre-parse the initial buffer here.
        The initial rx buffer may contain chunked encoding headers that needs
        to be processed by urlRead, which handles chunk decoding transparently.
     */
    while (!up->error && !up->needFree && up->sock && rState < R_STOPPING) {
        parseEvents(up);
        if (up->error || up->needFree || !up->sock) {
            break;
        }
        rCompactBuf(responseBuf);
        if (rReserveBufSpace(responseBuf, ME_BUFSIZE) < 0 ||
            (up->bufLimit > 0 && rGetBufLength(responseBuf) > up->bufLimit)) {
            urlError(up, "Response too big");
            break;
        }
        if ((nbytes = urlRead(up, responseBuf->end, rGetBufSpace(responseBuf))) > 0) {
            rAdjustBufEnd(responseBuf, nbytes);
            rAddNullToBuf(responseBuf);
        } else {
            // End of stream (nbytes == 0) or error (nbytes < 0)
            break;
        }
    }
    if (up->needFree) {
        //  The callback requested destruction; release the deferred Url.
        up->needFree = 0;
        urlFree(up);
        return R_ERR_DELETED;
    }
    return up->error ? R_ERR_CANT_COMPLETE : 0;
}

PUBLIC int urlGetEvents(cchar *uri, UrlSseProc proc, void *arg, char *headers, ...)
{
    va_list args;
    Url     *up;
    char    *sse;

    if (!uri || !proc) {
        return R_ERR_BAD_ARGS;
    }
    va_start(args, headers);
    headers = sfmtv(headers, args);
    va_end(args);

    /*
        Ask for an event stream on the request itself. urlSseRun only sets up->sse once the
        request has been written, so the Accept header in urlWriteHeaders never reaches the
        wire for the initial request and a server that content-negotiates would not send events.
     */
    sse = sjoin(headers, "Accept: text/event-stream\r\n", NULL);
    rFree(headers);
    headers = sse;

    if ((up = urlAlloc(0)) == 0) {
        rFree(headers);
        return R_ERR_MEMORY;
    }
    if (fetch(up, "GET", uri, 0, 0, headers) != URL_CODE_OK) {
        rFree(headers);
        urlFree(up);
        return R_ERR_CANT_COMPLETE;
    }
    if (urlSseRun(up, proc, arg, up->rx, up->deadline) != R_ERR_DELETED) {
        urlFree(up);
    }
    rFree(headers);
    return 0;
}
#endif /* URL_SSE */

/*
    Sets up->error if not already set, traces the result and CLOSES the socket.
 */
PUBLIC int urlError(Url *up, cchar *fmt, ...)
{
    va_list ap;

    if (!up) {
        return R_ERR_BAD_ARGS;
    }
    if (!up->error) {
        va_start(ap, fmt);
        rFree(up->error);
        up->error = sfmtv(fmt, ap);
        va_end(ap);
        rTrace("url", "%s, for %s:%d", up->error, up->host ? up->host : "localhost", up->port);
    }
    rCloseSocket(up->sock);
    return R_ERR_CANT_COMPLETE;
}

/******************************** Authentication ********************************/
#if URL_AUTH

PUBLIC void urlSetAuth(Url *up, cchar *username, cchar *password, cchar *authType)
{
    if (!up) {
        return;
    }
    rFree(up->username);
    rFree(up->password);
    rFree(up->authType);

    up->username = username ? sclone(username) : NULL;
    up->password = password ? sclone(password) : NULL;
    up->authType = authType ? sclone(authType) : NULL;
    up->offeredBasic = 0;

    // Clear challenge state when credentials are cleared
    if (!username || !password) {
        clearChallenge(up);
    }
}

/*
    Clear the authentication challenge and its origin.
 */
static void clearChallenge(Url *up)
{
    up->offeredBasic = 0;
    rFree(up->realm);
    up->realm = NULL;
    rFree(up->nonce);
    up->nonce = NULL;
    rFree(up->qop);
    up->qop = NULL;
    rFree(up->opaque);
    up->opaque = NULL;
    rFree(up->algorithm);
    up->algorithm = NULL;
    up->nc = 0;
    rFree(up->authOrigin);
    up->authOrigin = NULL;
}

/*
    Return the origin of the current request as an allocated "scheme://host:port"
 */
static char *getOrigin(Url *up)
{
    return sfmt("%s://%s:%d", up->scheme ? up->scheme : "", up->host ? up->host : "", up->port);
}

/*
    RFC 7230 3.2.6 and RFC 7617 2: credentials and challenge parameters never contain control characters
 */
static bool hasControlChars(cchar *s)
{
    for (; s && *s; s++) {
        if ((uchar) * s < ' ' || *s == 0x7f) {
            return 1;
        }
    }
    return 0;
}

/*
    Build an authorization header for the strongest scheme the configuration and the server's
    challenge allow. The configured authType is a floor: a challenge can raise the scheme but
    never lower it, and without a configured type nothing is sent until the server has asked.
    Returns an allocated string, or NULL if there is nothing to send yet.
 */
static char *buildAuthHeader(Url *up)
{
    char *origin;

    if (!up || !up->username || !up->password) {
        return NULL;
    }
    //  A challenge authorizes answering the origin that issued it, not every later request on this handle
    if (up->authOrigin) {
        origin = getOrigin(up);
        if (!smatch(origin, up->authOrigin)) {
            clearChallenge(up);
        }
        rFree(origin);
    }
    //  These go onto the wire, so a control character could end the header and inject another
    if (hasControlChars(up->username) || hasControlChars(up->password) || hasControlChars(up->realm) ||
        hasControlChars(up->nonce) || hasControlChars(up->opaque)) {
        urlError(up, "Invalid character in authentication credentials");
        return NULL;
    }
    if (scaselessmatch(up->authType, "digest") || (up->realm && up->nonce)) {
        //  NULL until a Digest challenge has supplied the realm and nonce
        return buildDigestAuthHeader(up);
    }
    if (scaselessmatch(up->authType, "basic") || up->offeredBasic) {
        return buildBasicAuthHeader(up);
    }
    return NULL;
}

/*
    Build HTTP Basic authentication header.
    Returns allocated string in the format: "Authorization: Basic <base64>\r\n"
 */
static char *buildBasicAuthHeader(Url *up)
{
    char *credentials, *encoded, *header;

    if (!up || !up->username || !up->password) {
        return NULL;
    }
    // Warn if sending Basic auth over unencrypted HTTP (development warning)
    // Note: This is acceptable for testing with self-signed certificates per project policy
    if (up->scheme && scaselessmatch(up->scheme, "http")) {
        rDebug("url", "Sending Basic authentication over unencrypted HTTP (OK for development/testing)");
    }
    credentials = sfmt("%s:%s", up->username, up->password);
    encoded = cryptEncode64(credentials);
    header = sfmt("Authorization: Basic %s\r\n", encoded);

    rFree(credentials);
    rFree(encoded);
    return header;
}

/*
    Escape quotes and backslashes for RFC 7616 quoted-string values.
    Per RFC 7616 Section 3.4, quoted-string values must escape:
    - Backslash (\) becomes \\
    - Quote (") becomes \"
    Returns allocated string (caller must free) or NULL if str is NULL.
 */
static char *escapeQuotedString(cchar *str)
{
    RBuf  *buf;
    cchar *cp;

    if (!str) {
        return NULL;
    }
    buf = rAllocBuf(0);
    for (cp = str; *cp; cp++) {
        if (*cp == '"' || *cp == '\\') {
            rPutCharToBuf(buf, '\\');  // Add backslash before quote or backslash
        }
        rPutCharToBuf(buf, *cp);
    }
    return rBufToStringAndFree(buf);
}

/*
    Build HTTP Digest authentication header with specified hash algorithm.
    Supports MD5 (RFC 2617) and SHA-256 (RFC 7616).
    Returns allocated string or NULL on error.
 */
static char *buildDigestAuthHeader(Url *up)
{
    char *ha1, *ha2, *response, *header, *cnonce, *nc, *temp, *algorithm;
    char *escapedUsername, *escapedRealm, *escapedNonce, *escapedOpaque;

    if (!up || !up->username || !up->password || !up->realm || !up->nonce || !up->qop) {
        return NULL;
    }
    // Increment nonce count
    up->nc++;

    // Determine hash algorithm (default to MD5 for RFC 2617 compatibility)
    algorithm = up->algorithm ? up->algorithm : "MD5";

    // Calculate HA1 = HASH(username:realm:password)
    temp = sfmt("%s:%s:%s", up->username, up->realm, up->password);
    if (scaselessmatch(algorithm, "SHA-256")) {
        ha1 = cryptGetSha256((uchar*) temp, slen(temp));
    } else {
        // Default to MD5 for RFC 2617 compatibility
        ha1 = cryptGetMd5((uchar*) temp, slen(temp));
    }
    rFree(temp);

    // Calculate HA2 = HASH(method:uri)
    if (up->query) {
        temp = sfmt("%s:/%s?%s", up->method, up->path, up->query);
    } else {
        temp = sfmt("%s:/%s", up->method, up->path);
    }
    if (scaselessmatch(algorithm, "SHA-256")) {
        ha2 = cryptGetSha256((uchar*) temp, slen(temp));
    } else {
        ha2 = cryptGetMd5((uchar*) temp, slen(temp));
    }
    rFree(temp);

    // Generate client nonce for qop (cryptographically secure random)
    cnonce = cryptID(16);
    nc = sfmt("%08x", (unsigned int) up->nc);

    /*
        Escape digest parameters per RFC 7616 Section 3.4 (quoted-string values)
        Defense-in-depth: Prevents header injection from malicious servers
     */
    escapedUsername = escapeQuotedString(up->username);
    escapedRealm = escapeQuotedString(up->realm);
    escapedNonce = escapeQuotedString(up->nonce);
    escapedOpaque = escapeQuotedString(up->opaque);  // NULL-safe

    // response = HASH(HA1:nonce:nc:cnonce:qop:HA2)
    temp = sfmt("%s:%s:%s:%s:%s:%s", ha1, up->nonce, nc, cnonce, up->qop, ha2);
    if (scaselessmatch(algorithm, "SHA-256")) {
        response = cryptGetSha256((uchar*) temp, slen(temp));
    } else {
        response = cryptGetMd5((uchar*) temp, slen(temp));
    }
    rFree(temp);

    header = sfmt("Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"/%s%s%s\", "
                  "qop=%s, nc=%s, cnonce=\"%s\", response=\"%s\", algorithm=%s%s%s%s\r\n",
                  escapedUsername, escapedRealm, escapedNonce, up->path, up->query ? "?" : "",
                  up->query ? up->query : "",
                  up->qop, nc, cnonce, response, algorithm, escapedOpaque ? ", opaque=\"" : "",
                  escapedOpaque ? escapedOpaque : "", escapedOpaque ? "\"" : "");
    rFree(ha1);
    rFree(ha2);
    rFree(response);
    rFree(cnonce);
    rFree(nc);
    rFree(escapedUsername);
    rFree(escapedRealm);
    rFree(escapedNonce);
    rFree(escapedOpaque);
    return header;
}

/*
    Parse a quoted-string or token value from digest challenge per RFC 7230.
    Handles escaped quotes (\") within quoted strings.
    Returns allocated string (caller must free) or NULL on error.
 */
static char *parseDigestValue(char **tokPtr)
{
    char *tok, *value, *dst;
    bool quoted;

    tok = *tokPtr;

    // Skip whitespace
    while (*tok && isWhite(*tok)) tok++;

    quoted = (*tok == '"');
    if (quoted) {
        tok++;                         // Skip opening quote
        value = rAlloc(slen(tok) + 1); // Allocate max needed (will be <= this after unescaping)
        dst = value;

        // Parse quoted-string with escape handling
        while (*tok && *tok != '"') {
            if (*tok == '\\' && *(tok + 1)) {
                // Escaped character - copy the next char literally
                tok++;
                *dst++ = *tok++;
            } else {
                *dst++ = *tok++;
            }
        }
        *dst = '\0';

        if (*tok == '"') {
            tok++;  // Skip closing quote
        }
    } else {
        // Unquoted token - read until comma or whitespace
        value = tok;
        while (*tok && *tok != ',' && !isWhite(*tok)) tok++;
        if (*tok) {
            value = snclone(value, (size_t) (tok - value));
        } else {
            value = sclone(value);
        }
    }

    // Skip trailing whitespace and comma
    while (*tok && (isWhite(*tok) || *tok == ',')) tok++;

    *tokPtr = tok;
    return value;
}

/*
    Parse WWW-Authenticate header for Digest or Basic authentication.
    Returns true if successfully parsed.
 */
PUBLIC bool urlParseAuthChallenge(Url *up)
{
    cchar *challenge;
    char  *buf, *tok, *key, *value, *qopCopy, *qtok, *next;
    bool  selected, stale;

    if (!up) {
        return 0;
    }
    challenge = urlGetHeader(up, "WWW-Authenticate");
    if (!challenge) {
        return 0;
    }
    buf = sclone(challenge);
    stale = 0;

    /*
        A challenge may raise the scheme above the configured floor, never lower it. Basic sends
        the password itself, so a client configured for Digest must not answer a Basic challenge,
        and a client with no configured type answers one only where the connection is encrypted.
     */
    if (sncaselesscmp(buf, "basic", 5) == 0) {
        rFree(buf);
        if (scaselessmatch(up->authType, "digest")) {
            urlError(up, "Server offers Basic authentication but Digest is configured");
            return 0;
        }
        if (!up->authType && !rIsSocketSecure(up->sock)) {
            urlError(up, "Basic authentication refused over an insecure connection");
            return 0;
        }
        up->offeredBasic = 1;
        rFree(up->authOrigin);
        up->authOrigin = getOrigin(up);
        return 1;
    }
    if (sncaselesscmp(buf, "digest", 6) == 0) {
        rFree(up->authOrigin);
        up->authOrigin = getOrigin(up);

        //  Each challenge stands alone: nothing from an earlier challenge carries over
        rFree(up->qop);
        up->qop = NULL;
        rFree(up->opaque);
        up->opaque = NULL;
        rFree(up->algorithm);
        up->algorithm = NULL;

        // Parse digest parameters
        tok = &buf[6];
        while (*tok) {
            // Skip whitespace
            while (*tok && isWhite(*tok)) tok++;
            if (!*tok) break;

            // Parse key
            key = tok;
            while (*tok && *tok != '=') tok++;
            if (*tok != '=') {
                break;
            }
            *tok++ = '\0';
            key = strim(key, " \t", 0);

            // Parse value (handles both quoted and unquoted, with escape support)
            value = parseDigestValue(&tok);
            if (!value) {
                rFree(buf);
                return 0;
            }
            // Validate parameter length to prevent DoS attacks from malicious servers
            if (slen(value) > MAX_DIGEST_PARAM_LEN) {
                rError("url", "Digest parameter '%s' too long: %d bytes (max %d)", key, (int) slen(value),
                       MAX_DIGEST_PARAM_LEN);
                rFree(value);
                rFree(buf);
                return 0;
            }
            // Store digest parameters
            if (scaselessmatch(key, "realm")) {
                rFree(up->realm);
                up->realm = value;
            } else if (scaselessmatch(key, "nonce")) {
                //  The nonce count restarts at 1 for every new nonce (RFC 7616 3.4)
                if (!smatch(up->nonce, value)) {
                    up->nc = 0;
                }
                rFree(up->nonce);
                up->nonce = value;
            } else if (scaselessmatch(key, "qop")) {
                rFree(up->qop);
                up->qop = value;
            } else if (scaselessmatch(key, "opaque")) {
                rFree(up->opaque);
                up->opaque = value;
            } else if (scaselessmatch(key, "algorithm")) {
                rFree(up->algorithm);
                up->algorithm = value;
            } else if (scaselessmatch(key, "stale")) {
                // RFC 7616: stale=true means nonce expired, retry with same credentials
                stale = scaselessmatch(value, "true");
                rFree(value);
            } else {
                // Unknown parameter - log for security monitoring
                rDebug("url", "Unknown digest auth parameter: %s=%s", key, value);
                rFree(value);
            }
        }

        //  stale=true supplies a fresh nonce for verified credentials.
        if (stale) {
            rDebug("url", "Server indicated stale nonce - retrying with the new nonce");
        }
        // Validate algorithm - only MD5 and SHA-256 are supported per RFC 2617/7616
        if (up->algorithm && !scaselessmatch(up->algorithm, "MD5") && !scaselessmatch(up->algorithm, "SHA-256")) {
            rError("url", "Unsupported digest algorithm: %s (only MD5 and SHA-256 are supported)", up->algorithm);
            rFree(buf);
            return 0;
        }
        /* Refuse no-qop Digest: without cnonce a hostile server can precompute password guesses. */
        if (!up->qop) {
            rError("url", "Digest challenge without qop refused");
            rFree(buf);
            return 0;
        }
        /*
            Validate qop - only "auth" is supported, not "auth-int"
            Parse as comma-separated list per RFC 7616
         */
        selected = 0;
        qopCopy = sclone(up->qop);
        qtok = stok(qopCopy, ",", &next);
        while (qtok) {
            qtok = strim(qtok, " \t", 0);
            if (scaselessmatch(qtok, "auth")) {
                selected = 1;
                break;
            }
            qtok = stok(NULL, ",", &next);
        }
        rFree(qopCopy);
        if (!selected) {
            rError("url", "Unsupported digest qop: %s (only 'auth' is supported, not 'auth-int')", up->qop);
            rFree(buf);
            return 0;
        }
        //  Reduce the offered list to the single chosen alternative for the response header
        rFree(up->qop);
        up->qop = sclone("auth");
        rFree(buf);
        return 1;
    }
    rFree(buf);
    return 0;
}

#endif /* URL_AUTH */

#else
void dummyUrl(void)
{
}
#endif /* ME_COM_URL */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

#else
void dummyUrl(){}
#endif /* ME_COM_URL */
