/*
    web.h -- Web server main header

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_WEB
#define _h_WEB 1

/********************************** Includes **********************************/

#include "me.h"

#include "r.h"
#include "json.h"
#include "crypt.h"
#include "websockets.h"

#if ME_COM_WEB
/*********************************** Defines **********************************/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ME_WEB_AUTH
    #define ME_WEB_AUTH        1
#endif
#ifndef ME_WEB_LIMITS
    #define ME_WEB_LIMITS      1
#endif
#ifndef ME_WEB_SESSIONS
    #define ME_WEB_SESSIONS    1
#endif
#ifndef ME_WEB_UPLOAD
    #define ME_WEB_UPLOAD      1
#endif
#ifndef ME_WEB_LIMITS
    #define ME_WEB_LIMITS      1                    /* Enable limit checking */
#endif
#ifndef ME_COM_WEBSOCKETS
    #define ME_COM_WEBSOCKETS  1                    /* Enable web sockets */
#endif

#define WEB_MAX_SIG            160                  /* Size of controller.method URL portion */

/*
    Dependencies
 */
#ifndef ME_WEB_CONFIG
    #define ME_WEB_CONFIG      "web.json5"
#endif
#ifndef WEB_SESSION_COOKIE
    #define WEB_SESSION_COOKIE "-web-session-"
#endif
#define WEB_SESSION_USERNAME   "_:username:_"       /* Session state username variable */
#define WEB_SESSION_ROLE       "_:role:_"           /* Session state role variable */

#define WEB_UNLIMITED          MAXINT64

#define WEB_CHUNK_START        1                    /**< Start of a new chunk */
#define WEB_CHUNK_DATA         2                    /**< Start of chunk data */
#define WEB_CHUNK_EOF          4                    /**< End of chunk data */

#define WEB_HEADERS            16                   /* Initial number of headers to provide for */

/*
    Forward declare
 */

/**
    Embedded web server
    @description The web server requires the use of fibers from the portable runtime.
    @stability Evolving
 */
struct Web;
struct WebAction;
struct WebHost;
struct WebRoute;
struct WebSession;
struct WebUpload;

/******************************************************************************/

/**
    WebAction callback procedure
    @param web Web object
    @stability Evolving
 */
typedef void (*WebProc)(struct Web *web);

/**
    WebHook callback procedure
    @param web Web object
    @param event ID of the event
    @stability Evolving
 */
typedef int (*WebHook)(struct Web *web, int event);

/**
    Action function bound to a URL prefix
    @stability Evolving
 */
typedef struct WebAction {
    char *role;                         /**< Role to invoke action */
    char *match;                        /**< Path prefix */
    WebProc fn;                         /**< Function to invoke */
} WebAction;

/**
    Routing object to match a request against a path prefix
    @stability Evolving
 */
typedef struct WebRoute {
    cchar *match;                       /**< Matching URI path pattern */
    bool validate : 1;                  /**< Validate request */
    bool exact : 1;                     /**< Exact match vs prefix match. If trailing "/" in route. */
    RHash *methods;                     /**< HTTP methods verbs */
    cchar *handler;                     /**< Request handler (file, action) */
    cchar *role;                        /**< Required user role */
    cchar *redirect;                    /**< Redirection */
    cchar *trim;                        /**< Portion to trim from path */
    bool stream;                        /**< Stream request body */
} WebRoute;

/**
    Site wide redirection
    @stability Evolving
 */
typedef struct WebRedirect {
    cchar *from;                        /**< Original URL path */
    cchar *to;                          /**< Target URL */
    int status;                         /**< Redirection HTTP status code */
} WebRedirect;

/**
    Initialize the web module
    @description Must call before using Web.
    @stability Evolving
 */
PUBLIC int webInit(void);

/**
    Initialize the web module
    @stability Evolving
 */
PUBLIC void webTerm(void);

/************************************* Host ***********************************/
/**
    Web host structure.
    @description The web host defines a web server and its configuration. Multiple web hosts can be
        created.
 */
