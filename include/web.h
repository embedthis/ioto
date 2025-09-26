/**
    @file web.h
    @brief Fast, secure, tiny web server for embedded applications
    @description The Web Server Module provides a high-performance, secure web server designed
        for embedded applications. Features include HTTP/1.0 and HTTP/1.1 support, TLS/SSL encryption,
        WebSocket support, SSE (Server-Sent Events), file upload/download capabilities, session management
        with XSRF protection, comprehensive input validation and sanitization, configurable request/response
        limits, flexible routing system, and the ability to invoke C functions bound to URL routes.

        The web server is designed for embedded IoT applications and integrates tightly with the
        Safe Runtime (r module) for memory management, fiber coroutines for concurrency, JSON5
        configuration parsing, cryptographic functions, and WebSocket protocol support.

        Key architectural features:
        - Single-threaded with fiber coroutines for concurrency
        - Null-tolerant APIs that gracefully handle NULL arguments
        - Cross-platform support (Linux, macOS, Windows/WSL, ESP32, FreeRTOS)
        - Modular design with minimal interdependencies
        - Professional HTML documentation generation via Doxygen

    Copyright (c) All Rights Reserved. See details at the end of the file.
    @stability Evolving
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

/**
 * @name Feature Control Macros
 * @description Compile-time feature configuration macros for the Web Server Module.
 *     These macros control which features are included in the build.
 * @{
 */
#ifndef ME_WEB_AUTH
    #define ME_WEB_AUTH        1        /**< Enable authentication and authorization support */
#endif
#ifndef ME_WEB_LIMITS
    #define ME_WEB_LIMITS      1        /**< Enable resource limits and security constraints */
#endif
#ifndef ME_WEB_SESSIONS
    #define ME_WEB_SESSIONS    1        /**< Enable session management support */
#endif
#ifndef ME_WEB_UPLOAD
    #define ME_WEB_UPLOAD      1        /**< Enable file upload functionality */
#endif
#ifndef ME_COM_WEBSOCKETS
    #define ME_COM_WEBSOCKETS  1        /**< Enable WebSocket protocol support */
#endif
/** @} */

/**
 * @name Web Server Constants
 * @description Core constants used throughout the Web Server Module.
 * @{
 */
#define WEB_MAX_SIG            160      /**< Maximum size of controller.method URL portion in API signatures */

/*
    Dependencies
 */
/**
 * @name Configuration Defaults
 * @description Default configuration values for the Web Server Module.
 * @{
 */
#ifndef ME_WEB_CONFIG
    #define ME_WEB_CONFIG      "web.json5"  /**< Default configuration file name */
#endif
#ifndef WEB_SESSION_COOKIE
    #define WEB_SESSION_COOKIE "-web-session-"  /**< Default session cookie name */
#endif
/** @} */

/**
 * @name Session State Variables
 * @description Internal session variable names for storing authentication state.
 * @{
 */
#define WEB_SESSION_USERNAME   "_:username:_"       /**< Session state username variable */
#define WEB_SESSION_ROLE       "_:role:_"           /**< Session state role variable */
#define WEB_SESSION_XSRF       "_:xsrf:_"           /**< Session state XSRF token variable */
/** @} */

/**
 * @name XSRF Protection
 * @description Cross-Site Request Forgery protection configuration.
 * @{
 */
#ifndef WEB_XSRF_HEADER
    #define WEB_XSRF_HEADER    "X-XSRF-TOKEN"       /**< CSRF token name in HTTP headers */
#endif
#ifndef WEB_XSRF_PARAM
    #define WEB_XSRF_PARAM     "-xsrf-"             /**< CSRF parameter in form fields */
#endif
/** @} */

#define WEB_UNLIMITED          MAXINT64    /**< Value indicating unlimited resource usage */

/**
 * @name HTTP Chunk Processing States
 * @description State flags for HTTP chunked transfer encoding processing.
 * @{
 */
#define WEB_CHUNK_START        1        /**< Start of a new chunk */
#define WEB_CHUNK_DATA         2        /**< Start of chunk data */
#define WEB_CHUNK_EOF          4        /**< End of chunk data */
/** @} */

#define WEB_HEADERS            16       /**< Initial number of header slots to allocate */
/** @} */

/*
    Forward declare
 */

/**
    Embedded web server forward declarations
    @description The web server provides a fast, secure, tiny web server for embedded applications.
        It requires the use of fibers from the portable runtime for concurrency. The web server supports
        HTTP/1.0, HTTP/1.1, TLS/SSL, WebSockets, file uploads, session management, and comprehensive
        security features including input validation and XSRF protection.
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
    @description Callback function signature for action routines that handle HTTP requests.
        Action routines are C functions bound to URL prefixes and invoked when matching requests are received.
    @param web Web request object containing all request state and response capabilities
    @stability Evolving
 */
typedef void (*WebProc)(struct Web *web);

