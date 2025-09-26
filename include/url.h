
/**
    @file url.h
    HTTP client library for embedded IoT applications.
    @description The URL module provides a lightweight, streaming HTTP client optimized for embedded IoT devices.
        Supports HTTP/HTTPS, WebSockets, and Server-Sent Events (SSE) with fiber-based concurrency.
        Uses the R runtime for memory management and cross-platform compatibility.
    @stability Evolving

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#pragma once

#ifndef _h_URL
#define _h_URL 1

/********************************** Includes **********************************/

#include "me.h"
#include "r.h"
#include "json.h"

#if ME_COM_WEBSOCKETS
#include "websockets.h"
#endif

/*********************************** Defines **********************************/
#if ME_COM_URL

#ifndef URL_SSE
    #define URL_SSE 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct Url;

/************************************ Url *************************************/
/*
    Standard HTTP/1.1 status codes
 */
#define URL_CODE_CONTINUE               100  /**< Continue with request, only parial content transmitted */
#define URL_CODE_SWITCHING              101  /**< Switching protocols */
#define URL_CODE_OK                     200  /**< The request completed successfully */
#define URL_CODE_CREATED                201  /**< The request has completed and a new resource was created */
#define URL_CODE_ACCEPTED               202  /**< The request has been accepted and processing is continuing */
#define URL_CODE_NOT_AUTHORITATIVE      203  /**< The request has completed but content may be from another source */
#define URL_CODE_NO_CONTENT             204  /**< The request has completed and there is no response to send */
#define URL_CODE_RESET                  205  /**< The request has completed with no content. Client must reset view */
#define URL_CODE_PARTIAL                206  /**< The request has completed and is returning parial content */
#define URL_CODE_MOVED_PERMANENTLY      301  /**< The requested URI has moved permanently to a new location */
#define URL_CODE_MOVED_TEMPORARILY      302  /**< The URI has moved temporarily to a new location */
#define URL_CODE_SEE_OTHER              303  /**< The requested URI can be found at another URI location */
#define URL_CODE_NOT_MODIFIED           304  /**< The requested resource has changed since the last request */
#define URL_CODE_USE_PROXY              305  /**< The requested resource must be accessed via the location proxy */
#define URL_CODE_TEMPORARY_REDIRECT     307  /**< The request should be repeated at another URI location */
#define URL_CODE_PERMANENT_REDIRECT     308  /**< The request has been permanently redirected to a new location */
#define URL_CODE_BAD_REQUEST            400  /**< The request is malformed */
#define URL_CODE_UNAUTHORIZED           401  /**< Authentication for the request has failed */
#define URL_CODE_PAYMENT_REQUIRED       402  /**< Reserved for future use */
#define URL_CODE_FORBIDDEN              403  /**< The request was legal, but the server refuses to process */
#define URL_CODE_NOT_FOUND              404  /**< The requested resource was not found */
#define URL_CODE_BAD_METHOD             405  /**< The request HTTP method was not supported by the resource */
#define URL_CODE_NOT_ACCEPTABLE         406  /**< The requested resource cannot generate the required content */
#define URL_CODE_REQUEST_TIMEOUT        408  /**< The server timed out waiting for the request to complete */
#define URL_CODE_CONFLICT               409  /**< The request had a conflict in the request headers and URI */
#define URL_CODE_GONE                   410  /**< The requested resource is no longer available*/
#define URL_CODE_LENGTH_REQUIRED        411  /**< The request did not specify a required content length*/
#define URL_CODE_PRECOND_FAILED         412  /**< The server cannot satisfy one of the request preconditions */
#define URL_CODE_REQUEST_TOO_LARGE      413  /**< The request is too large for the server to process */
#define URL_CODE_REQUEST_URL_TOO_LARGE  414  /**< The request URI is too long for the server to process */
#define URL_CODE_UNSUPPORTED_MEDIA_TYPE 415  /**< The request media type is not supported by the server or resource */
#define URL_CODE_RANGE_NOT_SATISFIABLE  416  /**< The request content range does not exist for the resource */
#define URL_CODE_EXPECTATION_FAILED     417  /**< The server cannot satisfy the Expect header requirements */
#define URL_CODE_IM_A_TEAPOT            418  /**< Short and stout error code (RFC 2324) */
#define URL_CODE_NO_RESPONSE            444  /**< The connection was closed with no response to the client */
#define URL_CODE_INTERNAL_SERVER_ERROR  500  /**< Server processing or configuration error. No response generated */
#define URL_CODE_NOT_IMPLEMENTED        501  /**< The server does not recognize the request or method */
#define URL_CODE_BAD_GATEWAY            502  /**< The server cannot act as a gateway for the given request */
#define URL_CODE_SERVICE_UNAVAILABLE    503  /**< The server is currently unavailable or overloaded */
#define URL_CODE_GATEWAY_TIMEOUT        504  /**< The server gateway timed out waiting for the upstream server */
#define URL_CODE_BAD_VERSION            505  /**< The server does not support the HTTP protocol version */
#define URL_CODE_INSUFFICIENT_STORAGE   507  /**< The server has insufficient storage to complete the request */

