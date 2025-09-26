/*
 * URL client Library Source
 */

#include "url.h"

#if ME_COM_URL



/********* Start of file ../../../src/urlLib.c ************/

/**
    url.c - URL client HTTP library.

    This is a pragmatic, compact http client. It does not attempt to be fully HTTP/1.1 compliant.
    It supports HTTP/1 keep-alive and transfer-chunking encoding.
    This module uses fiber coroutines to permit parallel execution with other application fibers.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/


#include    "websockets.h"

#if ME_COM_URL
/*********************************** Locals ***********************************/

#define URL_CHUNK_START      1                 /**< Start of a new chunk */
#define URL_CHUNK_DATA       2                 /**< Start of chunk data */
#define URL_CHUNK_EOF        4                 /**< End of all chunk data */

#ifndef ME_URL_TIMEOUT
    #define ME_URL_TIMEOUT   0                 /**< Default timeout (none) */
#endif

#ifndef URL_MAX_RESPONSE
    #define URL_MAX_RESPONSE (1024 * 1024)     /**< Maximum response size */
#endif

#ifndef URL_MAX_RETRIES
    #define URL_MAX_RETRIES  0                 /**< Maximum number of retries */
#endif

#define URL_BUFSIZE          4096              /**< Buffer size */
#define URL_UNLIMITED        MAXINT            /**< Unlimited size */

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
static int fetch(Url *up, cchar *method, cchar *uri, cvoid *data, ssize len, cchar *headers);
static ssize getContentLength(cchar *headers, ssize length);
static char *getHeader(char *line, char **key, char **value, int flags);
static bool isprintable(cchar *s, ssize len);
static int startRequest(Url *up);
static void parseEvents(Url *up);
static bool parseHeaders(Url *up);
static int parseResponse(Url *up, ssize size);
static ssize readBlock(Url *up, char *buf, ssize bufsize);
static ssize readChunk(Url *up, char *buf, ssize bufsize);
static int readHeaders(Url *up);
static ssize readSocket(Url *up, ssize bufsize);
static ssize readUntil(Url *up, cchar *until, char *buf, ssize bufsize);
static void resetState(Url *up);
static void resetSocket(Url *up);
static void setDeadline(Url *up);
static int verifyWebSocket(Url *up);
static int writeChunkDivider(Url *up, ssize len);
static ssize writeWebSocketHeaders(Url *up, RBuf *buf);

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
    up->deadline = MAXINT64;
    up->bufLimit = URL_MAX_RESPONSE;

    if (flags == 0) {
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
#if ME_COM_WEBSOCKETS
    if (up->webSocket) {
        webSocketFree(up->webSocket);
    }
#endif /* ME_COM_WEBSOCKETS */
#if URL_SSE
    if (up->abortEvent) {
        rStopEvent(up->abortEvent);
        up->abortEvent = 0;
    }
#endif /* URL_SSE */
    memset(up, 0, sizeof(Url));
    rFree(up);
}

static void resetState(Url *up)
{
    if (!up) {
        return;
    }
    resetSocket(up);
    up->boundary = 0;
    up->chunked = 0;
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

PUBLIC void urlSetBufLimit(Url *up, ssize limit)
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
    up->method = supper(sclone(method));
    rFree(up->url);
    up->url = sclone(url);
    return startRequest(up);
}

