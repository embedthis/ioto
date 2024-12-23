
/*
    url.h -- Header for the URL client HTTP library

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#pragma once

#ifndef _h_URL
#define _h_URL 1

/********************************** Includes **********************************/

#include "me.h"
#include "r.h"
#include "json.h"

/*********************************** Defines **********************************/
#if ME_COM_URL

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

/*
    UrlProc events
 */
#define URL_NOTIFY_IO                   1    /**< Streaming I/O event */
#define URL_NOTIFY_RETRY                2    /**< Retry initiated. Received only if streaming */
#define URL_NOTIFY_DONE                 3    /**< Request is complete */

/**
    URL callback procedure
    @param up URL object
    @param event Type of event. Set to URL_NOTIFY_IO, URL_NOTIFY_RETRY or URL_NOTIFY_DONE.
    @param buf Associated data
    @param len Associated data length
    @ingroup Url
    @stability Evolving
 */
typedef void (*UrlProc)(struct Url *up, int event, char *buf, ssize len);

/**
    Url request object
    @description The Url service is a streaming HTTP request client. This service requires the use of fibers from the
        portable runtime.
    @defgroup Url Url
    @stability Evolving
 */
typedef struct Url {
    uint status : 10;               /**< Response (rx) status */
    uint retries : 4;               /**< Client implemented retry field */
    uint chunked : 4;               /**< Request is using transfer chunk encoding */
    uint close : 1;                 /**< Connection should be closed on completion of the current request */
    uint certsDefined : 1;          /**< Certificates have been defined */
    uint protocol : 2;              /**< Use HTTP/1.0 without keep-alive. Defaults to HTTP/1.1. */
    uint gotResponse : 1;           /**< Response has been read */
    uint wroteHeaders : 1;          /**< Tx headers have been written */
    uint flags;                     /**< Alloc flags */

    char *method;                   /** HTTP request method */
    char *urlbuf;                   /**< Parsed and tokenized url */
    char *boundary;                 /**< Multipart mime upload file boundary */
    ssize txLen;                    /**< Length of tx body */

    RBuf *rx;                       /**< Buffer for progressive reading response data */
    char *response;                 /**< Response as a string */
    RBuf *responseBuf;              /**< Buffer to hold complete response */
    ssize rxLen;                    /**< Length of rx body */
    ssize rxRemaining;              /**< Remaining rx data to read from the socket */

    RBuf *rxHeaders;                /**< Buffer for Rx headers */

    char *error;                    /**< Error message */
    cchar *host;                    /**< Request host */
    cchar *path;                    /**< Request path without leading "/" and query/ref */
    cchar *query;                   /**< Request query portion */
    cchar *hash;                    /**< Request hash portion */
    cchar *scheme;                  /**< Request scheme */
    cchar *redirect;                /**< Redirect location */
    int port;                       /**< Request port */

    RSocket *sock;                  /**< Network socket */
    Ticks deadline;                 /**< Request time limit expiry deadline */
    Ticks timeout;                  /**< Request timeout */
} Url;

/**
    Allocate a URL object
    @description A URL object represents a network connection on which one or more HTTP client requests may be
        issued one at a time.
    @param flags Set flags to URL_SHOW_REQ_HEADERS | URL_SHOW_REQ_BODY | URL_SHOW_RESP_HEADERS | URL_SHOW_RESP_BODY.
        Defaults to 0.
    @return The url object
    @ingroup Url
    @stability Evolving
 */
PUBLIC Url *urlAlloc(int flags);

/**
    Set the URL flags
    @param up URL object.
    @param flags Set flags to URL_SHOW_REQ_HEADERS | URL_SHOW_REQ_BODY | URL_SHOW_RESP_HEADERS | URL_SHOW_RESP_BODY.
    @ingroup Url
    @stability Prototype
 */
PUBLIC void urlSetFlags(Url *up, int flags);

/**
    Close the underlying network socket associated with the URL object.
    @description This is not normally necessary to invoke unless you want to force the socket
        connection to be recreated before issuing a subsequent request.
    @param up URL object.
    @ingroup Url
    @stability Evolving
 */
PUBLIC void urlClose(Url *up);

