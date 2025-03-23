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

#define URL_CHUNK_START      1          /**< Start of a new chunk */
#define URL_CHUNK_DATA       2          /**< Start of chunk data */
#define URL_CHUNK_EOF        4          /**< End of all chunk data */

#ifndef ME_R_URL_TIMEOUT
    #define ME_R_URL_TIMEOUT 0          /**< Default timeout (none) */
#endif

#define URL_BUFSIZE          4096
#define URL_MAX_RESPONSE     (1 * 1024 * 1024 * 1024)
#define URL_UNLIMITED        MAXINT

static Ticks timeout = ME_R_URL_TIMEOUT;

/*********************************** Forwards *********************************/

static int connectHost(Url *up);
static int fetch(Url *up, cchar *method, cchar *uri, cvoid *data, ssize len, cchar *headers);
static ssize getContentLength(cchar *headers, ssize length);
static int startRequest(Url *up);
static bool parseHeaders(Url *up);
static int parseResponse(Url *up, ssize size);
static ssize readChunk(Url *up, char *buf, ssize bufsize);
static int readHeaders(Url *up);
static ssize readBlock(Url *up, char *buf, ssize bufsize);
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
    up->timeout = timeout;
    up->deadline = MAXINT64;

    if (flags == 0) {
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
    if (up) {
        rFreeSocket(up->sock);
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
#endif
        memset(up, 0, sizeof(Url));
        rFree(up);
    }
}

static void resetState(Url *up)
{
    resetSocket(up);
    up->boundary = 0;
    up->chunked = 0;
    up->finalized = 0;
    up->gotResponse = 0;
    up->redirect = 0;
    up->rxLen = -1;
    up->rxRemaining = URL_UNLIMITED;
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
    up->flags = flags;
}

PUBLIC void urlClose(Url *up)
{
    if (up) {
        rFreeSocket(up->sock);
        up->sock = 0;
    }
}

PUBLIC int urlStart(Url *up, cchar *method, cchar *url)
{
    if (!up->responseBuf) {
        up->responseBuf = rAllocBuf(ME_BUFSIZE);
    } else {
        rFlushBuf(up->responseBuf);
    }
    rFree(up->method);
    up->method = supper(sclone(method));
    up->url = sclone(url);
    return startRequest(up);
}

static int startRequest(Url *up)
{
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

    if (urlParse(up, up->url) < 0) {
        urlError(up, "Bad URL: %s", up);
        return R_ERR_BAD_ARGS;
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
            rLog("raw", "url", "\n%s %s:%d\n", up->sock->tls ? "HTTPS" : "HTTP", up->host, up->port);
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
        if (urlWrite(up, data, len) >= 0) {
            return up->status;
        }
        if (urlFinalize(up) < 0) {
            return urlError(up, "Cannot finalize");
        }
    }
    if (!up->error) {
        urlError(up, "Cannot run \"%s\" %s", method, uri);
    }
    return R_ERR_CANT_CONNECT;
}

/*
    Finalize the normal request body and reads the response line and response headers.
 */
PUBLIC int urlFinalize(Url *up)
{
    if (urlWrite(up, 0, 0) < 0) {
        return urlError(up, "Cannot finalize");
    }
    /*
        Verify the WebSocket handshake if required
     */
    if (up->webSocket && verifyWebSocket(up) < 0) {
        return urlError(up, "Cannot verify WebSockets connection");
    }
    return up->error ? R_ERR_CANT_WRITE: 0;
}