/*
    Alloc flags
 */
#define URL_SHOW_NONE                   0x1  /**< Trace nothing */
#define URL_SHOW_REQ_BODY               0x2  /**< Trace request body */
#define URL_SHOW_REQ_HEADERS            0x4  /**< Trace request headers */
#define URL_SHOW_RESP_BODY              0x8  /**< Trace response body */
#define URL_SHOW_RESP_HEADERS           0x10 /**< Trace response headers */
#define URL_HTTP_0                      0x20 /**< Use HTTP/1.0 */

#if URL_SSE
/**
    URL SSE callback
    @param up URL object
    @param id Event ID
    @param event Type of event.
    @param data Associated data
    @param arg User argument
    @stability Evolving
 */
typedef void (*UrlSseProc)(struct Url *up, ssize id, cchar *event, cchar *data, void *arg);
#endif

/**
    Url request object
    @description The Url service is a streaming HTTP request client. This service requires the use of fibers from the
        portable runtime.
    @stability Evolving
 */
typedef struct Url {
    int status;                    /**< Response (rx) status */
    uint chunked : 4;              /**< Request is using transfer chunk encoding */
    uint close : 1;                /**< Connection should be closed on completion of the current request */
    uint certsDefined : 1;         /**< Certificates have been defined */
    uint finalized : 1;            /**< The request body has been fully written */
    uint gotResponse : 1;          /**< Response has been read */
    uint inCallback : 1;           /**< SSE callback is in progress */
    uint needFree : 1;             /**< Free the URL object */
    uint nonblock : 1;             /**< Don't block in SSE callback */
    uint protocol : 2;             /**< Use HTTP/1.0 without keep-alive. Defaults to HTTP/1.1. */
    uint sse : 1;                  /**< SSE request */
    uint upgraded : 1;             /**< WebSocket upgrade has been completed */
    uint wroteHeaders : 1;         /**< Tx headers have been written */

    uint flags;                    /**< Alloc flags */

    char *url;                     /**< Request URL*/
    char *urlbuf;                  /**< Parsed and tokenized URL*/
    char *method;                  /** HTTP request method */
    char *boundary;                /**< Multipart mime upload file boundary */

    ssize txLen;                   /**< Length of tx body */
    RBuf *rx;                      /**< Buffer for progressive reading response data */
    char *response;                /**< Response as a string */
    RBuf *responseBuf;             /**< Buffer to hold complete response */

    // SECURITY Acceptable: Important to note that rxLen is ssize and overflow is checked
    ssize rxLen;                   /**< Length of rx body */
    ssize rxRemaining;             /**< Remaining rx data to read from the socket */
    ssize bufLimit;                /**< Maximum number of bytes to buffer from the response */

    RBuf *rxHeaders;               /**< Buffer for Rx headers */
    RBuf *txHeaders;               /**< Buffer for Tx headers */

    char *error;                   /**< Error message for internal errors, not HTTP errors */
    cchar *host;                   /**< Request host */
    cchar *path;                   /**< Request path without leading "/" and query/ref */
    cchar *query;                  /**< Request query portion */
    cchar *hash;                   /**< Request hash portion */
    cchar *scheme;                 /**< Request scheme */
    cchar *redirect;               /**< Redirect location */
    int port;                      /**< Request port */

    RSocket *sock;                 /**< Network socket */
    Ticks deadline;                /**< Request time limit expiry deadline */
    Ticks timeout;                 /**< Request timeout */

    RFiber *fiber;                 /**< Fiber */
    REvent abortEvent;             /**< Abort event */
    void *sseArg;                  /**< SSE callback argument */

#if ME_COM_WEBSOCKETS
    WebSocket *webSocket;          /**< WebSocket object */
#endif
#if URL_SSE
    UrlSseProc sseProc;            /**< SSE callback */
    uint retries;                  /**< Number of retries for SSE */
    uint maxRetries;               /**< Maximum of retries for SSE */
    ssize lastEventId;             /**< Last event ID (SSE) */
#endif
} Url;