/**
    WebHook callback procedure
    @description Callback function signature for web server event hooks. Hooks are invoked at
        important points during request processing to allow custom request handling and monitoring.
    @param web Web request object
    @param event Event identifier (WEB_HOOK_CONNECT, WEB_HOOK_START, WEB_HOOK_RUN, etc.)
    @return Zero to continue normal processing, non-zero to override default behavior
    @stability Evolving
 */
typedef int (*WebHook)(struct Web *web, int event);

/**
    Action function bound to a URL prefix
    @description Structure defining an action that maps URL prefixes to C callback functions.
        Actions enable the web server to invoke specific C functions when requests match configured URL patterns.
    @stability Evolving
 */
typedef struct WebAction {
    char *role;                         /**< Role to invoke action */
    char *match;                        /**< Path prefix */
    WebProc fn;                         /**< Function to invoke */
} WebAction;

/**
    Routing object to match a request against a path prefix
    @description Route configuration that defines how incoming HTTP requests are processed.
        Routes specify URL patterns, HTTP methods, authentication requirements, validation rules,
        and processing directives for matching requests.
    @stability Evolving
 */
typedef struct WebRoute {
    cchar *match;                       /**< Matching URI path pattern */
    bool validate : 1;                  /**< Validate request */
    bool exact : 1;                     /**< Exact match vs prefix match. If trailing "/" in route. */
    bool xsrf : 1;                      /**< Use XSRF tokens */
    RHash *methods;                     /**< HTTP methods verbs */
    cchar *handler;                     /**< Request handler (file, action) */
    cchar *role;                        /**< Required user role */
    cchar *redirect;                    /**< Redirection */
    cchar *trim;                        /**< Portion to trim from path */
    bool stream;                        /**< Stream request body */
} WebRoute;

/**
    Site-wide URL redirection configuration
    @description Defines a URL redirection rule that automatically redirects requests from
        one URL path to another with a specified HTTP status code.
    @stability Evolving
 */
typedef struct WebRedirect {
    cchar *from;                        /**< Original URL path */
    cchar *to;                          /**< Target URL */
    int status;                         /**< Redirection HTTP status code */
} WebRedirect;

/**
    Initialize the web module
    @description Initialize the web module and its dependencies. This function must be called
        before using any other web module functions. It sets up internal data structures,
        initializes the TLS subsystem if enabled, and prepares the module for operation.
    @return Zero if successful, otherwise a negative error code
    @stability Evolving
 */
PUBLIC int webInit(void);

/**
    Terminate the web module
    @description Clean up and terminate the web module. This function should be called when
        the web module is no longer needed. It releases all allocated resources, closes any
        open connections, and performs cleanup operations.
    @stability Evolving
 */
PUBLIC void webTerm(void);

/************************************* Host ***********************************/
/**
    Web host structure
    @description The web host defines a complete web server instance with its configuration,
        listeners, active connections, and runtime state. Multiple web hosts can be created to
        serve different virtual hosts or listening endpoints. The host contains all the routing rules,
        security settings, session management, and operational parameters for the web server.
    @stability Evolving
 */
typedef struct WebHost {
    RList *listeners;           /**< List of WebListen objects - listening endpoints for this host */
    RList *webs;                /**< List of active Web request objects currently being processed */
    Json *config;               /**< JSON5 configuration object containing all host settings */
    Json *signatures;           /**< API signatures for request/response validation */

    int flags;                  /**< Host control flags for debugging and operation modes */
    bool freeConfig : 1;        /**< True if config object was allocated and must be freed */
    bool httpOnly : 1;          /**< Default HttpOnly flag for session cookies */
    bool strictSignatures : 1;  /**< Enforce strict API signature compliance for validation */

    WebHook hook;               /**< Event notification callback function */
    RHash *users;               /**< Hash table of authenticated users and their credentials */
    RHash *sessions;            /**< Hash table of active client sessions indexed by session ID */
    RHash *methods;             /**< Supported HTTP method verbs (GET, POST, PUT, DELETE, etc.) */
    RHash *mimeTypes;           /**< MIME type mappings indexed by file extension */
    RList *actions;             /**< Ordered list of WebAction objects for URL-to-function bindings */
    RList *routes;              /**< Ordered list of WebRoute objects for request routing */
    RList *redirects;           /**< Ordered list of WebRedirect objects for URL redirections */
    REvent sessionEvent;        /**< Session timer event */
    int roles;                  /**< Base ID of roles in config */
    int headers;                /**< Base ID for headers in config */

    cchar *name;                /**< Host name for canonical redirects and URL generation */
    cchar *index;               /**< Default index file (e.g., "index.html") for directory requests */
    cchar *sameSite;            /**< SameSite cookie attribute ("strict", "lax", or "none") */
    cchar *sessionCookie;       /**< Cookie name used for session state storage */
    char *docs;                 /**< Document root directory path for serving static files */
    char *ip;                   /**< Default IP address for redirects when host IP is indeterminate */

#if ME_WEB_UPLOAD
    //  Upload configuration
    cchar *uploadDir;           /**< Directory path where uploaded files are temporarily stored */
    bool removeUploads : 1;     /**< Automatically remove uploaded files when request completes */
#endif
    //  Timeout configuration (in seconds)
    int inactivityTimeout;      /**< Maximum seconds of inactivity before closing connection */
    int parseTimeout;           /**< Maximum seconds allowed for parsing HTTP request headers */
    int requestTimeout;         /**< Maximum seconds for complete request processing */
    int sessionTimeout;         /**< Maximum seconds of inactivity before session expires */

    ssize connections;          /**< Current count of active client connections */

    //  Security and resource limits
#if ME_WEB_LIMITS || DOXYGEN
    int64 maxBuffer;            /**< Maximum response buffer size in bytes */
    int64 maxHeader;            /**< Maximum HTTP header size in bytes */
    int64 maxConnections;       /**< Maximum number of simultaneous connections */
    int64 maxBody;              /**< Maximum HTTP request body size in bytes */
    int64 maxSessions;          /**< Maximum number of concurrent user sessions */
    int64 maxUpload;            /**< Maximum file upload size in bytes */
    int64 maxUploads;           /**< Maximum number of files per upload request */

#if ME_COM_WEBSOCKETS
    cchar *webSocketsProtocol;  /**< WebSocket application sub-protocol identifier */
    int64 webSocketsMaxMessage; /**< Maximum WebSocket message size in bytes */
    int64 webSocketsMaxFrame;   /**< Maximum WebSocket frame size in bytes */
    int64 webSocketsPingPeriod; /**< WebSocket ping period in milliseconds */
    bool webSocketsValidateUTF; /**< Validate UTF-8 encoding in WebSocket text frames */
    bool webSocketsEnable;      /**< Enable WebSocket protocol support */
#endif /* ME_COM_WEBSOCKETS */
#endif /* ME_WEB_LIMITS */
} WebHost;