typedef struct WebHost {
    RList *listeners;           /**< Listening endpoints for this host */
    RList *webs;                /**< Active web requests for this host */
    Json *config;               /**< Web server configuration for this host */
    Json *signatures;           /**< Web Rest API signatures */

    int flags;                  /**< Control flags */
    bool freeConfig : 1;        /**< Config is allocated and must be freed */
    bool httpOnly : 1;          /**< Cookie httpOnly flag */
    bool strictSignatures : 1;  /** Enforce strict signature compliance */

    WebHook hook;               /**< Web notification hook */
    RHash *users;               /**< Hash table of users */
    RHash *sessions;            /**< Client session state */
    RHash *methods;             /**< Default HTTP methods verbs */
    RHash *mimeTypes;           /**< Mime types indexed by extension */
    RList *actions;             /**< Ordered list of configured actions */
    RList *routes;              /**< Ordered list of configured routes */
    RList *redirects;           /**< Ordered list of redirections */
    REvent sessionEvent;        /**< Session timer event */
    int roles;                  /**< Base ID of roles in config */
    int headers;                /**< Base ID for headers in config */

    cchar *name;                /**< Host name to use in canonical redirects */
    cchar *index;               /**< Index file to use for directory requests */
    cchar *sameSite;            /**< Cookie same site property */
    char *docs;                 /**< Web site documents */
    char *ip;                   /**< IP to use in redirects if indeterminate */

#if ME_WEB_UPLOAD
    //  Upload
    cchar *uploadDir;           /**< Directory to receive uploaded files */
    bool removeUploads : 1;     /**< Remove uploads when request completes (default) */
#endif
    //  Timeouts
    int inactivityTimeout;      /**< Timeout in secs for inactivity on a connection */
    int parseTimeout;           /**< Timeout in secs while parsing a request */
    int requestTimeout;         /**< Total request timeout in secs */
    int sessionTimeout;         /**< Inactivity timeout in secs for session state */

    ssize connections;          /**< Number of active connections */

    //  Limits
#if ME_WEB_LIMITS || DOXYGEN
    int64 maxBuffer;            /**< Max response buffer size */
    int64 maxHeader;            /**< Max header size */
    int64 maxConnections;       /**< Max number of connections */
    int64 maxBody;              /**< Max size of POST request */
    int64 maxSessions;          /**< Max number of sessions */
    int64 maxUpload;            /**< Max size of file upload */
    int64 maxUploads;           /**< Max number of files to upload */

#if ME_COM_WEBSOCKETS
    cchar *webSocketsProtocol;  /** WebSocket application protocol */
    int64 webSocketsMaxMessage; /** Max message size */
    int64 webSocketsMaxFrame;   /** Max frame size */
    int64 webSocketsPingPeriod; /** Ping period */
    bool webSocketsValidateUTF; /** Validate UTF */
    bool webSocketsEnable;      /** Enable WebSocket */
#endif /* ME_COM_WEBSOCKETS */
#endif /* ME_WEB_LIMITS */
} WebHost;

/**
    Add an action callback for a URL prefix
    @description An action routine is a C function that is bound to a set of URLs.
        The action routine will be invoked for URLs that match the leading URL prefix.
    @param host Host object
    @param prefix Leading URL prefix to associate with this action
    @param fn Function to invoke for requests matching this URL
    @param role Required user role for the action
    @stability Evolving
 */
PUBLIC void webAddAction(WebHost *host, cchar *prefix, WebProc fn, cchar *role);

/*
    webHostAlloc flags
 */
#define WEB_SHOW_NONE         0x1   /**< Trace nothing */
#define WEB_SHOW_REQ_BODY     0x2   /**< Trace request body */
#define WEB_SHOW_REQ_HEADERS  0x4   /**< Trace request headers */
#define WEB_SHOW_RESP_BODY    0x8   /**< Trace response body */
#define WEB_SHOW_RESP_HEADERS 0x10  /**< Trace response headers */

/**
    Allocate a new host object
    @description After allocating, the host may be started via webStartHost.
    @param config JSON configuration for the host.
    @param flags Allocation flags. Set to WEB_SHOW_NONE, WEB_SHOW_REQ_BODY, WEB_SHOW_REQ_HEADERS,
        REQ_SHOW_RESP_BODY, WEB_SHOW_RESP_HEADERS.
    @return A host object.
    @stability Evolving
 */
PUBLIC WebHost *webAllocHost(Json *config, int flags);

/**
    Free a host object
    @param host Host object.
    @stability Evolving
 */
PUBLIC void webFreeHost(WebHost *host);

/**
    Get the web documents directory for a host.
    @description This is configured via the web.documents configuration property.
    @param host Host object
    @return The web documents directory
    @stability Evolving
 */

PUBLIC cchar *webGetDocs(WebHost *host);

/**
    Set the IP address for the host
    @param host Host object
    @param ip IP address
    @stability Evolving
 */
PUBLIC void webSetHostDefaultIP(WebHost *host, cchar *ip);

/**
    Start listening for requests on the host
    @pre Must only be called from a fiber.
    @param host Host object
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int webStartHost(WebHost *host);

/**
    Stop listening for requests on the host
    @pre Must only be called from a fiber.
    @param host Host object
    @stability Evolving
 */