/**
    Allocate a URL object
    @description A URL object represents a network connection on which one or more HTTP client requests may be
        issued one at a time.
    @param flags Set flags to URL_SHOW_REQ_HEADERS | URL_SHOW_REQ_BODY | URL_SHOW_RESP_HEADERS | URL_SHOW_RESP_BODY.
        Defaults to 0. Can also call urlSetFlags to set flags.
    @return The url object
    @stability Evolving
 */
PUBLIC Url *urlAlloc(int flags);

/**
    Close the underlying network socket associated with the URL object.
    @description This is not normally necessary to invoke unless you want to force the socket
        connection to be recreated before issuing a subsequent request. Does not free the URL object.
    @param up URL object to close. Function is null tolerant.
    @stability Evolving
 */
PUBLIC void urlClose(Url *up);

/**
    Set the URL internal error message
    @description This routine is used to set the URL internal error message. Use $urlGetError to get the error message.
    @param up URL object
    @param message Printf style message format string
    @param ... Message arguments
    @return Zero if successful, otherwise a negative status code.
    @stability Evolving
 */
PUBLIC int urlError(Url *up, cchar *message, ...);

/**
    Fetch a URL
    @description This routine issues a HTTP request with optional body data and returns the HTTP status
    code. This routine will return after the response status and headers have been read and before the response body
    data has been read. Use $urlRead or $urlGetResponse to read the response body data.
    This routine will block the current fiber while waiting for the request to respond. Other fibers continue to run.
    @param up URL object.
    @param method HTTP method verb.
    @param url HTTP URL to fetch
    @param data Body data for request. Set to NULL if none.
    @param size Size of body data for request. Set to 0 if none.
    @param headers Optional request headers. This parameter is a printf style formatted pattern with following
        arguments. Individual header lines must be terminated with "\r\n".
    @param ... Optional header arguments.
    @return Response HTTP status code. Use urlGetResponse or urlRead to read the response.
    @stability Evolving
 */
PUBLIC int urlFetch(Url *up, cchar *method, cchar *url, cvoid *data, ssize size, cchar *headers, ...);

/**
    Fetch a URL and return a JSON response if the HTTP request is successful.
    @description This routine issues a HTTP request and reads and parses the response into a JSON object tree.
        This routine will block the current fiber while waiting for the request to complete. Other fibers continue to
           run.
    @param up URL object.
    @param method HTTP method verb.
    @param url HTTP URL to fetch
    @param data Body data for request. Set to NULL if none.
    @param size Size of body data for request. Set to 0 if none.
    @param headers Optional request headers. This parameter is a printf style formatted pattern with following
        arguments. Individual header lines must be terminated with "\r\n". If the headers are not provided, a headers
           value of "Content-Type: application/json\r\n" is used.
    @param ... Optional header arguments.
    @return Parsed JSON response. If the request does not return a HTTP 200 status code or the response is not JSON,
        the request returns NULL. Use urlGetError to get any error string and urlGetStatus to get the HTTP status code.
        Caller must free via jsonFree().
    @stability Evolving
 */
PUBLIC Json *urlJson(Url *up, cchar *method, cchar *url, cvoid *data, ssize size, cchar *headers, ...);

/**
    Finalize the request.
    @description This routine finalizes the request and signifies that the entire request including any
    request body data has been sent to the server. This routine MUST be called at the end of writing
    the request body (if any). It will read the response status and headers before returning.
    If using WebSockets, this should be called before calling urlWebSocketAsync to verify the WebSocket handshake.
    Internally, this function is implemented by calling urlWrite with a NULL buffer. This call is idempotent.
    @param up Url object
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int urlFinalize(Url *up);

/**
    Free a URL object and all resources.
    @description Releases all memory and closes any network connections associated with the URL object.
        After calling this function, the URL object is invalid and must not be used.
    @param up URL object to free. Function is null tolerant.
    @stability Evolving
 */
PUBLIC void urlFree(Url *up);