/**
    Fetch a URL
    @description This routine issues a HTTP request with optional body data and returns the HTTP status
    code. This routine will return before the response body data has been read. Use $urlRead or $urlGetResponse
    to read the response body data.
    This routine will block the current fiber while waiting for the request to respond. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param up URL object.
    @param method HTTP method verb.
    @param uri HTTP URL to fetch
    @param data Body data for request. Set to NULL if none.
    @param size Size of body data for request. Set to 0 if none.
    @param headers Optional request headers. This parameter is a printf style formatted pattern with following
        arguments. Individual header lines must be terminated with "\r\n".
    @param ... Optional header arguments.
    @return Response HTTP status code. Use urlGetResponse or urlRead to read the response.
    @ingroup Url
    @stability Evolving
 */
PUBLIC int urlFetch(Url *up, cchar *method, cchar *uri, cvoid *data, ssize size, cchar *headers, ...);

/**
    Fetch a URL and return a JSON response.
    @description This routine issues a HTTP request and reads and parses the response into a JSON object tree.
        This routine will block the current fiber while waiting for the request to complete. Other fibers continue to
           run.
    @pre Must only be called from a fiber.
    @param up URL object.
    @param method HTTP method verb.
    @param uri HTTP URL to fetch
    @param data Body data for request. Set to NULL if none.
    @param size Size of body data for request. Set to 0 if none.
    @param headers Optional request headers. This parameter is a printf style formatted pattern with following
        arguments. Individual header lines must be terminated with "\r\n". If the headers are not provided, a headers
           value of "Content-Type: application/json\r\n" is used.
    @param ... Optional header arguments.
    @return Parsed JSON response. If the request does not return a HTTP 200 status code or the response is not JSON,
        the request returns NULL. Use urlGetError to get any error string and urlGetStatus to get the HTTP status code.
        Caller must free via jsonFree().
    @ingroup Url
    @stability Evolving
 */
PUBLIC Json *urlJson(Url *up, cchar *method, cchar *uri, cvoid *data, ssize size, cchar *headers, ...);

/**
    Finalize request body.
    @description This routine will call urlWrite(web, NULL, 0);
    @param url Url object
    @return Number of bytes written. Returns a negative status code on errors.
    @ingroup Url
    @stability Evolving
 */
PUBLIC ssize urlFinalize(Url *url);

/**
    Free a URL object
    @param up URL object
    @ingroup Url
    @stability Evolving
 */
PUBLIC void urlFree(Url *up);

/**
    Get a URL using a HTTP GET request.
    @description This routine will block the current fiber while waiting for the request to complete.
        Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param uri HTTP URL to fetch
    @param headers Optional request headers. This parameter is a printf style formatted pattern with following
        arguments. Individual header lines must be terminated with "\r\n".
    @param ... Optional header arguments.
    @return Response body if successful, otherwise null. Caller must free.
    @ingroup Url
    @stability Evolving
 */
PUBLIC char *urlGet(cchar *uri, cchar *headers, ...);

/**
    Get the URL internal error message
    @description Errors are defined for unexpected errors. HTTP requests that respond with a non-200 status
        do not set the error message.
    @param up URL object
    @return The URL error message for the most recent request. Returns NULL if no error message defined.
        Caller must NOT free this message.
    @ingroup Url
    @stability Evolving
 */
PUBLIC cchar *urlGetError(Url *up);

/**
    Get a response HTTP header from the parsed response headers.
    @param up URL object
    @param header HTTP header name. This can be any case. For example: "Authorization".
    @return The value of the HTTP header. Returns NULL if not defined. Caller must NOT free the returned string.
    @ingroup Url
    @stability Evolving
 */
PUBLIC cchar *urlGetHeader(Url *up, cchar *header);

/**
    Issue a HTTP GET request and return parsed JSON.
    @pre Must only be called from a fiber.
    @param uri HTTP URL to fetch
    @param headers Optional request headers. This parameter is a printf style formatted pattern with following
        arguments. Individual header lines must be terminated with "\r\n".
    @param ... Optional header arguments.
    @return Parsed JSON response. If the request does not return a HTTP 200 status code or the response
        is not JSON, the request returns NULL. Caller must free via jsonFree().
    @ingroup Url
    @stability Evolving
 */
PUBLIC Json *urlGetJson(cchar *uri, cchar *headers, ...);

/**
    Get the response to a URL request as a JSON object tree.
    @description After issuing urlFetch, urlGet or urlPost, this routine may be called to read and parse the response
    as a JSON object. This call should only be used when the response is a valid JSON UTF-8 string.
        This routine buffers the entire response body and creates the parsed JSON tree.
        \n\n
        This routine will block the current fiber while waiting for the request to complete.
        Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param up URL object
    @return The response body as parsed JSON object. Caller must free the result via jsonFree.
    @ingroup Url
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
    @pre Must only be called from a fiber.
    @param up URL object
    @return The response body as a string. Caller must NOT free. Will return an empty string on errors.
    @ingroup Url
    @stability Evolving
 */