static int startRequest(Url *up)
{
    if (!up) {
        return R_ERR_BAD_ARGS;
    }
    resetState(up);
    setDeadline(up);
    connectHost(up);
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
    if (urlParse(up, up->url) < 0) {
        urlError(up, "Bad URL");
        return R_ERR_BAD_ARGS;
    }
    //  Validate host length before copying to prevent buffer overflow
    if (up->host && slen(up->host) >= sizeof(host)) {
        return urlError(up, "Host name too long");
    }
    //  Save prior host and port incase connection can be reused
    scopy(host, sizeof(host), up->host);
    port = up->port;

    if (up->sock) {
        if ((smatch(up->scheme, "https") || smatch(up->scheme, "wss") != rIsSocketSecure(up->sock)) ||
            (*host && !smatch(up->host, host)) || (port && up->port != port)) {
            rFreeSocket(up->sock);
            up->sock = 0;
        }
    }
    if (!up->sock) {
        up->sock = rAllocSocket();
    }
    if ((smatch(up->scheme, "https") || smatch(up->scheme, "wss")) && !rIsSocketSecure(up->sock)) {
        rSetTls(up->sock);
    }
    if (up->sock->fd == INVALID_SOCKET) {
        if (up->flags & URL_SHOW_REQ_HEADERS) {
            rLog("raw", "url", "\nConnect: %s://%s:%d\n", up->sock->tls ? "https" : "http", up->host, up->port);
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
 */
static int fetch(Url *up, cchar *method, cchar *uri, cvoid *data, ssize len, cchar *headers)
{
    char *allocHeaders;

    if (!up || !method || !uri) {
        return R_ERR_BAD_ARGS;
    }
    if (urlStart(up, method, uri) == 0) {
        if (getContentLength(headers, -1) < 0) {
            //  Add a content length header if not already present
            if (len < 0) {
                len = data ? slen(data) : 0;
            }
            if (len > 0) {
                headers = allocHeaders = sfmt("Content-Length: %ld\r\n%s", len, headers);
            } else {
                allocHeaders = NULL;
            }
            if (urlWriteHeaders(up, headers) < 0) {
                rFree(allocHeaders);
                return urlError(up, "Cannot write headers");
            }
            rFree(allocHeaders);
        } else {
            if (urlWriteHeaders(up, headers) < 0) {
                return urlError(up, "Cannot write headers");
            }
        }
        if (data && len > 0) {
            if (urlWrite(up, data, len) >= 0) {
                return up->status;
            }
        }
        if (urlFinalize(up) < 0) {
            return urlError(up, "Cannot finalize");
        }
        return up->status;
    }
    if (!up->error) {
        urlError(up, "Cannot run \"%s\" %s", method, uri);
    }
    return R_ERR_CANT_CONNECT;
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
        rFree(headers);
        return NULL;
    }
    if ((response = urlGetResponse(up)) == 0) {
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
    json = urlJson(up, "GET", uri, NULL, 0, headers);
    urlFree(up);
    rFree(headers);
    return json;
}

PUBLIC char *urlPost(cchar *uri, cvoid *data, ssize len, cchar *headersFmt, ...)
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
        rFree(headers);
        return NULL;
    }
    if ((response = urlGetResponse(up)) == 0) {
        rFree(headers);
        return NULL;
    }
    result = sclone(response);
    urlFree(up);
    rFree(headers);
    return result;
}

PUBLIC Json *urlPostJson(cchar *uri, cvoid *data, ssize len, cchar *headersFmt, ...)
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
    json = urlJson(up, "POST", uri, data, len, headers);
    urlFree(up);
    rFree(headers);
    return json;
}

PUBLIC int urlFetch(Url *up, cchar *method, cchar *uri, cvoid *data, ssize len, cchar *headersFmt, ...)
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

PUBLIC Json *urlJson(Url *up, cchar *method, cchar *uri, cvoid *data, ssize len, cchar *headersFmt, ...)
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
            return 0;
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

static void setDeadline(Url *up)
{
    if (!up) {
        return;
    }
    if (up->timeout) {
        up->deadline = up->timeout ? rGetTicks() + up->timeout : 0;
    } else {
        up->deadline = MAXINT64;
    }
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
    /*
        Verify the WebSocket handshake if required
     */
    if (up->webSocket && verifyWebSocket(up) < 0) {
        return urlError(up, "Cannot verify WebSockets connection");
    }
    return up->error ? R_ERR_CANT_WRITE: 0;
}

/*

    Write body data. Set bufsize to zero to signify end of body if the content length is not defined.
    If finalizing, read the response headers after the last write packet.
    This routine will handle chunking transparently and will trace request data if required.
    May close the socket if required.
 */