PUBLIC void webStopHost(WebHost *host);

/************************************ Upload **********************************/
#if ME_WEB_UPLOAD || DOXYGEN
/**
    File upload structure
    @see websUploadOpen websLookupUpload websGetUpload
 */
typedef struct WebUpload {
    char *filename;       /**< Local (temp) name of the file */
    char *clientFilename; /**< Client side name of the file */
    char *contentType;    /**< Content type */
    char *name;           /**< Symbolic name for the upload supplied by the client */
    ssize size;           /**< Uploaded file size */
    int fd;               /**< File descriptor used while writing the upload content */
} WebUpload;

/*
    Internal APIs
 */
PUBLIC int webInitUpload(struct Web *web);
PUBLIC void webFreeUpload(struct Web *web);
PUBLIC int webProcessUpload(struct Web *web);
#endif

/*************************************** Web **********************************/
/*
    Web hook event types
 */
#define WEB_HOOK_CONNECT    1 /**< New socket connection */
#define WEB_HOOK_DISCONNECT 2 /**< New socket connection */
#define WEB_HOOK_START      3 /**< Start new request */
#define WEB_HOOK_RUN        4 /**< Ready to run request or run custom request processing */
#define WEB_HOOK_ACTION     5 /**< Request an action */
#define WEB_HOOK_NOT_FOUND  6 /**< Document not found */
#define WEB_HOOK_ERROR      7 /**< Request error */
#define WEB_HOOK_CLOSE      8 /**< WebSocket close */
#define WEB_HOOK_END        9 /**< End of request */

typedef struct WebListen {
    RSocket *sock;            /**< Socket */
    char *endpoint;           /**< Endpoint definition */
    int port;                 /**< Listening port */
    WebHost *host;            /**< Host owning this listener */
} WebListen;

/*
    Properties ordered for debugability
 */
typedef struct Web {
    char *error;                /**< Error string for any request errors */
    cchar *method;              /**< Request method in upper case */
    char *url;                  /**< Full request url */
    char *path;                 /**< Path portion of the request url */

    RBuf *body;                 /**< Receive boday buffer transferred from rx */
    RBuf *rx;                   /**< Receive data buffer */
    RBuf *trace;                /**< Packet trace buffer */
    RBuf *buffer;               /**< Buffered response */
    Json *validatedJson;        /**< Used for validated responses that need to mutate the response */

    Offset chunkRemaining;      /**< Amount of chunked body to read */
    ssize rxLen;                /**< Receive content length (including chunked requests) */
    Offset rxRemaining;         /**< Amount of rx body data left to read in this request */
    ssize txLen;                /**< Transmit response body content length */
    Offset txRemaining;         /**< Transmit body data remaining to send */
    ssize lastEventId;          /**< Last event ID (SSE) */

    uint status : 16;           /**< Request response HTTP status code */
    uint chunked : 4;           /**< Receive transfer chunk encoding state */
    uint authenticated : 1;     /**< User authenticated and roleId defined */
    uint authChecked : 1;       /**< Authentication has been checked */
    uint close : 1;             /**< Should the connection be closed after the request completes */
    uint exists : 1;            /**< Does the requested resource exist */
    uint finalized : 1;         /**< The response has been finalized */
    uint formBody : 1;          /**< Is the current request a POSTed form */
    uint http10 : 1;            /**< Is the current request a HTTP/1.0 request */
    uint jsonBody : 1;          /**< Is the current request a POSTed json request */
    uint moreBody : 1;          /**< More response body to trace */
    uint secure : 1;            /**< Has secure listening endpoint */
    uint upgraded : 1;          /**< Is the connection upgraded to a WebSocket */
    uint writingHeaders : 1;    /**< Are headers being created and written */
    uint wroteHeaders : 1;      /**< Have the response headers been written */

    RFiber *fiber;              /**< Original owning fiber */
    WebHost *host;              /**< Owning host object */
    struct WebSession *session; /**< Session state */
    WebRoute *route;            /**< Matching route for this request */
    WebListen *listen;          /**< Listening endpoint */

    Json *vars;                 /**< Parsed request body variables */
    Json *qvars;                /**< Parsed request query string variables */
    RSocket *sock;

    Ticks started;              /**< Time when the request started */
    Ticks deadline;             /**< Timeout deadline for when the next I/O must complete */

    RBuf *rxHeaders;            /**< Request received headers */
    RHash *txHeaders;           /**< Output headers */

    //  Parsed request
    cchar *contentType;         /**< Receive content type header value */
    cchar *contentDisposition;  /**< Receive content disposition header value */
    cchar *ext;                 /**< Request URL extension */
    cchar *mime;                /**< Request mime type based on the extension */
    cchar *origin;              /**< Request origin header */
    cchar *protocol;            /**< Request HTTP protocol. Set to HTTP/1.0 or HTTP/1.1 */
    cchar *scheme;              /**< Request HTTP protocol. Set to "http", "https", "ws", or "wss" */
    cchar *upgrade;             /**< Request upgrade to websockets */
    char *query;                /**< Request URL query portion */
    char *redirect;             /**< Response redirect location. Used to set the Location header */
    char *hash;                 /**< Request URL reference portion */
    time_t since;               /**< Value of the if-modified-since value in seconds since epoch */

    //  Auth
    char *cookie;               /**< Request cookie string */
    cchar *username;            /**< Username */
    cchar *role;                /**< Authorized role */
    int roleId;                 /**< Index into roles for the authorized role */
    int signature;              /**< Index into signature definition for this request */
    int64 reuse;                /**< Keep-alive reuse counter */
    int64 conn;                 /**< Web connection sequence */

#if ME_WEB_UPLOAD
    //  Upload
    RHash *uploads;             /**< Table of uploaded files for this request */
    WebUpload *upload;          /**< Current uploading file */
    int numUploads;             /**< Count of uploaded files */
    cchar *uploadDir;           /**< Directory to place uploaded files */
    char *boundary;             /**< Upload file boundary */
    ssize boundaryLen;          /**< Length of the boundary */
#endif
#if ME_COM_WEBSOCKETS
    struct WebSocket *webSocket;/**< Web socket object */
#endif
} Web;