/**
    Get a URL using a HTTP GET request.
    @description This will issue a HTTP GET request to the specified URL.
        If the HTTP status is 200, this will return the response body.
        This routine will block the current fiber while waiting for the request to complete.
        Other fibers continue to run.
    @param url HTTP URL to fetch
    @param headers Optional request headers. This parameter is a printf style formatted pattern with following
        arguments. Individual header lines must be terminated with "\r\n".
    @param ... Optional header arguments.
    @return Response body if successful, otherwise null. Caller must free.
    @stability Evolving
 */
PUBLIC char *urlGet(cchar *url, cchar *headers, ...);

/**
    Get a cookie from the response headers.
    @description Extracts a specific cookie value from the HTTP response headers.
        The request must have been completed and response headers received.
    @param up URL object containing the response headers
    @param name Cookie name to retrieve
    @return The cookie value. Returns NULL if not found. Caller must free the returned string.
    @stability Prototype
 */
PUBLIC char *urlGetCookie(Url *up, cchar *name);

/**
    Get the URL internal error message
    @description Errors are defined for unexpected errors. HTTP requests that respond with a non-200 status
        do not set the error message.
    @param up URL object
    @return The URL error message for the most recent request. Returns NULL if no error message defined.
        Caller must NOT free this message.
    @stability Evolving
 */
PUBLIC cchar *urlGetError(Url *up);

/**
    Get a response HTTP header from the parsed response headers.
    @param up URL object
    @param header HTTP header name. This can be any case. For example: "Authorization".
    @return The value of the HTTP header. Returns NULL if not defined. Caller must NOT free the returned string.
    @stability Evolving
 */
PUBLIC cchar *urlGetHeader(Url *up, cchar *header);

/**
    Issue a HTTP GET request and return parsed JSON.
    @description This will issue a HTTP GET request to the specified URL and if successful, will
        return the parsed response body.
    @param url HTTP URL to fetch
    @param headers Optional request headers. This parameter is a printf style formatted pattern with following
        arguments. Individual header lines must be terminated with "\r\n".
    @param ... Optional header arguments.
    @return Parsed JSON response. If the request does not return a HTTP 200 status code or the response
        is not JSON, the request returns NULL. Caller must free via jsonFree().
    @stability Evolving
 */
PUBLIC Json *urlGetJson(cchar *url, cchar *headers, ...);

/**
    Get the response to a URL request as a JSON object tree.
    @description After issuing urlFetch, urlGet or urlPost, this routine may be called to read and parse the response
    as a JSON object. This call should only be used when the response is a valid JSON UTF-8 string.
        This routine buffers the entire response body and creates the parsed JSON tree.
        \n\n
        This routine will block the current fiber while waiting for the request to complete.
        Other fibers continue to run.
    @param up URL object
    @return The response body as parsed JSON object. Caller must free the result via jsonFree.
    @stability Evolving
 */
PUBLIC Json *urlGetJsonResponse(Url *up);

/**
    Get the response to a URL request as a string.
    @description After issuing urlFetch, urlGet or urlPost, this routine may be called to read, buffer and return
        the response body. This call should only be used when the response is a valid UTF-8 string. Otherwise, use
        urlRead to read the response body. As this routine buffers the entire response body, it should only be used for
        relatively small requests. Otherwise, the memory footprint of the application may be larger than desired.
        \n\n
        This routine will block the current fiber while waiting for the request to complete.
        Other fibers continue to run.
        \n\n
        If receiving a binary response, use urlGetResponseBuf instead.
    @param up URL object containing the response data
    @return The response body as a null-terminated string. Caller must NOT free. Will return an empty string on errors.
    @stability Evolving
 */
PUBLIC cchar *urlGetResponse(Url *up);

/**
    Get the response to a URL request in a buffer.
    @description After issuing urlFetch, urlGet or urlPost, this routine may be called to read, buffer and return
        the response body. Unlike urlGetResponse, this works with both text and binary response data.
        As this routine buffers the entire response body, it should only be used for relatively small requests.
        \n\n
        This routine will block the current fiber while waiting for the request to complete.
        Other fibers continue to run.
    @param up URL object containing the response data
    @return The response body as runtime buffer. Returns NULL on errors. Caller must NOT free.
    @stability Evolving
 */
PUBLIC RBuf *urlGetResponseBuf(Url *up);

/**
    Get the HTTP response status code for a request.
    @description This will finalize the request if not already finalized.
    @param up URL object
    @return The HTTP status code for the most recently completed request.
    @stability Evolving
 */