/**
    Add an action callback for a URL prefix
    @description Register a C function to be invoked for HTTP requests matching a specific URL prefix.
        The action function will be called for any request whose URL path starts with the specified prefix.
        Actions provide a simple way to bind C code directly to URL routes without complex routing configuration.
        The specified role, if provided, will be used for authorization checking before invoking the action.
    @param host Web host object that will own this action
    @param prefix URL path prefix to match (e.g., "/api/", "/admin")
    @param fn WebProc callback function to invoke for matching requests
    @param role Required user role for authorization, or NULL for no role requirement
    @stability Evolving
 */
PUBLIC void webAddAction(WebHost *host, cchar *prefix, WebProc fn, cchar *role);

/**
 * @name Debug Tracing Flags
 * @description Flags for webAllocHost() to control debug tracing output.
 *     These flags can be combined using bitwise OR to enable multiple trace types.
 * @{
 */
#define WEB_SHOW_NONE         0x1   /**< Trace nothing (disable all tracing) */
#define WEB_SHOW_REQ_BODY     0x2   /**< Trace HTTP request body content */
#define WEB_SHOW_REQ_HEADERS  0x4   /**< Trace HTTP request headers */
#define WEB_SHOW_RESP_BODY    0x8   /**< Trace HTTP response body content */
#define WEB_SHOW_RESP_HEADERS 0x10  /**< Trace HTTP response headers */
/** @} */

/**
    Allocate a new host object
    @description Create and initialize a new web host with the specified configuration.
        The host will be configured according to the provided JSON5 configuration object.
        After allocation, the host can be started with webStartHost() to begin accepting requests.
    @param config JSON5 configuration object containing host settings, or NULL for defaults
    @param flags Debug tracing flags. Combine WEB_SHOW_NONE, WEB_SHOW_REQ_BODY, WEB_SHOW_REQ_HEADERS,
        WEB_SHOW_RESP_BODY, WEB_SHOW_RESP_HEADERS to control request/response tracing
    @return Allocated WebHost object, or NULL on allocation failure
    @stability Evolving
 */
PUBLIC WebHost *webAllocHost(Json *config, int flags);

/**
    Free a host object
    @description Release all resources associated with a web host and deallocate the host object.
        This will close all active connections, free all sessions, and cleanup all allocated memory.
        The host should be stopped with webStopHost() before calling this function.
    @param host Web host object to free
    @stability Evolving
 */
PUBLIC void webFreeHost(WebHost *host);

/**
    Get the web documents directory for a host
    @description Retrieve the document root directory path where static files are served from.
        This directory is configured via the web.documents property in the host configuration.
    @param host Web host object
    @return Document root directory path, or NULL if not configured
    @stability Evolving
 */

PUBLIC cchar *webGetDocs(WebHost *host);

/**
    Set the default IP address for the host
    @description Configure the default IP address to use in redirects and URL generation
        when the host's IP address cannot be determined from the listening socket or request headers.
    @param host Web host object
    @param ip Default IP address string (e.g., "192.168.1.100")
    @stability Evolving
 */
PUBLIC void webSetHostDefaultIP(WebHost *host, cchar *ip);

/**
    Start listening for requests on the host
    @description Begin accepting HTTP connections on all configured listening endpoints.
        This creates socket listeners based on the host configuration and starts the request
        processing loop. The function will block until webStopHost() is called.
    @pre Must only be called from a fiber
    @param host Web host object to start
    @return Zero if successful, otherwise a negative error code
    @stability Evolving
 */