PUBLIC ssize urlWrite(Url *up, cvoid *buf, ssize bufsize)
{
    if (!up) {
        return R_ERR_BAD_ARGS;
    }
    if (up->finalized) {
        if (buf && bufsize > 0) {
            return urlError(up, "Cannot write after finalize");
        }
    }
    if (buf == NULL) {
        bufsize = 0;
    } else if (bufsize < 0) {
        bufsize = slen(buf);
    }
    if (!up->wroteHeaders && urlWriteHeaders(up, NULL) < 0) {
        //  Already closed
        return R_ERR_CANT_WRITE;
    }
    if (writeChunkDivider(up, bufsize) < 0) {
        //  Already closed
        return R_ERR_CANT_WRITE;
    }
    if (bufsize > 0) {
        if (up->wroteHeaders && up->flags & URL_SHOW_REQ_BODY) {
            rLog("raw", "url", "%.*s\n\n", (int) bufsize, (char*) buf);
        }
        if (rWriteSocket(up->sock, buf, bufsize, up->deadline) != bufsize) {
            return urlError(up, "Cannot write to socket");
        }
    }
    /*
        If all data written, finalize and read the response headers.
     */
    if (bufsize == 0 || bufsize == up->txLen) {
        if (!up->rxHeaders && readHeaders(up) < 0) {
            //  Already closed
            return R_ERR_CANT_READ;
        }
    }
    //  Close the socket if required once we have read all the required response data
    if (up->close && up->rxRemaining == 0) {
        rCloseSocket(up->sock);
    }
    return bufsize;
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
    ssize nbytes;
    int   fd;

    if (!up || path == NULL) {
        return R_ERR_BAD_ARGS;
    }
    if ((fd = open(path, R_OK)) < 0) {
        return urlError(up, "Cannot open %s", path);
    }
    do {
        if ((nbytes = read(fd, buf, sizeof(buf))) < 0) {
            urlError(up, "Cannot read from %s", path);
            break;
        }
        if (nbytes > 0 && urlWrite(up, buf, nbytes) < 0) {
            urlError(up, "Cannot write to socket");
            break;
        }
    } while (nbytes > 0);

    close(fd);
    return nbytes < 0 ? R_ERR_CANT_WRITE : 0;
}

/*
    Write a chunked transfer encoding header for a given length.
    If len is zero, write the chunked trailer.
 */