PUBLIC char *urlGet(cchar *uri, cchar *headersFmt, ...)
{
    va_list args;
    Url     *up;
    cchar   *response;
    char    *headers, *result;

    va_start(args, headersFmt);
    headers = headersFmt ? sfmtv(headersFmt, args) : 0;
    va_end(args);

    up = urlAlloc(0);
    if (fetch(up, "GET", uri, 0, 0, headers) != 200) {
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

    va_start(args, headersFmt);
    headers = headersFmt ? sfmtv(headersFmt, args) : 0;
    va_end(args);

    up = urlAlloc(0);
    if (fetch(up, "POST", uri, data, len, headers) != 200) {
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
            rError("url", "Cannot fetch %s. Error: %s", uri, urlGetError(up));
        } else {
            rError("url", "Cannot fetch %s. Bad status %d", uri, up->status);
        }
        rError("url", "%s", urlGetResponse(up));
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
    if (up->timeout) {
        up->deadline = up->timeout ? rGetTicks() + up->timeout : 0;
    } else {
        up->deadline = MAXINT64;
    }
}

/*
    Write body data. Set bufsize to zero to signify end of body if the content length is not defined.
    If finalizing, read the response headers after the last write packet.
    This routine will handle chunking transparently and will trace request data if required.
    May close the socket if required.
 */
PUBLIC ssize urlWrite(Url *up, cvoid *buf, ssize bufsize)
{
    if (up->finalized) {
        return 0;
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
        up->finalized = 1;
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
    Read data into the supplied buffer up to bufsize. After reading the headers, the headers
    are copied to the rxHeaders. User body data is read through the rx buffer into the
    user supplied buffer. rxRemaining is the amount of remaining data for this request that must
    be read into the low level rx buffer. If chunked, rxRemaining may be set to unlimited before
    reading a chunk and the chunk length.
 */
PUBLIC ssize urlRead(Url *up, char *buf, ssize bufsize)
{
    ssize nbytes;

    if (up->gotResponse) {
        return urlError(up, "Should not call urlRead after urlGetResponse");
    }
    if (!up->rxHeaders && readHeaders(up) < 0) {
        //  Already closed
        return R_ERR_CANT_READ;
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
    Read a chunked transfer segment.
 */
static ssize readChunk(Url *up, char *buf, ssize bufsize)
{
    ssize chunkSize, nbytes;
    char  cbuf[32];

    nbytes = 0;

    if (up->chunked == URL_CHUNK_START) {
        if (readUntil(up, "\r\n", cbuf, sizeof(cbuf)) < 0) {
            return urlError(up, "Bad chunk data");
        }
        cbuf[sizeof(cbuf) - 1] = '\0';
        chunkSize = (int) stoiradix(cbuf, 16, NULL);
        if (chunkSize < 0) {
            return urlError(up, "Bad chunk specification");

        } else if (chunkSize) {
            //  Set rxRemaining to the next chunk size
            up->rxRemaining = chunkSize;
            up->chunked = URL_CHUNK_DATA;

        } else {
            //  End of body so consume the trailing <CR><NL>
            if (readUntil(up, "\r\n", cbuf, sizeof(cbuf)) < 0) {
                return urlError(up, "Bad chunk data");
            }
            up->rxRemaining = 0;
            up->chunked = URL_CHUNK_EOF;
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
    if (!up->sock) {
        up->sock = rAllocSocket();
    }
    rSetSocketCerts(up->sock, ca, key, cert, revoke);
    up->certsDefined = 1;
}

PUBLIC void urlSetCiphers(Url *up, cchar *ciphers)
{
    if (!up->sock) {
        up->sock = rAllocSocket();
    }
    rSetSocketCiphers(up->sock, ciphers);
}

PUBLIC void urlSetVerify(Url *up, int verifyPeer, int verifyIssuer)
{
    if (!up->sock) {
        up->sock = rAllocSocket();
    }
    rSetSocketVerify(up->sock, verifyPeer, verifyIssuer);
}

PUBLIC int urlParse(Url *up, cchar *uri)
{
    char *next, *tok;
    bool hasScheme;

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
        up->scheme = tok;
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
                up->port = atoi(tok);
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
        up->port = atoi(tok);
        if ((tok = schr(tok, '/')) != 0) {
            tok++;
        } else {
            tok = "";
        }
    }
    if (*tok) {
        up->path = tok;
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

    if (urlFinalize(up) < 0) {
        urlError(up, "Cannot finalize request");
        return NULL;
    }

    buf = up->responseBuf;
    contentLength = urlGetHeader(up, "Content-Length");
    len = contentLength ? stoi(contentLength) : -1;
    if (!up->gotResponse && len != 0) {
        do {
            nbytes = contentLength ? len : min(buf->buflen * 2, ME_BUFSIZE * 1024);
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
    return buf;
}

PUBLIC cchar *urlGetResponse(Url *up)
{
    RBuf *buf;

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

    if ((json = jsonParseString(urlGetResponse(up), &errorMsg, 0)) == 0) {
        urlError(up, "Cannot parse json. %s", errorMsg);
        rFree(errorMsg);
        return 0;
    }
    return json;
}

PUBLIC int urlGetStatus(Url *up)
{
    if (urlFinalize(up) < 0) {
        return urlError(up, "Cannot finalize request");
    }
    return up->status;
}

/*
    Headers have been tokenized with a null replacing the ":" and "\r\n"
 */
PUBLIC cchar *urlGetHeader(Url *up, cchar *header)
{
    cchar *cp, *end, *start, *value;

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
            return atoi(cp);
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
    Parse the headers in-situ. The headers string is modified by tokenizing with '\0'.
 */
static bool parseHeaders(Url *up)
{
    char c, *cp, *key, *value;

    for (cp = rGetBufStart(up->rxHeaders); cp < rGetBufEnd(up->rxHeaders); ) {
        key = cp;
        if ((cp = strchr(cp, ':')) == 0) {
            urlError(up, "Bad headers");
            return 0;
        }
        *cp++ = '\0';
        while (*cp && *cp == ' ') cp++;
        value = cp;
        while (*cp && *cp != '\r') cp++;

        if (*cp != '\r') {
            urlError(up, "Bad headers");
            return 0;
        }
        *cp++ = '\0';
        if (*cp != '\n') {
            urlError(up, "Bad headers");
            return 0;
        }
        *cp++ = '\0';

        c = tolower(*key);
        if (c == 'c') {
            if (scaselessmatch(key, "content-length")) {
                up->rxLen = atoi(value);
                up->rxRemaining = smatch(up->method, "HEAD") ? 0 : up->rxLen;

            } else if (scaselessmatch(key, "connection")) {
                if (scaselessmatch(value, "close")) {
                    up->close = 1;
                }
            }
        } else if (c == 'l' && scaselessmatch(key, "location")) {
            up->redirect = value;
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
    up->timeout = timeout;
    up->deadline = timeout > 0 ? rGetTicks() + timeout : MAXINT64;
}

PUBLIC void urlSetDefaultTimeout(Ticks value)
{
    timeout = value;
}

static int writeRequestLine(Url *up)
{
    cchar *protocol, *hash, *hsep, *query, *qsep;

    protocol = up->protocol ? "HTTP/1.1" : "HTTP/1.0";
    query = up->query ? up->query : "";
    qsep = up->query ? "?" : "";
    hash = up->hash ? up->hash : "";
    hsep = up->hash ? "#" : "";

    rPutToBuf(up->txHeaders, "%s /%s%s%s%s%s %s\r\n", up->method, up->path, qsep, query, hsep, hash, protocol);
    if (up->boundary) {
        rPutToBuf(up->txHeaders, "Content-Type: multipart/form-data; boundary=%s\r\n", &up->boundary[2]);
    }
    return 0;
}

/*
    Write HTTP request headers. Can be called multiple times.
 */
PUBLIC int urlWriteHeaders(Url *up, cchar *headers)
{
    RBuf  *buf;

    if (up->wroteHeaders) {
        return 0;
    }
    if (writeRequestLine(up) < 0) {
        return R_ERR_CANT_WRITE;
    }
    buf = up->txHeaders;
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
    up->wroteHeaders = 1;
    return 0;
}

PUBLIC void urlSetStatus(Url *up, int status)
{
    up->status = status;
}

PUBLIC cchar *urlGetError(Url *up)
{
    if (up->error) {
        return up->error;
    }
    return "";
}

PUBLIC void urlSetProtocol(Url *up, int protocol)
{
    up->protocol = protocol;
    up->close = protocol == 0 ? 1 : 0;
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
            if (!rFileExists(path)) {
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
/*
    Return zero when closed and a negative error code on errors.
 */
PUBLIC int urlWebSocket(cchar *uri, WebSocketProc callback, void *arg, cchar *headers)
{
    Url     *up;
    int     rc;

    rc = 0;
    up = urlAlloc(0);

    if (urlStart(up, "GET", uri) == 0) {
        if (urlWriteHeaders(up, headers) < 0) {
            return urlError(up, "Cannot write headers");
        }
        // Read response headers
        if (urlFinalize(up) < 0) {
            return urlError(up, "Cannot finalize request");
        }
        // Setup callback and invoke connected event
        urlAsync(up, callback, arg);
        
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

    cryptGetRandomBytes(bytes, sizeof(bytes), 0);
    key = cryptEncode64Block(bytes, sizeof(bytes));
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
        urlError(up, "Bad WebSocket handshake key\n%s\n%s", key, expected);
        rFree(expected);
        return R_ERR_BAD_STATE;
    }
    rFree(expected);
    return 0;
}

PUBLIC void urlAsync(Url *up, WebSocketProc callback, void *arg)
{
    webSocketAsync(up->webSocket, callback, arg);
}

PUBLIC int urlWait(Url *up)
{
    return webSocketWait(up->webSocket, up->deadline);
}

PUBLIC WebSocket *urlGetWebSocket(Url *up)
{
    return up->webSocket;
}
#endif

/*
    Sets up->error if not already set, traces the result and CLOSES the socket.
 */
PUBLIC int urlError(Url *up, cchar *fmt, ...)
{
    va_list ap;

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

#else
void dummyUrl()
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