PUBLIC int webStartHost(WebHost *host);

/**
    Stop listening for requests on the host
    @description Stop accepting new connections and gracefully shutdown all listening endpoints.
        Existing connections will be allowed to complete their current requests before being closed.
        This function will cause webStartHost() to return.
    @pre Must only be called from a fiber
    @param host Web host object to stop
    @stability Evolving
 */
PUBLIC void webStopHost(WebHost *host);

/************************************ Upload **********************************/
#if ME_WEB_UPLOAD || DOXYGEN
/**
    File upload structure
    @description Represents a single file uploaded via HTTP multipart/form-data.
        Contains metadata about the uploaded file including original filename, content type,
        and temporary storage location. Upload files are automatically cleaned up when
        the request completes unless explicitly preserved.
    @stability Evolving
 */
typedef struct WebUpload {
    char *filename;       /**< Temporary filename on server where uploaded content is stored */
    char *clientFilename; /**< Original filename as provided by the client */
    char *contentType;    /**< MIME content type of the uploaded file */
    char *name;           /**< Form field name associated with this upload */
    ssize size;           /**< Total size of uploaded file in bytes */
    int fd;               /**< File descriptor for the temporary upload file (internal use) */
} WebUpload;

/*
    Internal APIs
 */
PUBLIC int webInitUpload(struct Web *web);
PUBLIC void webFreeUpload(struct Web *web);
PUBLIC int webProcessUpload(struct Web *web);
#endif

/*************************************** Web **********************************/
/**
 * @name Web Hook Event Types
 * @description Event types passed to WebHook callback functions during request processing.
 *     These events allow custom handling at important points in the request lifecycle.
 * @{
 */
#define WEB_HOOK_CONNECT    1 /**< New socket connection established */
#define WEB_HOOK_DISCONNECT 2 /**< Socket connection being closed */
#define WEB_HOOK_START      3 /**< New HTTP request started */
#define WEB_HOOK_RUN        4 /**< Ready to run request or custom request processing */
#define WEB_HOOK_ACTION     5 /**< About to invoke an action callback */
#define WEB_HOOK_NOT_FOUND  6 /**< Requested document/resource not found */
#define WEB_HOOK_ERROR      7 /**< Request processing error occurred */
#define WEB_HOOK_CLOSE      8 /**< WebSocket connection being closed */
#define WEB_HOOK_END        9 /**< End of request processing */
/** @} */

typedef struct WebListen {
    RSocket *sock;            /**< Socket */
    char *endpoint;           /**< Endpoint definition */
    int port;                 /**< Listening port */
    WebHost *host;            /**< Host owning this listener */
} WebListen;

/**
    Web request object
    @description The main request/response object representing an individual HTTP transaction.
        Contains all request state, parsed headers and body, response buffers, and processing context.
        Each Web object handles one complete HTTP request/response cycle and provides the primary
        API for reading request data and generating responses.
    @stability Evolving
 */