PUBLIC cchar *urlGetResponse(Url *up);

/**
    Get the response to a URL request in a buffer
    @description After issuing urlFetch, urlGet or urlPost, this routine may be called to read, buffer and return
        the response body. This call should only be used when the response is a valid UTF-8 string. Otherwise, use
        urlRead to read the response body. As this routine buffers the entire response body, it should only be used for
        relatively small requests. Otherwise, the memory footprint of the application may be larger than desired.
        \n\n
        This routine will block the current fiber while waiting for the request to complete.
        Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param up URL object
    @return The response body as runtime buffer. Caller must NOT free.
    @ingroup Url
    @stability Evolving
 */
PUBLIC RBuf *urlGetResponseBuf(Url *up);

/**
    Get the HTTP response status code for a request.
    @param up URL object
    @return The HTTP status code for the most recently completed request.
    @ingroup Url
    @stability Evolving
 */
PUBLIC int urlGetStatus(Url *up);

/**
    Parse a URL into its constituent components in the Url structure.
    @param up URL object
    @param uri Uri to parse.
    @return Zero if the uri parses successfully.
    @ingroup Url
    @stability Evolving
 */
PUBLIC int urlParse(Url *up, cchar *uri);

/**
    Issue a HTTP POST request.
    @pre Must only be called from a fiber.
    @param uri HTTP URL to fetch
    @param data Body data for request. Set to NULL if none.
    @param size Size of body data for request.
    @param headers Optional request headers. This parameter is a printf style formatted pattern with following
       arguments. Individual header lines must be terminated with "\r\n".
    @param ... Headers arguments
    @return Response body if successful, otherwise null. Caller must free.
    @ingroup Url
    @stability Evolving
 */
PUBLIC char *urlPost(cchar *uri, cvoid *data, ssize size, cchar *headers, ...);

/**
    Issue a HTTP POST request and return parsed JSON.
    @pre Must only be called from a fiber.
    @param uri HTTP URL to fetch
    @param data Body data for request. Set to NULL if none.
    @param len Size of body data for request.
    @param headers Optional request headers. This parameter is a printf style formatted pattern with following
        arguments. Individual header lines must be terminated with "\r\n".
    @return Parsed JSON response. If the request does not return a HTTP 200 status code or the response is not JSON,
        the request returns NULL. Caller must free via jsonFree().
    @ingroup Url
    @stability Evolving
 */
PUBLIC Json *urlPostJson(cchar *uri, cvoid *data, ssize len, cchar *headers, ...);

/**
    Low level read routine to read response data for a request.
    @description This routine may be called to progressively read response data. It should not be called
        if using urlGetResponse directly or indirectly via urlGet, urlPost, urlPostJson or urlJson.
        This routine will block the current fiber if necessary. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param up URL object
    @param buf Buffer to read into
    @param bufsize Size of the buffer
    @return The number of bytes read. Returns < 0 on errors. Returns 0 when there is no more data to read.
    @ingroup Url
    @stability Evolving
 */
PUBLIC ssize urlRead(Url *up, char *buf, ssize bufsize);

/**
    Define the certificates to use with TLS
    @param up URL object
    @param ca Certificate authority to verify client certificates
    @param key Certificate private key
    @param cert Certificate text
    @param revoke Certificates to revoke
    @ingroup Url
    @stability Evolving
 */
PUBLIC void urlSetCerts(Url *up, cchar *ca, cchar *key, cchar *cert, cchar *revoke);

/**
    Set the list of available ciphers to use
    @param up URL object
    @param ciphers String list of available ciphers
    @ingroup Url
    @stability Prototype
 */
PUBLIC void urlSetCiphers(Url *up, cchar *ciphers);

/**
    Set the default number of retries for requests.
    @description This does not change the number of retries for existing Url objects.
    @param retries Number of retries.
    @ingroup Url
    @stability Evolving
 */
PUBLIC void urlSetDefaultRetries(int retries);

/**
    Set the default request timeout to use for future URL instances.
    @description This does not change the timeout for existing Url objects.
    @param timeout Timeout in milliseconds.
    @ingroup Url
    @stability Evolving
 */
PUBLIC void urlSetDefaultTimeout(Ticks timeout);