/**
    Add a header to the request response.
    @param web Web object
    @param key HTTP header name
    @param fmt Format string for the header value
    @param ... Format arguments
    @stability Evolving
 */
PUBLIC void webAddHeader(Web *web, cchar *key, cchar *fmt, ...);

/**
    Add a static string header to the request response.
    @description Use to add a static literal string header. This is a higher performance option
        to webAddHeader. The string must be a literal string or a persistent string.
    @param web Web object
    @param key HTTP header name
    @param value Static string for the header value
    @stability Prototype
 */
PUBLIC void webAddHeaderStaticString(Web *web, cchar *key, cchar *value);

/**
    Add a dynamic string header to the request response.
    @description Use this when you have an allocated string and the web server can
    assume responsibility for freeing the string.
    @param web Web object
    @param key HTTP header name
    @param value Dynamic string value. Caller must not free.
    @stability Prototype
 */
PUBLIC void webAddHeaderDynamicString(Web *web, cchar *key, char *value);

/**
    Add an Access-Control-Allow-Origin response header for the request host name.
    @param web Web object
    @stability Evolving
 */
PUBLIC void webAddAccessControlHeader(Web *web);

/**
    Buffer the response body.
    @description This routine will cause webWrite calls to be buffered.
        If the buffer is not allocated, it will be allocated to the given size.
        If the buffer is already allocated, it will be resized if required.
        When webFinalize is called, the content length will be set, the headers will be written,
        and the buffer will be flushed.
    @param web Web object
    @param size Size of the buffer.
    @stability Evolving
 */
PUBLIC void webBuffer(Web *web, ssize size);

/**
    Read data and buffer until a given pattern or limit is reached.
    @description This reads the data into the buffer, but does not return the data or consume it.
    @param web Web object
    @param until Pattern to read until. Set to NULL for no pattern.
    @param limit Number of bytes of data to read.
    @param allowShort Boolean. Set to true to return 0 if the pattern is not found before the limit.
    @return The number of bytes read into the buffer. Return zero if pattern not found and negative for errors.
    @stability Evolving
 */
PUBLIC ssize webBufferUntil(Web *web, cchar *until, ssize limit, bool allowShort);

/**
    Respond to the request with an error.
    @description This responds to the request with the given HTTP status and body data. Use this only when a valid HTTP
       error response can be generated. Use webNetError() for other cases when the HTTP connection is compromised.
    @param web Web object
    @param status HTTP response status code
    @param fmt Body data to send as the response. This is a printf style string.
    @param ... Body response arguments.
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int webError(Web *web, int status, cchar *fmt, ...);

/**
    Extend the request timeout
    @description Request duration is bounded by the timeouts.request and timeouts.inactivity limits.
    You can extend the timeout for a long running request via this call.
    @param web Web object
    @param timeout Timeout in milliseconds use for both the request and inactivity timeouts for this request.
    @stability DEPRECATED: Use webUpdateDeadline() instead.
 */