typedef struct Web {
    char *error;                /**< Error message string for request processing errors */
    cchar *method;              /**< HTTP request method in uppercase (GET, POST, PUT, DELETE, etc.) */
    char *url;                  /**< Complete request URL including query string */
    char *path;                 /**< URL path portion without query string or fragment */

    RBuf *body;                 /**< Parsed request body data (POST/PUT content) */
    RBuf *rx;                   /**< Raw incoming data buffer for request parsing */
    // RBuf *trace;                /**< Packet trace buffer for debugging */
    RBuf *buffer;               /**< Response output buffer for efficient response generation */

    Offset chunkRemaining;      /**< Bytes remaining in current HTTP chunk */
    ssize rxLen;                /**< Total expected request content length */
    Offset rxRemaining;         /**< Request body bytes remaining to be read */
    ssize txLen;                /**< Response content length for Content-Length header */
    Offset txRemaining;         /**< Response body bytes remaining to be sent */
    ssize lastEventId;          /**< Last Server-Sent Events (SSE) event identifier */

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

    uint del : 1;               /**< Is the current request a DELETE request */
    uint get : 1;               /**< Is the current request a GET request */
    uint head : 1;              /**< Is the current request a HEAD request */
    uint options : 1;           /**< Is the current request an OPTIONS request */
    uint post : 1;              /**< Is the current request a POST request */
    uint put : 1;               /**< Is the current request a PUT request */
    uint trace : 1;             /**< Is the current request a TRACE request */

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
    char *securityToken;        /**< Request security token */
    time_t since;               /**< Value of the if-modified-since value in seconds since epoch */

    //  Auth
    char *cookie;               /**< Request cookie string. Multiple cookies are joined and separated by ";" */
    cchar *username;            /**< Username */
    cchar *role;                /**< Authorized role */
    int roleId;                 /**< Index into roles for the authorized role */
    int signature;              /**< Index into host->signatures for this request */
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
    Add a header to the request response
    @description Add a HTTP response header with a formatted value. The header will be sent
        to the client when the response headers are written. Multiple headers with the same
        name will be combined according to HTTP standards.
    @param web Web request object
    @param key HTTP header name (case-insensitive)
    @param fmt Printf-style format string for the header value
    @param ... Arguments for the format string
    @stability Evolving
 */
PUBLIC void webAddHeader(Web *web, cchar *key, cchar *fmt, ...);

/**
    Add a static string header to the request response
    @description Add a HTTP response header using a static string value. This is a higher
        performance alternative to webAddHeader() when the header value is a compile-time
        constant or persistent string that does not need to be copied.
    @param web Web request object
    @param key HTTP header name (case-insensitive)
    @param value Static string value for the header (must be persistent)
    @stability Evolving
 */
PUBLIC void webAddHeaderStaticString(Web *web, cchar *key, cchar *value);

/**
    Add a dynamic string header to the request response
    @description Add a HTTP response header with a dynamically allocated string value.
        The web server takes ownership of the string and will free it automatically.
        Use this when you have an allocated string that can be transferred to the web server.
    @param web Web request object
    @param key HTTP header name (case-insensitive)
    @param value Dynamically allocated string value (ownership transfers to web server)
    @stability Prototype
 */
PUBLIC void webAddHeaderDynamicString(Web *web, cchar *key, char *value);

/**
    Add an Access-Control-Allow-Origin response header for the request host name
    @description Add a CORS (Cross-Origin Resource Sharing) Access-Control-Allow-Origin header
        using the current request's host name. This enables cross-origin requests from the
        requesting host while maintaining security.
    @param web Web request object
    @stability Evolving
 */
PUBLIC void webAddAccessControlHeader(Web *web);

/**
    Buffer the response body
    @description Enable response buffering to improve performance and allow automatic
        Content-Length header generation. All subsequent webWrite() calls will accumulate
        data in the buffer instead of sending immediately. When webFinalize() is called,
        the complete response will be sent with proper Content-Length header.
    @param web Web request object
    @param size Initial buffer size in bytes (will grow automatically if needed)
    @stability Evolving
 */
PUBLIC void webBuffer(Web *web, ssize size);

/**
    Read data and buffer until a given pattern or limit is reached
    @description Read data from the request stream into an internal buffer until a specific
        pattern is found or a byte limit is reached. The data remains in the buffer for
        subsequent processing and is not consumed by this call.
    @param web Web request object
    @param until Pattern string to search for, or NULL to read only up to the limit
    @param limit Maximum number of bytes to buffer
    @param allowShort If true, return 0 when pattern not found before limit; if false, continue reading
    @return Number of bytes read into buffer, 0 if pattern not found (when allowShort=true), negative on error
    @stability Evolving
 */
PUBLIC ssize webBufferUntil(Web *web, cchar *until, ssize limit, bool allowShort);

/**
    Respond to the request with an error
    @description Generate a complete HTTP error response with the specified status code and message.
        This function sets the response status, adds appropriate headers, writes the error message
        as the response body, and finalizes the response. Use this only when a valid HTTP error
        response can be generated. Use webNetError() when the HTTP connection is compromised.
    @param web Web request object
    @param status HTTP response status code (e.g., 400, 404, 500)
    @param fmt Printf-style format string for the error message body
    @param ... Arguments for the format string
    @return Zero if successful, negative on failure
    @stability Evolving
 */
PUBLIC int webError(Web *web, int status, cchar *fmt, ...);

/**
    Extend the request timeout
    @description Extend the timeout values for a long-running request. Request duration is
        bounded by the configured request and inactivity timeout limits. This function allows
        extending both timeouts for the current request.
    @param web Web request object
    @param timeout Timeout value in milliseconds for both request and inactivity timeouts
    @stability DEPRECATED: Use webUpdateDeadline() instead
 */
PUBLIC void webExtendTimeout(Web *web, Ticks timeout);

/**
    Finalize response output
    @description Complete the HTTP response by writing any pending headers and finalizing
        the response body. This function MUST be called after all response content has been
        written. It ensures proper HTTP protocol compliance including Content-Length headers
        and transfer-encoding termination. The call is idempotent and safe to call multiple times.
    @param web Web request object
    @return Number of bytes written during finalization
    @stability Evolving
 */
PUBLIC ssize webFinalize(Web *web);

/**
    Get a request cookie value
    @description Extract a specific cookie value from the request Cookie header.
        Parses the Cookie header and returns the value for the named cookie.
    @param web Web request object
    @param name Cookie name to look up
    @return Allocated cookie value string, or NULL if not found. Caller must free.
    @stability Prototype
 */
PUBLIC char *webGetCookie(Web *web, cchar *name);

/**
    Get a request header value
    @description Retrieve the value of a specific HTTP request header. Header name
        matching is case-insensitive per HTTP standards.
    @param web Web request object
    @param key HTTP header name (case-insensitive)
    @return Header value string, or NULL if header not found
    @stability Evolving
 */
PUBLIC cchar *webGetHeader(Web *web, cchar *key);

/**
    Get the next request header in sequence
    @description Iterate through all HTTP request headers. Call repeatedly to enumerate
        all headers in the request. Set *pkey to NULL initially to start iteration.
    @param web Web request object
    @param pkey Pointer to header name pointer (input/output for iteration state)
    @param pvalue Pointer to receive header value pointer
    @return True if a header was returned, false when iteration is complete
    @stability Evolving
 */
PUBLIC bool webGetNextHeader(Web *web, cchar **pkey, cchar **pvalue);

/**
    Get the host name of the endpoint serving the request
    @description Determine the host name for the current request. Returns the configured
        WebHost name if available, otherwise uses the listening endpoint address, or
        falls back to the socket IP address.
    @param web Web request object
    @return Newly allocated string containing the host name. Caller must free.
    @stability Evolving
 */
PUBLIC char *webGetHostName(Web *web);

/**
    Get the authenticated user's role
    @description Retrieve the role of the currently authenticated user. The role is established
        during authentication and stored in the session state. Returns NULL if no user is
        authenticated or no role is assigned.
    @param web Web request object
    @return User's role string, or NULL if not authenticated. Reference is not long-term stable.
    @stability Evolving
 */
PUBLIC cchar *webGetRole(Web *web);

/**
    Get a request variable value from the request form/body
    @description Retrieve a form variable from the parsed request body or query string.
        Variables are parsed from URL-encoded form data (POST) or query parameters (GET).
        JSON request bodies are also parsed and made available as variables.
    @param web Web request object
    @param name Variable name to look up
    @param defaultValue Default value to return if variable is not defined
    @return Variable value string, or defaultValue if not found
    @stability Evolving
 */
PUBLIC cchar *webGetVar(Web *web, cchar *name, cchar *defaultValue);

/**
    Close the current request and issue no response
    @description Immediately close the connection without sending any HTTP response.
        Use this when the connection or request is compromised and no valid HTTP response
        can be generated. The error message is logged for debugging purposes.
    @param web Web request object
    @param msg Printf-style error message for logging
    @param ... Arguments for the error message format string
    @return Zero if successful
    @stability Evolving
 */
PUBLIC int webNetError(Web *web, cchar *msg, ...);

/**
    Parse a cookie header string and return a cookie value
    @description Parse the HTTP Cookie header and extract the value for a specific cookie.
        This is a lower-level function; consider using webGetCookie() for most use cases.
    @param web Web request object
    @param name Cookie name to extract
    @return Allocated cookie value string, or NULL if not found. Caller must free.
    @stability Evolving
 */
PUBLIC char *webParseCookie(Web *web, cchar *name);

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
    @stability Evolving
 */
PUBLIC char *webParseUrl(cchar *url,
                         cchar **scheme,
                         cchar **host,
                         int *port,
                         cchar **path,
                         cchar **query,
                         cchar **hash);

/**
    Read request body data
    @description Read data from the HTTP request body into the provided buffer. This function
        handles chunked transfer encoding and content length limits automatically. The function
        will yield the current fiber if data is not immediately available, allowing other
        fibers to continue execution.
    @pre Must only be called from a fiber
    @param web Web request object
    @param buf Buffer to receive the request body data
    @param bufsize Maximum number of bytes to read
    @return Number of bytes read, 0 when all body data consumed, or negative on error
    @stability Evolving
 */
PUBLIC ssize webRead(Web *web, char *buf, ssize bufsize);

/**
    Read data from the socket into the receive buffer
    @description Read raw data from the client socket into the internal receive buffer.
        This is a low-level function used internally for HTTP request parsing.
    @param web Web request object
    @param bufsize Maximum number of bytes to read from the socket
    @return Number of bytes read from socket, or negative on error
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
    @description Send an HTTP redirect response to the client. Sets the Location header
        and appropriate status code to instruct the client to request a different URL.
    @pre Must only be called from a fiber
    @param web Web request object
    @param status HTTP redirect status code (301 for permanent, 302 for temporary)
    @param uri Target URL for the redirect
    @stability Evolving
 */
PUBLIC void webRedirect(Web *web, int status, cchar *uri);

/**
    Remove a request variable
    @description Remove a variable from the request's variable collection. This affects
        variables parsed from form data, query strings, or programmatically set variables.
    @param web Web request object
    @param name Variable name to remove
    @stability Evolving
 */
PUBLIC void webRemoveVar(Web *web, cchar *name);

/**
    Write a file response
    @description Read the complete contents of an open file descriptor and send it as the
        HTTP response body. The function will yield the current fiber as needed to avoid
        blocking other concurrent operations.
    @pre Must only be called from a fiber
    @param web Web request object
    @param fd Open file descriptor to read from (file or pipe)
    @return Number of bytes written to the response, or negative on error
    @stability Evolving
 */

PUBLIC ssize webSendFile(Web *web, int fd);

/**
    Set the content length for the response
    @description Set the HTTP Content-Length header value for the response. This should be
        called before writing the response body if the total size is known in advance.
        Setting content length enables HTTP keep-alive connections.
    @param web Web request object
    @param len Response body length in bytes
    @stability Evolving
 */
PUBLIC void webSetContentLength(Web *web, ssize len);

/**
    Set the response HTTP status code
    @description Set the HTTP status code for the response (e.g., 200, 404, 500).
        This must be called before writing response headers or body content.
    @param web Web request object
    @param status HTTP status code (e.g., 200 for OK, 404 for Not Found)
    @stability Evolving
 */
PUBLIC void webSetStatus(Web *web, int status);

/**
    Set a request variable value
    @description Add or update a request variable in the web object's variable collection.
        Request variables are typically parsed from form data or query strings, but can
        be programmatically set using this function.
    @param web Web request object
    @param name Variable name
    @param value Variable value to set
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
    @stability Internal
 */
PUBLIC bool webValidateRequest(Web *web, cchar *path);

/**
    Low level routine to validate a string body against a signature
    @description Use this routine to validate request and response bodies if you cannot use the
        integrated validation or webValidateRequestBody.
    @param web Web object
    @param buf Optional buffer to store the validated data.
    @param data Request body data
    @param sigKey Signature key to validate against. Set to NULL for the standard response signature.
    @param tag Tag name for the request body. Set to "request", "response" or "query".
    @return True if the request is valid. Otherwise, return false and generate an error response to the client.
    @stability Evolving
 */
PUBLIC bool webValidateData(Web *web, RBuf *buf, cchar *data, cchar *sigKey, cchar *tag);

/**
    Validate a JSON object against the API signature.
    @description Use this routine to validate request and response bodies if you cannot use the
        integrated validation or webValidateRequestBody.
    @param web Web object
    @param buf Optional buffer to store the validated JSON.
    @param cjson JSON object
    @param jid Base JSON node ID from which to convert. Set to zero for the top level. If NULL, the top level is used.
    @param sigKey Signature key to validate against. Set to NULL for the standard response signature.
    @param tag Tag name for the request body. Set to "request", "response" or "query".
    @return True if the request is valid. Otherwise, return false and generate an error response to the client.
    @stability Evolving
 */
PUBLIC bool webValidateJson(Web *web, RBuf *buf, const Json *cjson, int jid, cchar *sigKey, cchar *tag);

/**
    Low level validate a JSON object against a signature using a signature specified by a signature ID.
    @param web Web object
    @param buf Optional buffer to store the validated JSON.
    @param cjson JSON object
    @param jid Base JSON node ID from which to convert. Set to zero for the top level. If NULL, the top level is used.
    @param sid Signature ID to validate against.
    @param depth Depth of the JSON object.
    @param tag Tag name for the request body. Set to "request", "response" or "query".
    @return True if the request is valid. Otherwise, return false and generate an error response to the client.
    @stability Evolving
 */
PUBLIC bool webValidateSignature(Web *web, RBuf *buf, const Json *cjson, int jid, int sid, int depth, cchar *tag);

/**
    Write response data
    @description Write data to the HTTP response body. This function automatically writes
        response headers if they haven't been sent yet. The function will yield the current
        fiber if the socket buffer is full, allowing other fibers to continue execution.
        Passing NULL buffer or zero size finalizes the response.
    @pre Must only be called from a fiber
    @param web Web request object
    @param buf Buffer containing data to write, or NULL to finalize
    @param bufsize Number of bytes to write, or 0 to finalize
    @return Number of bytes written, or negative on error
    @stability Evolving
 */
PUBLIC ssize webWrite(Web *web, cvoid *buf, ssize bufsize);

/**
    Write formatted string response data
    @description Write a formatted string to the HTTP response body using printf-style
        formatting. This is a convenience function that formats the string and calls webWrite().
        The function will yield the current fiber if necessary.
    @pre Must only be called from a fiber
    @param web Web request object
    @param fmt Printf-style format string
    @param ... Arguments for the format string
    @return Number of bytes written, or negative on error
    @stability Evolving
 */
PUBLIC ssize webWriteFmt(Web *web, cchar *fmt, ...);

/**
    Write JSON object as response data
    @description Serialize a JSON object and write it to the HTTP response body.
        Automatically sets the Content-Type header to "application/json" if not already set.
        The function will yield the current fiber if necessary.
    @pre Must only be called from a fiber
    @param web Web request object
    @param json JSON object to serialize and send
    @return Number of bytes written, or negative on error
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
    This routine will block the current fiber if necessary. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param web Web object
    @param json JSON object
    @param sigKey Signature key to validate against. Set to NULL for the standard response signature.
    @return The number of bytes written, or -1 for errors.
    @stability Evolving
 */
PUBLIC ssize webWriteValidatedJson(Web *web, const Json *json, cchar *sigKey);

/**
    Write a buffer with a validated signature
    @pre Must only be called from a fiber.
    @param web Web object
    @param buf Buffer of data to write.
    @param sigKey Signature key to validate against. Set to NULL for the standard response signature.
    @return The number of bytes written.
    @stability Evolving
 */
PUBLIC ssize webWriteValidatedData(Web *web, cchar *buf, cchar *sigKey);

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
/**
 * @name Cookie Configuration Flags
 * @description Flags for webSetCookie() to override default cookie settings.
 *     Use WEB_COOKIE_OVERRIDE combined with other flags to customize cookie behavior.
 * @{
 */
#define WEB_COOKIE_OVERRIDE  0x1           /**< Override the default cookie settings from host config */
#define WEB_COOKIE_HTTP_ONLY 0x2           /**< Set the HttpOnly flag (prevent JavaScript access) */
#define WEB_COOKIE_SECURE    0x4           /**< Set the Secure flag (HTTPS only) */
#define WEB_COOKIE_SAME_SITE 0x8           /**< Set the SameSite flag (CSRF protection) */
/** @} */

typedef struct WebSession {
    char *id;                              /**< Session ID key */
    int lifespan;                          /**< Session inactivity timeout (secs) */
    Ticks expires;                         /**< When the session expires */
    RHash *cache;                          /**< Cache of session variables */
} WebSession;

/**
    Add the security token to the response.
    @description To minimize form replay attacks, an XSRF security token can be utilized for requests on a route.
    This call will set an XSRF security token in the response as a response header and as a response cookie.
    Client-side Javascript can then send this token as a request header in subsquent POST requests.
    This will be caused automatically by the server for GET requests on a route with the 'xsrf' flag set to true.
    You can call this API for other requests to ensure a security token is present.
    To configure the server to require security tokens, set xsrf to true in the route.
    @param web Web object
    @param recreate Set to true to recreate the security token.
    @stability Prototype
 */
PUBLIC int webAddSecurityToken(Web *web, bool recreate);

/**
    Check an XSRF security token.
    @description Check the request security token against the security token defined in the session state. This function
       is called automatically by the web framework for state-changing requests on routes with the 'xsrf' flag set to
       true. You should not need to call it directly.  Make sure you have
    the 'Access-Control-Expose-Headers': 'X-XSRF-TOKEN' header set in your web.json5 headers configuration so the client
       can access the token.
    If the token is invalid, the request will be rejected with a 400 status code.
    @param web Web object
    @return True if the security token matches the session held token.
    @stability Prototype
 */
PUBLIC bool webCheckSecurityToken(Web *web);

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
    @stability Evolving
 */
PUBLIC void webDestroySession(Web *web);

/**
    Get a unique security token.
    @description This will get an existing security token or create a new token if one does not exist.
        If recreate is true, the security token will be recreated.
        Use #webAddSecurityToken to add the token to the response headers.
    @param web Web object
    @param recreate Set to true to recreate the security token.
    @return The security token string. Caller must not free.
    @stability Prototype
 */
PUBLIC cchar *webGetSecurityToken(Web *web, bool recreate);

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
    Set a response cookie
    @param web Web request object
    @param name Session variable name
    @param value Session variable value
    @param path Session variable path
    @param lifespan Session variable lifespan
    @param flags Flags to override the default cookie settings. Use WEB_COOKIE_OVERRIDE in combination with:
       WEB_COOKIE_HTTP_ONLY, WEB_COOKIE_SECURE, WEB_COOKIE_SAME_SITE.
    @stability Prototype
 */
PUBLIC void webSetCookie(Web *web, cchar *name, cchar *value, cchar *path, Ticks lifespan, int flags);

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
    @stability Evolving
    @internal
 */
PUBLIC int webUpgradeSocket(Web *web);
PUBLIC void webAsync(Web *web, WebSocketProc callback, void *arg);
PUBLIC int webWait(Web *web);
#endif /* ME_COM_WEBSOCKETS */

/************************************ Misc ************************************/

/**
    Convert a time to an HTTP date string
    @description Convert a Unix timestamp to a properly formatted HTTP date string
        suitable for use in HTTP headers like Last-Modified or Expires. The format
        follows RFC 2822 specifications.
    @param buf Buffer to hold the generated date string (must be at least 64 bytes)
    @param when Unix timestamp to convert
    @return Pointer to the buffer containing the formatted date string
    @stability Evolving
 */
PUBLIC char *webDate(char *buf, time_t when);

/**
    Decode a URL-encoded string
    @description Decode URL percent-encoded characters in place. Converts sequences
        like %20 back to their original characters. The string is modified in-place.
    @param str URL-encoded string to decode (modified in place)
    @return Pointer to the same string buffer after decoding
    @stability Evolving
 */
PUBLIC char *webDecode(char *str);

/**
    Encode a URL string
    @description URL-encode a string by converting special characters to percent-encoded
        sequences (e.g., space becomes %20). This creates a new allocated string.
    @param uri String to URL-encode
    @return Newly allocated URL-encoded string. Caller must free.
    @stability Evolving
 */
PUBLIC char *webEncode(cchar *uri);

/**
    Escape HTML special characters
    @description Escape HTML special characters (&, <, >, ", ') to their HTML entity
        equivalents to prevent XSS attacks and ensure proper HTML rendering.
    @param html HTML string to escape
    @return Newly allocated escaped HTML string. Caller must free.
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