PUBLIC int urlGetStatus(Url *up);

/**
    Parse a URL into its constituent components in the Url structure.
    @description This will parse URLs or URL portions. It will use default values for missing components. For example:
       'http://' will use localhost as the host, port 80 and a path of '/'. If passed 'a-string', it wll interpret this
       as the path portion and use 'http' as the scheme and 'localhost' as the host. If passed ':4100/' it will
       interpret this as the path portion and use 'http' as the scheme and 'localhost' as the host and port 4100. This
       should not be manually called while a URL request is in progress as it will invalidate the current request.
    @param up URL object
    @param url Url to parse.
    @return Zero if the url parses successfully.
    @stability Evolving
 */
PUBLIC int urlParse(Url *up, cchar *url);

/**
    Issue a HTTP POST request.
    @description This will issue a HTTP POST request to the specified URL with the optional body data.
        Regardless of the HTTP status, this will return the response body.
    @param url HTTP URL to fetch
    @param data Body data for request. Set to NULL if none.
    @param size Size of body data for request.
    @param headers Optional request headers. This parameter is a printf style formatted pattern with following
       arguments. Individual header lines must be terminated with "\r\n".
    @param ... Headers arguments
    @return Response body if successful, otherwise null. Caller must free.
    @stability Evolving
 */
PUBLIC char *urlPost(cchar *url, cvoid *data, ssize size, cchar *headers, ...);

/**
    Issue a HTTP POST request and return parsed JSON.
    @description This will issue a HTTP POST request to the specified URL with the optional body data. If successful,
        it will return the parsed response body.
    @param url HTTP URL to fetch
    @param data Body data for request. Set to NULL if none.
    @param len Size of body data for request.
    @param headers Optional request headers. This parameter is a printf style formatted pattern with following
        arguments. Individual header lines must be terminated with "\r\n".
    @return Parsed JSON response. If the request does not return a HTTP 200 status code or the response is not JSON,
        the request returns NULL. Caller must free via jsonFree().
    @stability Evolving
 */
PUBLIC Json *urlPostJson(cchar *url, cvoid *data, ssize len, cchar *headers, ...);

/**
    Low level read routine to read response data for a request.
    @description This routine may be called to progressively read response data. It should not be called
        if using $urlGetResponse directly or indirectly via urlGet, urlPost, urlPostJson or urlJson.
        This routine will block the current fiber if necessary. Other fibers continue to run.
    @param up URL object
    @param buf Buffer to read into
    @param bufsize Size of the buffer
    @return The number of bytes read. Returns < 0 on errors. Returns 0 when there is no more data to read.
    @stability Evolving
 */
PUBLIC ssize urlRead(Url *up, char *buf, ssize bufsize);

/**
    Set the maximum number of bytes to buffer from the response.
    @description This will limit the number of bytes that can be buffered from the response. If using urlRead, this
       limit will not apply. Use this to prevent excessive memory consumption for large responses.
    @param up URL object to configure
    @param limit Maximum number of bytes to buffer. Set to 0 for unlimited buffering.
    @stability Evolving
 */
PUBLIC void urlSetBufLimit(Url *up, ssize limit);

/**
    Define the certificates to use with TLS connections.
    @description Configure TLS client certificates for secure connections. All parameters are optional.
        Use NULL for any certificate parameter not required.
    @param up URL object to configure
    @param ca Certificate authority bundle to verify server certificates. PEM format.
    @param key Private key for client certificate authentication. PEM format.
    @param cert Client certificate for authentication. PEM format.
    @param revoke Certificate revocation list. PEM format.
    @stability Evolving
 */
PUBLIC void urlSetCerts(Url *up, cchar *ca, cchar *key, cchar *cert, cchar *revoke);

/**
    Set the list of available ciphers to use for TLS connections.
    @description Configure the cipher suites available for TLS negotiation.
        Use standard OpenSSL or MbedTLS cipher suite names.
    @param up URL object to configure
    @param ciphers Colon-separated list of cipher suites in order of preference
    @stability Evolving
 */
PUBLIC void urlSetCiphers(Url *up, cchar *ciphers);

/**
    Set the default request timeout to use for future URL instances.
    @description This does not change the timeout for existing Url objects. The timeout applies to
        the entire request including connection establishment, data transfer, and response reading.
    @param timeout Timeout in milliseconds. Set to 0 for no timeout.
    @stability Evolving
 */