PUBLIC void webExtendTimeout(Web *web, Ticks timeout);

/**
    Finalize response output.
    @description This routine MUST be called after all output has been written.
        It will ensure that headers are written and that transfer-encoding is finalized.
        For WebSockets this routine is called as part of the WebSockets upgrade by the server.
        This call is idempotent and can be called multiple times.
    @param web Web object
    @return The number of bytes written.
    @stability Evolving
 */
PUBLIC ssize webFinalize(Web *web);

/**
    Get a request header value
    @param web Web object
    @param key HTTP header name. Case does not matter.
    @return Header value or null if not found
    @stability Prototype
 */
PUBLIC cchar *webGetHeader(Web *web, cchar *key);

/**
    Get the next request header in sequence
    @description Used to iterate over all headers
    @param web Web object
    @param pkey Pointer to key. Used to pass in the last key value and return the next key.
        Set to NULL initially.
    @param pvalue Pointer to header value
    @return true if more headers to visit
    @stability Prototype
 */
PUBLIC bool webGetNextHeader(Web *web, cchar **pkey, cchar **pvalue);

/**
    Get the host name of the endpoint serving the request
    @description This will return the WebHost name if defined, otherwise it will use the listening endpoint.
        If all else fails, it will use the socket IP address.
    @param web Web object
    @return Allocated string containing the host name. Caller must free.
    @stability Evolving
 */
PUBLIC char *webGetHostName(Web *web);

/**
    Get the user's role
    @param web Web object
    @return The user's role. The returned reference should only be used short-term and is not
        long-term stable.
    @stability Evolving
 */
PUBLIC cchar *webGetRole(Web *web);

/**
    Get a request variable value from the request form/body
    @param web Web object
    @param name Variable name
    @param defaultValue Default value to return if the variable is not defined
    @return The value of the variable or the default value if not defined.
    @stability Evolving
 */
PUBLIC cchar *webGetVar(Web *web, cchar *name, cchar *defaultValue);

/**
    Close the current request and issue no response
    @description This closes the request connection and issues no response. It should be used when
        a request is received that indicates the connection is compromised.
    @param web Web object
    @param msg Message to the error log. This is a printf style string.
    @param ... Message response arguments.
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int webNetError(Web *web, cchar *msg, ...);

/**
    Parse a cookie header string and return a cookie value.
    @param web Web object
    @param name Cookie name to extract.
    @return The cookie value or NULL if not defined.
    @stability Evolving
 */
PUBLIC char *webParseCookie(Web *web, char *name);

/**
    Parse a URL into its components
    @description The url is parsed into components that are returned via
        the argument references. If a component is not present in the url, the
        reference will be set to NULL.
    @param url Web object
    @param scheme Pointer to scheme portion
    @param host Pointer to host portion
    @param port Pointer to port portion (a string)
    @param path Pointer to path portion
    @param query Pointer to query portion
    @param hash Pointer to hash portion
    @return An allocated storage buffer. Caller must free.
    @stability Prototype
 */
PUBLIC char *webParseUrl(cchar *url,
                         cchar **scheme,
                         cchar **host,
                         int *port,
                         cchar **path,
                         cchar **query,
                         cchar **hash);

/**
    Read request body data.
    @description This routine will read the body data and return the number of bytes read.
        This routine will block the current fiber if necessary. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param web Web object
    @param buf Data buffer to read into
    @param bufsize Size of the buffer
    @return The number of bytes read. Return < 0 for errors and 0 when all the body data has been read.
    @stability Evolving
 */
PUBLIC ssize webRead(Web *web, char *buf, ssize bufsize);

/**
    Read data from the socket into the web->rx buffer.
    @description This routine will read the data from the socket into the web->rx buffer.
    @param web Web object
    @param bufsize Size of the buffer
    @return The number of bytes read. Return < 0 for errors.
    @stability Evolving
 */
PUBLIC ssize webReadSocket(Web *web, ssize bufsize);

/**
    Read request body data until a given pattern is reached.
    @description This routine will read the body data and return the number of bytes read.
        This routine will block the current fiber if necessary. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param web Web object
    @param until Pattern to read until. Set to NULL for no pattern.
    @param buf Data buffer to read into
    @param bufsize Size of the buffer
    @return The number of bytes read. Return < 0 for errors and 0 when all the body data has been read.
    @stability Internal
 */
PUBLIC ssize webReadUntil(Web *web, cchar *until, char *buf, ssize bufsize);