static int writeChunkDivider(Url *up, ssize len)
{
    char chunk[24];

    if (!up) {
        return R_ERR_BAD_ARGS;
    }
    if (up->txLen >= 0 || up->boundary) {
        //  Content-Length is known or doing multipart mime file upload
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

    if ((size = readUntil(up, "\r\n\r\n", NULL, 0)) < 0) {
        return R_ERR_CANT_READ;
    }
    if (parseResponse(up, size) < 0) {
        return R_ERR_CANT_READ;
    }
    return up->status;
}

static int parseResponse(Url *up, ssize headerSize)
{
    RBuf  *buf;
    char  *end, *tok;
    ssize len;

    buf = up->rx;
    if (headerSize <= 10) {
        return urlError(up, "Bad response header");
    }
    end = buf->start + headerSize;
    end[-2] = '\0';

    if (up->flags & URL_SHOW_RESP_HEADERS) {
        rLog("raw", "url", "%s\n", rBufToString(buf));
    }
    if ((tok = strchr(buf->start, ' ')) == 0) {
        return R_ERR_BAD_STATE;
    }
    while (*tok == ' ') tok++;
    up->status = atoi(tok);
    if (up->status < 100 || up->status > 599) {
        return urlError(up, "Bad response status");
    }
    rAdjustBufStart(buf, end - buf->start);

    if ((tok = strchr(tok, '\n')) != 0) {
        len = end - 2 - ++tok;
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
PUBLIC ssize urlRead(Url *up, char *buf, ssize bufsize)
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
static ssize readChunk(Url *up, char *buf, ssize bufsize)
{
    ssize chunkSize, nbytes;
    char  cbuf[32], *end;

    nbytes = 0;

    if (up->chunked == URL_CHUNK_START) {
        if (readUntil(up, "\r\n", cbuf, sizeof(cbuf)) < 0) {
            return urlError(up, "Bad chunk data");
        }
        cbuf[sizeof(cbuf) - 1] = '\0';
        chunkSize = strtol(cbuf, &end, 16);
        if (chunkSize < 0 || chunkSize > SSIZE_MAX || (*end != '\0' && !isspace(*end))) {
            return urlError(up, "Bad chunk specification");
        }
        if (chunkSize) {
            //  Set rxRemaining to the next chunk size
            up->rxRemaining = chunkSize;
            up->chunked = URL_CHUNK_DATA;

        } else {
            /*
                EOF - end of body so consume the trailing <CR><NL>
             */
            if (readUntil(up, "\r\n", cbuf, sizeof(cbuf)) < 0) {
                return urlError(up, "Bad chunk data");
            }
            up->chunked = URL_CHUNK_EOF;
            up->rxRemaining = 0;
        }
    }
    if (up->chunked == URL_CHUNK_DATA) {
        if ((nbytes = readBlock(up, buf, min(bufsize, up->rxRemaining))) <= 0) {
            return urlError(up, "Cannot read chunk data");
        }
        up->rxRemaining -= nbytes;
        if (up->rxRemaining == 0) {
            //  Move onto the next chunk. Set rxRemaining high until we know the chunk length.
            up->chunked = URL_CHUNK_START;
            up->rxRemaining = URL_UNLIMITED;
            if (readUntil(up, "\r\n", cbuf, sizeof(cbuf)) < 0) {
                return urlError(up, "Bad chunk data");
            }
        }
    }
    return nbytes;
}

/*
    Low-level read data from the socket into the web->rx buffer.
    Read up to bufsize bytes.
    Return the number of bytes read or a negative error code.
 */
static ssize readSocket(Url *up, ssize bufsize)
{
    RBuf  *bp;
    ssize nbytes, space, toRead;

    bp = up->rx;
    rCompactBuf(bp);
    space = min(bufsize <= ME_BUFSIZE ? ME_BUFSIZE : ME_BUFSIZE * 2, up->rxRemaining);
    rReserveBufSpace(bp, space);
    toRead = min(rGetBufSpace(bp), up->rxRemaining);
    if ((nbytes = rReadSocket(up->sock, bp->start, toRead, up->deadline)) < 0) {
        return urlError(up, "Cannot read from socket");
    }
    rAdjustBufEnd(bp, nbytes);
    if (!up->chunked && up->rxRemaining > 0) {
        up->rxRemaining -= nbytes;
    }
    return rGetBufLength(bp);
}

/*
    Read a block of data into the supplied buffer up to bufsize.
    This reads data through the rx buffer into the user supplied buffer.
 */
static ssize readBlock(Url *up, char *buf, ssize bufsize)
{
    RBuf  *bp;
    ssize nbytes;

    bp = up->rx;

    if (rGetBufLength(bp) == 0 && readSocket(up, bufsize) < 0) {
        return R_ERR_CANT_READ;
    }
    nbytes = min(rGetBufLength(bp), bufsize);
    if (buf && nbytes > 0) {
        memcpy(buf, bp->start, nbytes);
        rAdjustBufStart(bp, nbytes);
    }
    return nbytes;
}

/*
    Read response data until a designated pattern (can be null). Data is read through the rx buffer.
    When reading until a pattern, may overread and data must be buffered for the next read.
 */
static ssize readUntil(Url *up, cchar *until, char *buf, ssize bufsize)
{
    RBuf  *bp;
    char  *end;
    ssize len, nbytes, toRead;

    bp = up->rx;
    rAddNullToBuf(bp);

    while ((end = strstr(bp->start, until)) == 0) {
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
            up->rxRemaining -= nbytes;
        }
    }
    nbytes = (end - bp->start + slen(until));
    if (buf && nbytes > 0) {
        len = min(nbytes, bufsize);
        memcpy(buf, bp->start, len);
        rAdjustBufStart(bp, len);
    }
    //  Special case for reading headers. Don't transfer data if bufsize is zero, but do return nbytes.
    return nbytes;
}

PUBLIC void urlSetCerts(Url *up, cchar *ca, cchar *key, cchar *cert, cchar *revoke)
{
    if (!up) {
        return;
    }
    if (!up->sock) {
        up->sock = rAllocSocket();
    }
    rSetSocketCerts(up->sock, ca, key, cert, revoke);
    up->certsDefined = 1;
}

PUBLIC void urlSetCiphers(Url *up, cchar *ciphers)
{
    if (!up) {
        return;
    }
    if (!up->sock) {
        up->sock = rAllocSocket();
    }
    rSetSocketCiphers(up->sock, ciphers);
}

PUBLIC void urlSetVerify(Url *up, int verifyPeer, int verifyIssuer)
{
    if (!up) {
        return;
    }
    if (!up->sock) {
        up->sock = rAllocSocket();
    }
    rSetSocketVerify(up->sock, verifyPeer, verifyIssuer);
}

/*
    Parse the URL and set the up->host, up->port, up->path, up->hash, and up->query fields.
    Return 0 on success or a negative error code.
    The uri is already trimmed of whitespace.
 */
PUBLIC int urlParse(Url *up, cchar *uri)
{
    char *next, *tok;
    bool hasScheme;
    char *end;
    long port;

    if (!up || uri == NULL) {
        return R_ERR_BAD_ARGS;
    }
    rFree(up->urlbuf);

    up->urlbuf = sclone(uri);
    up->scheme = "http";
    up->host = "localhost";
    up->port = 80;
    up->path = "/";
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
        /* IPv6  [::]:port/url */
        up->host = &tok[1];
        *next++ = 0;
        tok = next;

    } else if (*tok && *tok != '/' && *tok != ':' && (hasScheme || strchr(tok, ':'))) {
        // hostname:port/path
        up->host = tok;
        if ((tok = spbrk(tok, ":/")) != 0) {
            if (*tok == ':') {
                *tok++ = 0;
                errno = 0;
                port = (int) strtol(tok, &end, 10);
                if (errno == ERANGE || (*end != '\0' && *end != '/')) {
                    urlError(up, "Invalid port number");
                    return R_ERR_BAD_STATE;
                }
                if (port < 1 || port > 65535) {
                    urlError(up, "Invalid port number");
                    return R_ERR_BAD_STATE;
                }
                up->port = (int) port;
                if ((tok = schr(tok, '/')) != 0) {
                    tok++;
                } else {
                    tok = "";
                }
            } else {
                *tok++ = 0;
            }
        } else {
            // Should never happen
            tok = "";
        }
    } else if (*tok && *tok == ':') {
        //  :port/path without hostname
        *tok++ = 0;
        errno = 0;
        port = (int) strtol(tok, &end, 10);
        if (errno == ERANGE || (*end != '\0' && *end != '/')) {
            urlError(up, "Invalid port number");
            return R_ERR_BAD_STATE;
        }
        if (port < 1 || port > 65535) {
            urlError(up, "Invalid port number");
            return R_ERR_BAD_STATE;
        }
        up->port = (int) port;
        if ((tok = schr(tok, '/')) != 0) {
            tok++;
        } else {
            tok = "";
        }
    }
    if (*tok) {
        up->path = tok;
    }
    if (slen(up->host) > 255) {
        urlError(up, "Invalid host name");
        return R_ERR_BAD_STATE;
    }
    return 0;
}

/*
    Return the entire URL response which is stored in the up->responseBuf held by the URL object.
    No need for the caller to free. User should not call urlRead if using urlGetResponse.
 */
PUBLIC RBuf *urlGetResponseBuf(Url *up)
{
    RBuf  *buf;
    cchar *contentLength;
    ssize len, nbytes;

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
    contentLength = urlGetHeader(up, "Content-Length");
    if (contentLength) {
        len = stoi(contentLength);
        if (len < 0 || len >= SSIZE_MAX || len >= up->bufLimit) {
            urlError(up, "Invalid Content-Length");
            return NULL;
        }
    } else {
        len = -1;
    }
    if (up->bufLimit > 0 && len >= up->bufLimit) {
        urlError(up, "Response too big");
        return NULL;
    }
    if (!up->gotResponse && len != 0) {
        do {
            nbytes = contentLength ? len : min(buf->buflen * 2, ME_BUFSIZE * 1024);
            if (up->bufLimit > 0) {
                nbytes = min(nbytes, up->bufLimit - rGetBufLength(buf));
                if (nbytes <= 0) {
                    urlError(up, "Response too big");
                    break;
                }
            }
            rReserveBufSpace(buf, nbytes);
            if ((nbytes = urlRead(up, rGetBufEnd(buf), nbytes)) < 0) {
                if (up->rxRemaining) {
                    urlError(up, "Cannot read response");
                    return NULL;
                }
            }
            rAdjustBufEnd(buf, nbytes);
            if (len >= 0) {
                len -= nbytes;
                if (len <= 0) {
                    break;
                }
            }
        } while (nbytes > 0);
        up->gotResponse = 1;
    }
    if (up->flags & URL_SHOW_RESP_BODY && isprintable(rGetBufStart(buf), rGetBufLength(buf))) {
        rLog("raw", "url", "Response Body >>>>\n\n%*s", (int) rGetBufLength(buf), (char*) buf->start);
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
            while (*cp == ' ') cp++;
            value = cp;
            break;
        }
        //  Step over header
        cp += slen(cp) + 1;
        if (cp < end && *cp) {
            //  Step over value
            cp += slen(cp) + 1;
        }
    }
    return value;
}