/**
    Set the URL internal error message
    @param up URL object
    @param message Printf style message format string
    @param ... Message arguments
    @ingroup Url
    @stability Evolving
 */
PUBLIC void urlSetError(Url *up, cchar *message, ...);

/**
    Set the URL internal error message and HTTP response status
    @param up URL object
    @param status HTTP status code
    @param message Printf style message format string
    @param ... Message arguments
    @ingroup Url
    @stability Evolving
 */
PUBLIC void urlSetStatusError(Url *up, int status, cchar *message, ...);

/**
    Set the HTTP protocol to use
    @param up URL object
    @param protocol Set to 0 for HTTP/1.0 or 1 for HTTP/1.1. Defaults to 1.
    @ingroup Url
    @stability Prototype
 */
PUBLIC void urlSetProtocol(Url *up, int protocol);

/**
    Set the request timeout to use for the specific URL object.
    @param up URL object
    @param timeout Timeout in milliseconds.
    @ingroup Url
    @stability Evolving
 */
PUBLIC void urlSetTimeout(Url *up, Ticks timeout);

/**
    Control verification of TLS connections
    @param up URL object
    @param verifyPeer Set to true to verify the certificate of the remote peer.
    @param verifyIssuer Set to true to verify the issuer of the peer certificate.
    @ingroup Url
    @stability Evolving
 */
PUBLIC void urlSetVerify(Url *up, int verifyPeer, int verifyIssuer);

/**
    Start a Url request.
    @description This is a low level API that initiates a connection to a remote HTTP resource.
        Use urlWriteHeaders and urlWrite to send the request.
    @pre Must only be called from a fiber.
    @param up URL object
    @param method HTTP method verb.
    @param uri HTTP URL to fetch
    @param txLen Content length of request body. This must match the length of data written with urlWrite. Set to -1 if
       the content length is not known.
    @return Zero if successful.
    @ingroup Url
    @stability Evolving
 */
PUBLIC int urlStart(Url *up, cchar *method, cchar *uri, ssize txLen);

/**
    Upload files in a request.
    @description This is a low level API that initiates a connection to a remote HTTP resource.
        Use urlWriteHeaders and urlWrite to send the request.
    @pre Must only be called from a fiber.
    @param up URL object
    @param files List of filenames to upload.
    @param forms Hash of key/value form values to add to the request.
    @param headers Optional request headers. This parameter is a printf style formatted pattern with following
       arguments. Individual header lines must be terminated with "\r\n".
    @param ... Headers arguments
    @return Zero if successful.
    @ingroup Url
    @stability Evolving
 */
PUBLIC int urlUpload(Url *up, RList *files, RHash *forms, cchar *headers, ...);

/**
    Write body data for a request
    @description This routine will block the current fiber. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param up URL object
    @param data Buffer of data to write
    @param size Length of data to write. Set to -1 to calculate the length of data as a null terminated string.
    @return The number of bytes actually written. On errors, returns a negative status code.
    @ingroup Url
    @stability Evolving
 */
PUBLIC ssize urlWrite(Url *up, cvoid *data, ssize size);

/**
    Write formatted body data for a request
    @description This routine will block the current fiber. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param up URL object
    @param fmt Printf style format string
    @param ... Format arguments
    @return The number of bytes actually written. On errors, returns a negative status code.
    @ingroup Url
    @stability Evolving
 */
PUBLIC ssize urlWriteFmt(Url *up, cchar *fmt, ...);

/**
    Write a file contents for a request
    @description This routine will read the file contents and write to the client.
        It will block the current fiber. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param up URL object
    @param path File pathname.
    @return The number of bytes actually written. On errors, returns a negative status code.
    @ingroup Url
    @stability Evolving
 */
PUBLIC ssize urlWriteFile(Url *up, cchar *path);

/**
    Write request headers
    @description This will write the HTTP request line and supplied headers. If Host and/or Content-Length are
        not supplied, they will be added if known.
        This routine will block the current fiber. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param up URL object
    @param headers Optional request headers. This parameter is a printf style formatted pattern with following
        arguments. Individual header lines must be terminated with "\r\n".
    @param ... Optional header arguments.
    @return Zero if successful
    @ingroup Url
    @stability Evolving
 */
PUBLIC int urlWriteHeaders(Url *up, cchar *headers, ...);

#ifdef __cplusplus
}
#endif

#endif /* ME_COM_URL */
#endif /* _h_URL */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