/**
    Redirect the client to a new URL
    @pre Must only be called from a fiber.
    @param web Web object
    @param status HTTP status code. Must set to 301 or 302.
    @param uri URL to redirect the client toward.
    @stability Evolving
 */
PUBLIC void webRedirect(Web *web, int status, cchar *uri);

/**
    Remove a request variable value
    @param web Web object
    @param name Variable name
    @stability Evolving
 */
PUBLIC void webRemoveVar(Web *web, cchar *name);

/**
    Write a file response
    @description This routine will read the contents of the open file descriptor and send as a
    response. This routine will block the current fiber if necessary. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param web Web object
    @param fd File descriptor for an open file or pipe.
    @return The number of bytes written.
    @stability Evolving
 */

PUBLIC ssize webSendFile(Web *web, int fd);

/**
    Set the content length for the response.
    @param web Web object
    @param len Content length.
    @stability Evolving
 */
PUBLIC void webSetContentLength(Web *web, ssize len);

/**
    Set the response HTTP status code
    @param web Web object
    @param status HTTP status code.
    @stability Evolving
 */
PUBLIC void webSetStatus(Web *web, int status);

/**
    Set a request variable value
    @param web Web object
    @param name Variable name
    @param value Value to set.
    @stability Evolving
 */
PUBLIC void webSetVar(Web *web, cchar *name, cchar *value);

/**
    Validate a request body and query with the API signature.
    The path is used as a JSON property path into the signatures.json5 file. It is typically based
    upon the request URL path with "/" characters converted to ".".
    This routine will generate an error response if the signature is not found and strictSignatures is true.
    @param web Web object
    @param path JSON property path into the signatures.json5 file.
    @return True if the request is valid. Otherwise, return false and generate an error response to the client..
    @stability Evolving
 */
PUBLIC bool webValidateRequest(Web *web, cchar *path);

/**
    Validate a JSON object against the API signature.
    @param web Web object
    @param json JSON object
    @return True if the request is valid. Otherwise, return false and generate an error response to the client.
    @stability Evolving
 */
PUBLIC bool webValidateJson(Web *web, const Json *json);

/**
    Validate a data buffer against the API signature.
    @param web Web object
    @param buf Data buffer
    @return True if the request is valid. Otherwise, return false and generate an error response to the client.
    @stability Evolving
 */
PUBLIC bool webValidateData(Web *web, cchar *buf);

/**
    Low level routine to validate a string body against a signature
    @description Use this routine to validate request and response bodies if you cannot use the
        integrated validation or webValidateRequestBody.
    @param web Web object
    @param tag Tag name for the request body. Set to "request", "response" or "query".
    @param body Request body data
    @param signature Signature to validate against
    @return True if the request is valid. Otherwise, return false and generate an error response to the client.
    @stability Evolving
 */
PUBLIC bool webValidateDataSignature(Web *web, cchar *tag, cchar *body, JsonNode *signature);

/**
    Low level routine to validate a JSON body against a signature
    @description Use this routine to validate request and response bodies if you cannot use the
    @param web Web object
    @param tag Tag name for the request body. Set to "request", "response" or "query".
    @param json JSON object
    @param jid Base JSON node ID from which to convert. Set to zero for the top level.
    @param signature Signature to validate against
    @param depth Depth of the JSON object
    @return True if the request is valid. Otherwise, return false and generate an error response to the client.
    @stability Evolving
 */
PUBLIC bool webValidateJsonSignature(Web *web, cchar *tag, const Json *json, int jid, JsonNode *signature, int depth);

/**
    Write response data
    @description This routine will block the current fiber if necessary. Other fibers continue to run.
        This routine will write the response HTTP headers if required.
        Writing a null buffer or zero bufsize finalizes the response and indicates there is no more output.
        The webFinalize API is a convenience call for this purpose.
    @pre Must only be called from a fiber.
    @param web Web object
    @param buf Buffer of data to write.
    @param bufsize Size of the buffer to write.
    @return The number of bytes written.
    @stability Evolving
 */
PUBLIC ssize webWrite(Web *web, cvoid *buf, ssize bufsize);

/**
    Write string response data
    @description This routine will block the current fiber if necessary. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param web Web object
    @param fmt Printf style message string
    @param ... Format arguments.
    @return The number of bytes written.
    @stability Evolving
 */
PUBLIC ssize webWriteFmt(Web *web, cchar *fmt, ...);

/**
    Write data from a JSON object
    @description This routine will block the current fiber if necessary. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param web Web object
    @param json JSON object
    @return The number of bytes written.
    @stability Evolving
 */
PUBLIC ssize webWriteJson(Web *web, const Json *json);