PUBLIC void urlSetDefaultTimeout(Ticks timeout);

/**
    Set the URL flags for request tracing and protocol control.
    @description Configure debugging output and protocol behavior.
    @param up URL object to configure
    @param flags Bitwise OR of URL_SHOW_REQ_HEADERS, URL_SHOW_REQ_BODY, URL_SHOW_RESP_HEADERS,
        URL_SHOW_RESP_BODY, URL_HTTP_0 flags
    @stability Evolving
 */
PUBLIC void urlSetFlags(Url *up, int flags);

/**
    Set the HTTP response status code.
    @description Override the HTTP response status. Typically used internally or for testing.
    @param up URL object to modify
    @param status HTTP status code to set
    @stability Evolving
 */
PUBLIC void urlSetStatus(Url *up, int status);

/**
    Set the HTTP protocol to use
    @param up URL object
    @param protocol Set to 0 for HTTP/1.0 or 1 for HTTP/1.1. Defaults to 1.
    @stability Evolving
 */
PUBLIC void urlSetProtocol(Url *up, int protocol);

/**
    Set the request timeout to use for the specific URL object.
    @description The timeout applies to the entire request including connection establishment,
        data transfer, and response reading. Overrides the default timeout for this URL object.
    @param up URL object to configure
    @param timeout Timeout in milliseconds. Set to 0 for no timeout.
    @stability Evolving
 */
PUBLIC void urlSetTimeout(Url *up, Ticks timeout);

/**
    Control verification of TLS connections
    @param up URL object
    @param verifyPeer Set to true to verify the certificate of the remote peer.
    @param verifyIssuer Set to true to verify the issuer of the peer certificate.
    @stability Evolving
 */
PUBLIC void urlSetVerify(Url *up, int verifyPeer, int verifyIssuer);

/**
    Start a Url request.
    @description This is a low level API that initiates a connection to a remote HTTP resource.
        Use $urlWriteHeaders to write headers and $urlWrite to write any request body.
        Must call $urlFinalize to signify the end of the request body.
    @param up URL object
    @param method HTTP method verb.
    @param url HTTP URL to fetch
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int urlStart(Url *up, cchar *method, cchar *url);

/**
    Upload files in a multipart/form-data request.
    @description This constructs and writes the HTTP request headers for a multipart/form-data request.
        Use this routine instead of urlWriteHeaders to write headers.
        Use urlStart to initiate the request before calling urlUpload.
    @param up URL object for the request
    @param files List of filenames to upload. May be NULL if no files.
    @param forms Hash of key/value form values to add to the request. May be NULL if no form data.
    @param headers Printf style formatted pattern with following arguments. Individual header lines must be
       terminated with "\r\n". May be NULL.
    @param ... Optional header arguments
    @return Zero if successful, negative error code on failure.
    @stability Evolving
 */
PUBLIC int urlUpload(Url *up, RList *files, RHash *forms, cchar *headers, ...);

/**
    Write body data for a request
    @description This routine will block the current fiber. Other fibers continue to run.
    @param up URL object
    @param data Buffer of data to write. Set to NULL to finalize the request body.
    @param size Length of data to write. Set to -1 to calculate the length of data as a null terminated string.
    @return The number of bytes actually written. On errors, returns a negative status code.
    @stability Evolving
 */
PUBLIC ssize urlWrite(Url *up, cvoid *data, ssize size);

/**
    Write formatted body data for a request
    @description This routine will block the current fiber. Other fibers continue to run.
    @param up URL object
    @param fmt Printf style format string
    @param ... Format arguments
    @return The number of bytes actually written. On errors, returns a negative status code.
    @stability Evolving
 */
PUBLIC ssize urlWriteFmt(Url *up, cchar *fmt, ...);

/**
    Write a file contents for a request
    @description This routine will read the file contents and write to the client.
        It will block the current fiber. Other fibers continue to run.
    @param up URL object
    @param path File pathname.
    @return The number of bytes actually written. On errors, returns a negative status code.
    @stability Evolving
 */
PUBLIC ssize urlWriteFile(Url *up, cchar *path);