PUBLIC char *urlGetCookie(Url *up, cchar *name)
{
    cchar *cp, *end, *ep, *start;
    char  *value;

    if (!up) {
        return NULL;
    }
    if (!up->rxHeaders) {
        return 0;
    }
    start = rGetBufStart(up->rxHeaders);
    end = up->rxHeaders->end;
    value = 0;

    /*
        Examine all Set-Cookie headers for the required cookie name. Can be multiple.
     */
    for (cp = start; cp < end; cp++) {
        if (scaselessmatch(cp, "Set-Cookie")) {
            cp += 11;
            while (isWhite(*cp)) cp++;
            for (ep = cp + 1; *ep && *ep != '='; ep++) {
            }
            if (sncaselesscmp(cp, name, ep - cp - 1) == 0) {
                // Step over '='
                for (cp = ep + 1; *cp && isWhite(*cp); cp++) {
                }
                if (*cp) {
                    for (ep = cp + 1; *ep && *ep != ';'; ep++) {
                    }
                    while (ep > cp && isWhite(ep[-1])) ep--;
                    value = snclone(cp, ep - cp);
                    break;
                }
            }
        }
        //  Step over header
        cp += slen(cp) + 1;
        if (cp < end && *cp) {
            //  Step over value
            cp += slen(cp) + 1;
        }
    }
    return value;
}