/**
    Write request response headers
    @description This will write the HTTP response headers. This writes the supplied headers and any required headers if
       not supplied.
        This routine will block the current fiber if necessary. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param web Web object
    @return The number of bytes written.
    @stability Evolving
 */
PUBLIC ssize webWriteHeaders(Web *web);

/**
    Write a response
    @description This routine writes a single plain text response and finalizes the
        response in one call.  If status is zero, set the status to 400 and close the
        socket after issuing the response.  It will block the current fiber if necessary.
        Other fibers continue to run. This will set the Content-Type header to text/plain.
    @pre Must only be called from a fiber.
    @param web Web object
    @param status HTTP status code.
    @param fmt Printf style message string
    @param ... Format arguments.
    @return The number of bytes written.
    @stability Evolving
 */
PUBLIC ssize webWriteResponse(Web *web, int status, cchar *fmt, ...);

/**
    Write an SSE event to the client
    @param web Web object
    @param id Event ID
    @param name Event name
    @param fmt Printf style message string
    @param ... Format arguments.
    @stability Evolving
 */
PUBLIC ssize webWriteEvent(Web *web, int64 id, cchar *name, cchar *fmt, ...);

/**
    Write response data from a JSON object and validate against the API signature
    @description This routine only removes discard fields and does not validate response data types.
    This routine will block the current fiber if necessary. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param web Web object
    @param json JSON object
    @return The number of bytes written, or -1 for errors.
    @stability Evolving
 */
PUBLIC ssize webWriteValidatedJson(Web *web, const Json *json);

/**
    Write a buffer with a validated signature
    @pre Must only be called from a fiber.
    @param web Web object
    @param buf Buffer of data to write.
    @return The number of bytes written.
    @stability Evolving
 */
PUBLIC ssize webWriteValidatedData(Web *web, cchar *buf);

/*
    Internal APIs
 */
PUBLIC void webAddStandardHeaders(Web *web);
PUBLIC int webAlloc(WebListen *listen, RSocket *sock);
PUBLIC bool webCheckSignature(Web *web, Json *json, int nid, JsonNode *signature, int depth);
PUBLIC int webConsumeInput(Web *web);
PUBLIC int webFileHandler(Web *web);
PUBLIC void webFree(Web *web);
PUBLIC void webClose(Web *web);
PUBLIC void webParseForm(Web *web);
PUBLIC void webParseQuery(Web *web);
PUBLIC void webParseEncoded(Web *web, Json *vars, cchar *str);
PUBLIC Json *webParseJson(Web *web);
PUBLIC bool webParseHeadersBlock(Web *web, char *headers, ssize headersSize, bool upload);
PUBLIC int webReadBody(Web *web);
PUBLIC void webTestInit(WebHost *host, cchar *prefix);
PUBLIC void webUpdateDeadline(Web *web);
PUBLIC int webValidateUrl(Web *web);

/************************************ Session *********************************/

typedef struct WebSession {
    char *id;                              /**< Session ID key */
    int lifespan;                          /**< Session inactivity timeout (secs) */
    Ticks expires;                         /**< When the session expires */
    RHash *cache;                          /**< Cache of session variables */
} WebSession;

/**
    Create a login session
    @param web Web request object
    @return Allocated session object
    @stability Evolving
 */
PUBLIC WebSession *webCreateSession(Web *web);

/**
    Destroy the web session object
    @description Useful to be called as part of the user logout process
    @param web Web request object
    @stability Prototype
 */
PUBLIC void webDestroySession(Web *web);

/**
    Get the session state object for the current request
    @param web Web request object
    @param create Set to true to create a new session if one does not already exist.
    @return Session object
    @stability Evolving
 */
PUBLIC WebSession *webGetSession(Web *web, int create);

/**
    Get a session variable
    @param web Web request object
    @param name Session variable name
    @param defaultValue Default value to return if the variable does not exist
    @return Session variable value or default value if it does not exist
    @stability Evolving
 */
PUBLIC cchar *webGetSessionVar(Web *web, cchar *name, cchar *defaultValue);

/**
    Remove a session variable
    @param web Web request object
    @param name Session variable name
    @stability Evolving
 */
PUBLIC void webRemoveSessionVar(Web *web, cchar *name);

/**
    Set a session variable name value
    @param web Web request object
    @param name Session variable name
    @param fmt Format string for the value
    @param ... Format args
    @return The value set for the variable. Caller must not free.
    @stability Evolving
 */
PUBLIC cchar *webSetSessionVar(Web *web, cchar *name, cchar *fmt, ...);