/**
    Write request headers using a printf style formatted pattern
    @description This will write the HTTP request line and supplied headers. If Host and/or Content-Length are
        not supplied, they will be added if known. This routine will add a Transfer-Encoding: chunked header if
        the Content-Length is not supplied and the request method is not a GET. If the HTTP scheme is ws or wss,
        this routine will add the WebSocket upgrade headers. Request headers will be traced if the flags have
        URL_SHOW_REQ_HEADERS set.
        This routine will block the current fiber. Other fibers continue to run.
    @param up URL object
    @param headers Optional request headers. This parameter is a printf style formatted pattern with following
        arguments. Individual header lines must be terminated with "\r\n".
    @param ... Optional header arguments.
    @return Zero if successful
    @stability Evolving
 */
PUBLIC int urlWriteHeaders(Url *up, cchar *headers);

#if ME_COM_WEBSOCKETS
/**
    Issue a simple WebSocket request.
    @description Establishes a WebSocket connection and processes messages until the connection closes.
        This is a blocking call that will run until the WebSocket is closed.
    @param url WebSocket URL (ws:// or wss://)
    @param callback Callback function to invoke for received WebSocket messages
    @param arg User argument to pass to the callback function
    @param headers Optional request headers. Individual header lines must be terminated with "\r\n"
    @return Zero when connection closes normally, negative error code on failure.
    @stability Evolving
 */
PUBLIC int urlWebSocket(cchar *url, WebSocketProc callback, void *arg, cchar *headers);

/**
    Get the WebSocket object for a URL request.
    @description Retrieve the underlying WebSocket object after a successful WebSocket upgrade.
        The URL object must have completed a WebSocket handshake.
    @param up URL object with established WebSocket connection
    @return The WebSocket object for the URL request. Returns NULL if no WebSocket connection.
    @stability Evolving
 */
PUBLIC WebSocket *urlGetWebSocket(Url *up);

/**
    Define async WebSocket callbacks.
    @description Configure asynchronous WebSocket message handling. This will invoke the callback function
        with an OPEN event initially, then MESSAGE events for received data.
    @param up URL object with WebSocket connection
    @param callback Callback function to invoke for WebSocket events
    @param arg User argument to pass to the callback function
    @stability Evolving
 */
PUBLIC void urlWebSocketAsync(Url *up, WebSocketProc callback, void *arg);
#endif /* ME_COM_WEBSOCKETS */

#if URL_SSE
/**
    Wait for Server-Sent Events.
    @description Block the current fiber waiting for SSE events. Other fibers continue to run.
        Events will be delivered via the SSE callback if configured.
    @param up URL object with active SSE connection
    @return Zero if successful, negative error code on failure.
    @stability Evolving
 */
PUBLIC int urlSseWait(Url *up);

/**
    Define an SSE async callback.
    @description Configure asynchronous Server-Sent Event handling. The callback function will be
        invoked when SSE events are received from the server.
    @param up URL object with SSE connection
    @param proc Callback function to invoke for SSE events
    @param arg User argument to pass to the callback function
    @stability Evolving
 */
PUBLIC void urlSseAsync(Url *up, UrlSseProc proc, void *arg);

/**
    Get Server-Sent Events from a URL.
    @description Establish a connection to receive SSE events and invoke the callback for each event.
        This is a blocking call that runs until the connection closes.
    @param uri URL to get events from (http:// or https://)
    @param proc Callback function to invoke for received SSE events
    @param arg User argument to pass to the callback function
    @param headers Optional request headers. Printf style format string.
    @param ... Optional header arguments
    @return Zero if successful, negative error code on failure.
    @stability Evolving
 */
PUBLIC int urlGetEvents(cchar *uri, UrlSseProc proc, void *arg, char *headers, ...);

/**
    Set the maximum number of retries for SSE requests.
    @description Configure automatic retry behavior for SSE connections that fail or disconnect.
        The client will automatically attempt to reconnect up to the specified limit.
    @param up URL object to configure
    @param maxRetries Maximum number of automatic reconnection attempts
    @stability Evolving
 */
PUBLIC void urlSetMaxRetries(Url *up, int maxRetries);
#endif

#if URL_SSE || ME_COM_WEBSOCKETS
/**
    Wait for the connection to be closed.
    @description Block the current fiber until the connection closes. Used for SSE and WebSocket connections
        to keep the connection alive until the remote end closes it.
    @param up URL object with active connection
    @return Zero when connection closes normally, negative error code on failure.
    @stability Evolving
 */
PUBLIC int urlWait(Url *up);
#endif /* URL_SSE || ME_COM_WEBSOCKETS */

#ifdef __cplusplus
}
#endif

#endif /* ME_COM_URL */
#endif /* _h_URL */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