static ssize getContentLength(cchar *headers, ssize size)
{
    cchar *cp, *end, *header;
    ssize len;

    if (!headers) {
        return -1;
    }
    if (size < 0) {
        size = slen(headers);
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
    while (*line && isWhite(*line)) line++;

    if ((cp = strchr(line, ':')) == 0) {
        return 0;
    }
    *key = line;
    *cp++ = '\0';

    //  Validate header key
    for (tok = *key; *tok; tok++) {
        if (*tok >= sizeof(validHeaderChars) || !validHeaderChars[*tok & 0x7f]) {
            return 0;
        }
    }
    if (*key[0] == '\0' && !(flags & HDR_SSE)) {
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

    if (flags & HDR_HTTP) {
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
    char c, *key, *tok, *value;

    for (tok = rGetBufStart(up->rxHeaders); tok < rGetBufEnd(up->rxHeaders); ) {

        if ((tok = getHeader(tok, &key, &value, HDR_HTTP)) == 0) {
            urlError(up, "Bad header");
            return 0;
        }
        c = tolower(*key);
        if (c == 'c') {
            if (scaselessmatch(key, "content-length")) {
                // SECURITY Acceptable: rxLen is ssize and overflow is checked
                up->rxLen = stoi(value);
                if (up->rxLen < 0 || up->rxLen >= SSIZE_MAX) {
                    urlError(up, "Invalid Content-Length");
                    return 0;
                }
                up->rxRemaining = smatch(up->method, "HEAD") ? 0 : up->rxLen;

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
            up->chunked = URL_CHUNK_START;
        }
    }
    if (up->status == URL_CODE_NO_CONTENT || smatch(up->method, "HEAD")) {
        up->rxRemaining = 0;
    } else if (up->chunked) {
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
    up->deadline = timeout > 0 ? rGetTicks() + timeout : MAXINT64;
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
    cchar *protocol, *hash, *hsep, *query, *qsep, *startHeaders;

    if (!up) {
        return R_ERR_BAD_ARGS;
    }
    if (up->wroteHeaders) {
        return 0;
    }

    protocol = up->protocol ? "HTTP/1.1" : "HTTP/1.0";
    query = up->query ? up->query : "";
    qsep = up->query ? "?" : "";
    hash = up->hash ? up->hash : "";
    hsep = up->hash ? "#" : "";

    buf = up->txHeaders;
    rPutToBuf(buf, "%s /%s%s%s%s%s %s\r\n", up->method, up->path, qsep, query, hsep, hash, protocol);
    if (up->boundary) {
        rPutToBuf(buf, "Content-Type: multipart/form-data; boundary=%s\r\n", &up->boundary[2]);
    }
    startHeaders = rGetBufEnd(buf);
    if (headers) {
        rPutStringToBuf(buf, headers);
    }
    if (!sncaselesscontains(headers, "host:", -1)) {
        if (up->port != 80 && up->port != 443) {
            rPutToBuf(buf, "Host: %s:%d\r\n", up->host, up->port);
        } else {
            rPutToBuf(buf, "Host: %s\r\n", up->host);
        }
    }
#if URL_SSE
    if (up->sse) {
        rPutStringToBuf(buf, "Accept: text/event-stream\r\n");
        if (up->lastEventId >= 0) {
            rPutToBuf(buf, "Last-Event-ID: %ld\r\n", up->lastEventId);
        }
    }
#endif
    if (smatch(up->scheme, "ws") || smatch(up->scheme, "wss")) {
        webSocketFree(up->webSocket);
        if ((up->webSocket = webSocketAlloc(up->sock, WS_CLIENT)) == 0) {
            rFatal("sockets", "memory error");
            return R_ERR_MEMORY;
        }
        if (writeWebSocketHeaders(up, buf) < 0) {
            return urlError(up, "Cannot upgrade WebSocket");
        }
    }
    /*
        See if the caller has specified a content length and intelligently handle chunked encoding.
     */
    up->txLen = getContentLength(rBufToString(buf), rGetBufLength(buf));

    if (up->txLen < 0 && scaselessmatch(up->method, "GET") && !sncaselesscontains(headers, "Transfer-Encoding", -1)) {
        up->txLen = 0;
    }
    if (up->txLen < 0 && !up->boundary) {
        rPutToBuf(buf, "Transfer-Encoding: chunked\r\n");
    }
    if (up->txLen >= 0 || up->boundary) {
        /*
            If using transfer encoding and not upload, defer adding blank line till writeChunk().
            Saves one write per chunk.
         */
        rPutStringToBuf(buf, "\r\n");
    }
    if (up->flags & URL_SHOW_REQ_HEADERS) {
        rLog("raw", "url", "%s", rBufToString(buf));
    }
    if (rWriteSocket(up->sock, rBufToString(buf), rGetBufLength(buf), up->deadline) != rGetBufLength(buf)) {
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
    /*
       if (up->status != URL_CODE_OK) {
        up->error = sfmt("Error %d", up->status);
        return up->error;
       } */
    return NULL;
}

PUBLIC void urlSetProtocol(Url *up, int protocol)
{
    up->protocol = protocol;
    up->close = protocol == 0 ? 1 : 0;
}

static bool isprintable(cchar *s, ssize len)
{
    cchar *cp;
    int   c;

    for (cp = s; *cp && cp < &s[len]; cp++) {
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
        rFree(up->boundary);
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
                urlWriteFmt(up, "Content-Type: application/x-www-form-urlencoded\r\n\r\n%s\r\n", field->value) < 0) {
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
                            up->boundary, next - 1, name) < 0) {
                return urlError(up, "Cannot write to socket");
            }
            urlWrite(up, "\r\n", -1);
            if (urlWriteFile(up, path) < 0) {
                return urlError(up, "Cannot write file to socket");
            }
            urlWrite(up, "\r\n", -1);
        }
    }
    if (urlWriteFmt(up, "%s--\r\n", up->boundary) < 0) {
        return urlError(up, "Cannot write to socket");
    }
    return (int) urlFinalize(up);
}

#if ME_COM_WEBSOCKETS

PUBLIC void urlWebSocketAsync(Url *up, WebSocketProc callback, void *arg)
{
    if (!up) {
        return;
    }
    if (up->webSocket) {
        webSocketAsync(up->webSocket, callback, arg, up->rx);
    }
}

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
        if (urlFinalize(up) < 0) {
            return urlError(up, "Cannot finalize request");
        }
        // Setup callback and invoke connected event
        urlWebSocketAsync(up, callback, arg);

        if (urlWait(up) < 0) {
            rDebug("test", "WebSocket error");
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
static ssize writeWebSocketHeaders(Url *up, RBuf *buf)
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
    sjoinbuf(keybuf, sizeof(keybuf), webSocketGetClientKey(ws), WS_MAGIC);
    expected = cryptGetSha1Base64(keybuf, -1);
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
#endif /* ME_COM_WEBSOCKETS */

#if URL_SSE
PUBLIC void urlSetMaxRetries(Url *up, int maxRetries)
{
    if (!up) {
        return;
    }
    up->maxRetries = maxRetries;
}

static void invokeCallback(Url *up, ssize id, cchar *event, cchar *data, void *arg)
{
    if (up->sseProc) {
        up->inCallback = 1;
        up->sseProc(up, id, event, data, up->sseArg);
        up->inCallback = 0;
        if (up->needFree) {
            urlFree(up);
        }
    }
}

/**
    Read the response from the socket and retry connection if necessary
    This will continue to try to reconnect until the user calls urlFree or urlClose
    in their callback.
 */
PUBLIC void sseCallback(Url *up)
{
    RBuf  *buf;
    ssize nbytes;
    char  *headers, *url;
    int   status;

    buf = up->responseBuf;
    if (!buf) {
        buf = up->responseBuf = rAllocBuf(ME_BUFSIZE);
    }
    while (!up->error && !up->needFree && up->sock) {
        //  Parse buffered SSE events first before reading
        parseEvents(up);
        if (up->nonblock) {
            break;
        }

        /*
            Check if an event is too big to handle.
         */
        rCompactBuf(buf);
        rReserveBufSpace(buf, ME_BUFSIZE);
        if (rGetBufLength(buf) > up->bufLimit) {
            urlError(up, "Response too big");
            break;
        }
        /*
            Read the response from the socket and retry connection if necessary
         */
        if ((nbytes = urlRead(up, buf->end, rGetBufSpace(buf))) > 0) {
            rAdjustBufEnd(buf, nbytes);
        } else {
            //  Normal end of stream or error. Reconnect if retries are allowed.
            if (++up->retries > up->maxRetries) {
                if (up->maxRetries) {
                    urlError(up, "Too many SSE retries");
                }
                // up->rxRemaining is set to 0 when the request completes
                break;
            }
            headers = sclone(rBufToString(up->txHeaders));
            rFlushBuf(up->txHeaders);
            url = sclone(up->url);

            status = fetch(up, "GET", url, 0, 0, headers);
            rFree(url);
            rFree(headers);

            if (status != URL_CODE_OK) {
                urlError(up, "Cannot retry requeset");
                break;
            }
        }
    }
    if (up->fiber) {
        rSetWaitHandler(up->sock->wait, 0, 0, 0, 0);
        rResumeFiber(up->fiber, 0);
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
         (end = sncontains(cp, "\n\n", rGetBufLength(buf))) != NULL; ) {

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
            if (*key == '\0') {
                break;
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
            } else {
                urlError(up, "Bad header");
                break;
            }
        }
        if (data) {
            if (up->sseProc) {
                invokeCallback(up, id, event, data, up->sseArg);
                if (up->needFree || !up->sock) {
                    break;
                }
            }
        } else {
            urlError(up, "No data in SSE event");
            break;
        }
        rAdjustBufStart(buf, end - start + 2);
        rFlushBuf(dataBuf);
        cp = rGetBufStart(buf);
    }
    rFreeBuf(dataBuf);
}

PUBLIC void urlSseAsync(Url *up, UrlSseProc proc, void *arg)
{
    if (!up) {
        return;
    }
    up->sseProc = proc;
    up->sseArg = arg;
    up->sse = 1;
    up->maxRetries = URL_MAX_RETRIES;
    up->retries = 0;

    if (rGetBufLength(up->rx) > 0) {
        up->nonblock = 1;
        sseCallback(up);
        up->nonblock = 0;
        if (up->rxRemaining == 0) {
            //  EOF
            return;
        }
    }
    rSetWaitHandler(up->sock->wait, (RWaitProc) sseCallback, up, R_READABLE, up->deadline);
}

/*
    Timeout waiting for SSE events
 */
static void abortWait(Url *up, int mask)
{
    rSetWaitHandler(up->sock->wait, 0, 0, 0, 0);
    if (up->fiber) {
        urlError(up, "Timeout waiting for WebSocket events");
        rResumeFiber(up->fiber, 0);
    }
}

PUBLIC int urlSseWait(Url *up)
{
    if (!up) {
        return R_ERR_BAD_ARGS;
    }
    if (up->rxRemaining) {
        if (rIsMain()) {
            rServiceEvents();
            while (rState < R_STOPPING && !up->error) {
                rWait(rRunEvents());
                if (rGetTicks() > up->deadline) {
                    urlError(up, "Timeout waiting for SSE events");
                    break;
                }
            }
        } else {
            up->fiber = rGetFiber();
            up->abortEvent = rStartEvent((REventProc) abortWait, up, up->deadline);
            rYieldFiber(0);
            rStopEvent(up->abortEvent);
            up->abortEvent = 0;
            up->fiber = 0;
        }
    }
    return up->error ? R_ERR_NOT_CONNECTED : 0;
}

PUBLIC int urlGetEvents(cchar *uri, UrlSseProc proc, void *arg, char *headers, ...)
{
    va_list args;
    Url     *up;

    if (!uri || !proc) {
        return R_ERR_BAD_ARGS;
    }
    va_start(args, headers);
    headers = sfmtv(headers, args);
    va_end(args);

    if ((up = urlAlloc(0)) == 0) {
        return R_ERR_MEMORY;
    }
    if (fetch(up, "GET", uri, 0, 0, headers) != URL_CODE_OK) {
        rFree(headers);
        urlFree(up);
        return R_ERR_CANT_COMPLETE;
    }
    urlSseAsync(up, proc, arg);
    if (urlWait(up) < 0) {
        rDebug("test", "WebSocket error");
    }
    urlFree(up);
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

#if ME_COM_WEBSOCKETS || URL_SSE
PUBLIC int urlWait(Url *up)
{
    if (!up) {
        return R_ERR_BAD_ARGS;
    }
#if ME_COM_WEBSOCKETS
    if (up->webSocket) {
        return webSocketWait(up->webSocket, up->deadline);
#endif
#if URL_SSE
} else if (up->sse) {
    return urlSseWait(up);
#endif
}
return 0;
}
#endif /* ME_COM_WEBSOCKETS || URL_SSE */

#else
void dummyUrl(void)
{
}
#endif /* ME_COM_URL */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

#else
void dummyUrl(){}
#endif /* ME_COM_URL */