//  Internal
PUBLIC int webInitSessions(WebHost *host);
PUBLIC void webFreeSession(WebSession *sp);

/************************************* Auth ***********************************/
/**
    Authenticate a user
    @description The user is authenticated if required by the selected request route.
    @param web Web request object
    @return True if the route does not require authentication or the user is authenticated successfully.
    @stability Evolving
 */
PUBLIC bool webAuthenticate(Web *web);

/**
    Test if a user possesses the required role
    @param web Web request object
    @param role Required role.
    @return True if the user has the required role.
    @stability Evolving
 */
PUBLIC bool webCan(Web *web, cchar *role);

/**
    Test if the user has been authenticated.
    @param web Web request object
    @return True if the user has been authenticated.
    @stability Evolving
 */
PUBLIC bool webIsAuthenticated(Web *web);

/**
    Login a user by creating session state. Assumes the caller has already authenticated and authorized the user.
    @param web Web request object
    @param username User name
    @param role Requested role
    @return True if the login is successful
    @stability Evolving
 */
PUBLIC bool webLogin(Web *web, cchar *username, cchar *role);

/**
    Logout a user and remove the user login session.
    @param web Web request object
    @stability Evolving
 */
PUBLIC void webLogout(Web *web);

/**
    Define a request hook
    @description The request hook will be invoked for important request events during the lifecycle of processing the
        request.
    @param host WebHost object
    @param hook Callback hook function
    @stability Evolving
 */
PUBLIC void webSetHook(WebHost *host, WebHook hook);

//  Internal
PUBLIC int webHook(Web *web, int event);

/********************************* Web Sockets ********************************/
#if ME_COM_WEBSOCKETS
/**
    Upgrade a HTTP connection connection to use WebSockets
    @description This responds to a request to upgrade the connection use WebSockets.
    This routine will be invoked automatically when the WebSocket upgrade request is received.
    Uses should not call this routine directly.
    @param web Web object
    @return Return Zero if the connection upgrade can be requested.
    @stability Prototype
    @internal
 */
PUBLIC int webUpgradeSocket(Web *web);
PUBLIC void webAsync(Web *web, WebSocketProc callback, void *arg);
PUBLIC int webWait(Web *web);
#endif /* ME_COM_WEBSOCKETS */

/************************************ Misc ************************************/

/**
    Convert a time to a date string
    @param buf Buffer to hold the generated date string
    @param when Timestamp to convert
    @return A reference to the buffer
    @stability Evolving
 */
PUBLIC char *webDate(char *buf, time_t when);

/**
    Decode a URL
    @description The string is converted in-situ.
    @param str String to decode
    @return The original string reference.
    @stability Evolving
 */
PUBLIC char *webDecode(char *str);

/**
    Encode a URL
    @description The string is converted in-situ.
    @param uri Uri to encode.
    @return An allocated, escaped URI. Caller must free.
    @stability Evolving
 */
PUBLIC char *webEncode(cchar *uri);

/**
    Escape HTML
    @description The string is converted in-situ.
    @param html Html to escape.
    @return An allocated, escaped HTML. Caller must free.
    @stability Evolving
 */
PUBLIC char *webEscapeHtml(cchar *html);

/**
    Get a status message corresponding to a HTTP status code.
    @param status HTTP status code.
    @return A status message. Caller must not free.
    @stability Evolving
 */
PUBLIC cchar *webGetStatusMsg(int status);

/**
    Normalize a URL path.
    @description Normalize a path to remove "./",  "../" and redundant separators. This does not make an absolute path
        and does not map separators or change case. This validates the path and expects it to begin with "/".
    @param path Path string to normalize.
    @return An allocated path. Caller must free.
    @stability Evolving
 */
PUBLIC char *webNormalizePath(cchar *path);

/**
    Validate a controller/action against the API signatures.
    @description This routine will check the request controller and action against the API signatures.
        If the request is valid, it will return true. Otherwise, it will return false.
    @param web Web object
    @param controller Controller name
    @param action Action name
    @return True if the request is valid.
    @stability Evolving
 */
PUBLIC bool webValidateControllerAction(Web *web, cchar *controller, cchar *action);

//  DEPRECATED - use webValidateControllerAction() instead
#define webCheckRequest webValidateControllerAction

/**
    Validate a URL
    @description Check a url for invalid characters.
    @param uri Url path.
    @return True if the url contains only valid characters.
    @stability Evolving
 */
PUBLIC bool webValidatePath(cchar *uri);

#if ME_COM_SSL
/* Internal */
PUBLIC int webSecureEndpoint(WebListen *listen);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ME_COM_WEB */
#endif /* _h_WEB */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
