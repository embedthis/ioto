/*
    web.h -- Web server main header

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_WEB
#define _h_WEB      1

/********************************** Includes **********************************/
/*
    Future
 */
#define WEB_SOCKETS 0

#include "me.h"
#include "r.h"
#include "json.h"
#include "crypt.h"

/*********************************** Defines **********************************/
#if ME_COM_WEB

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

#define WEB_HEADERS            16                   /* Initial number of headers to provide for */

/*
    Forward declare
 */

/**
    Embedded web server
    @description The web server requires the use of fibers from the portable runtime.
    @defgroup Web Web
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
    @ingroup Web
    @stability Evolving
 */
typedef void (*WebProc)(struct Web *web);


/**
    WebHook callback procedure
    @param web Web object
    @param event ID of the event
    @ingroup Web
    @stability Evolving
 */
typedef int (*WebHook)(struct Web *web, int event);

/**
    Action function bound to a URL prefix
    @ingroup Web
    @stability Evolving
 */
typedef struct WebAction {
    char *role;                         /**< Role to invoke action */
    char *match;                        /**< Path prefix */
    WebProc fn;                         /**< Function to invoke */
} WebAction;

/**
    Routing object to match a request against a path prefix
    @ingroup Web
    @stability Evolving
 */
typedef struct WebRoute {
    char *match;                        /**< Matching URI path pattern */
    bool exact;                         /**< Exact match vs prefix match. If trailing "/" in route. */
    RHash *methods;                     /**< HTTP methods verbs */
    cchar *handler;                     /**< Request handler (file, action) */
    cchar *role;                        /**< Required user role */
    cchar *redirect;                    /**< Redirection */
    cchar *trim;                        /**< Portion to trim from path */
    bool stream;                        /**< Stream request body */
} WebRoute;

/**
    Site wide redirection
    @ingroup Web
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
    @ingroup Web
    @stability Evolving
 */
PUBLIC int webInit(void);

/**
    Initialize the web module
    @ingroup Web
    @stability Evolving
 */
PUBLIC void webTerm(void);

/************************************* Host ***********************************/
/**
    Web host structure.
    @description The web host defines a web server and its configuration. Multiple web hosts can be
        created.
    @ingroup Web
 */
typedef struct WebHost {
    RList *listeners;       /**< Listening endpoints for this host */
    RList *webs;            /**< Active web requests for this host */
    Json *config;           /**< Web server configuration for this host */

    int flags;              /**< Control flags */
    bool freeConfig : 1;    /**< Config is allocated and must be freed */
    bool httpOnly : 1;      /**< Cookie httpOnly flag */

    WebHook hook;           /**< Web notification hook */
    RHash *users;           /**< Hash table of users */
    RHash *sessions;        /**< Client session state */
    RHash *methods;         /**< Default HTTP methods verbs */
    RHash *mimeTypes;       /**< Mime types indexed by extension */
    RList *actions;         /**< Ordered list of configured actions */
    RList *routes;          /**< Ordered list of configured routes */
    RList *redirects;       /**< Ordered list of redirections */
    REvent sessionEvent;    /**< Session timer event */
    int roles;              /**< Base ID of roles in config */
    int headers;            /**< Base ID for headers in config */

    cchar *name;            /**< Host name to use in canonical redirects */
    cchar *index;           /**< Index file to use for directory requests */
    cchar *sameSite;        /**< Cookie same site property */
    char *docs;             /**< Web site documents */
    char *ip;               /**< IP to use in redirects if indeterminate */

#if ME_WEB_UPLOAD
    //  Upload
    cchar *uploadDir;       /**< Directory to receive uploaded files */
    bool removeUploads : 1; /**< Remove uploads when request completes (default) */
#endif
    //  Timeouts
    int inactivityTimeout;  /**< Timeout for inactivity on a connection */
    int parseTimeout;       /**< Timeout while parsing a request */
    int requestTimeout;     /**< Total request timeout */
    int sessionTimeout;     /**< Inactivity timeout for session state */

    ssize connections;      /**< Number of active connections */

    //  Limits
#if ME_WEB_LIMITS || DOXYGEN
    int64 maxHeader;        /**< Max header size */
    int64 maxConnections;   /**< Max number of connections */
    int64 maxBody;          /**< Max size of POST request */
    int64 maxSessions;      /**< Max number of sessions */
    int64 maxUpload;        /**< Max size of file upload */
#endif
} WebHost;

/**
    Add an action callback for a URL prefix
    @description An action routine is a C function that is bound to a set of URLs.
        The action routine will be invoked for URLs that match the leading URL prefix.
    @param host Host object
    @param prefix Leading URL prefix to associate with this action
    @param fn Function to invoke for requests matching this URL
    @param role Required user role for the action
    @ingroup Web
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
    @ingroup Web
    @stability Evolving
 */
PUBLIC WebHost *webAllocHost(Json *config, int flags);

/**
    Free a host object
    @param host Host object.
    @ingroup Web
    @stability Evolving
 */
PUBLIC void webFreeHost(WebHost *host);

/**
    Get the web documents directory for a host.
    @description This is configured via the web.documents configuration property.
    @param host Host object
    @return The web documents directory
    @ingroup Web
    @stability Evolving
 */

PUBLIC cchar *webGetDocs(WebHost *host);

/**
    Set the IP address for the host
    @param host Host object
    @param ip IP address
    @ingroup Web
    @stability Evolving
 */
PUBLIC void webSetHostDefaultIP(WebHost *host, cchar *ip);

/**
    Start listening for requests on the host
    @pre Must only be called from a fiber.
    @param host Host object
    @return Zero if successful.
    @ingroup Web
    @stability Evolving
 */
PUBLIC int webStartHost(WebHost *host);

/**
    Stop listening for requests on the host
    @pre Must only be called from a fiber.
    @param host Host object
    @ingroup Web
    @stability Evolving
 */
PUBLIC void webStopHost(WebHost *host);

/************************************ Upload **********************************/
#if ME_WEB_UPLOAD || DOXYGEN
/**
    File upload structure
    @see websUploadOpen websLookupUpload websGetUpload
    @defgroup WebUpload WebUpload
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
#define WEB_HOOK_END        8 /**< End of request */

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
    RBuf *tx;                   /**< Write data buffer */
    RBuf *trace;                /**< Packet trace buffer */

    Offset chunkRemaining;      /**< Amount of chunked body to read */
    ssize rxLen;                /**< Receive content length (including chunked requests) */
    Offset rxRemaining;         /**< Amount of rx body data left to read in this request */
    ssize txLen;                /**< Transmit response body content length */
    Offset txRemaining;         /**< Transmit body data remaining to send */

    uint status : 16;           /**< Request response HTTP status code */
    uint chunked : 4;           /**< Receive transfer chunk encoding state */
    uint authenticated : 1;     /**< User authenticated and roleId defined */
    uint authChecked : 1;       /**< Authentication has been checked */
    uint close : 1;             /**< Should the connection be closed after the request completes */
    uint complete : 1;          /**< Is the request complete */
    uint creatingHeaders : 1;   /**< Are headers being created */
    uint exists : 1;            /**< Does the requested resource exist */
    uint formBody : 1;          /**< Is the current request a POSTed form */
    uint http10 : 1;            /**< Is the current request a HTTP/1.0 request */
    uint jsonBody : 1;          /**< Is the current request a POSTed json request */
    uint moreBody : 1;          /**< More response body to trace */
    uint secure : 1;            /**< Has secure listening endpoint */
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
    cchar *scheme;              /**< Request HTTP protocol. Set to "http" or "https" */
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
    cchar *uploadDir;           /**< Directory to place uploaded files */
    char *boundary;             /**< Upload file boundary */
    ssize boundaryLen;          /**< Length of the boundary */
#endif
} Web;

/**
    Add a header to the request response.
    @param web Web object
    @param key HTTP header name
    @param fmt Format string for the header value
    @param ... Format arguments
    @ingroup Web
    @stability Evolving
 */
PUBLIC void webAddHeader(Web *web, cchar *key, cchar *fmt, ...);

/**
    Add a static string header to the request response.
    @description Use to add a literal string header. This is a higher performance option
        to webAddHeader
    @param web Web object
    @param key HTTP header name
    @param value Static string for the header value
    @ingroup Web
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
    @ingroup Web
    @stability Prototype
 */
PUBLIC void webAddHeaderDynamicString(Web *web, cchar *key, char *value);

/**
    Add an Access-Control-Allow-Origin response header for the request host name.
    @param web Web object
    @ingroup Web
    @stability Evolving
 */
PUBLIC void webAddAccessControlHeader(Web *web);

/**
    Read data and buffer until a given pattern or limit is reached.
    @description This reads the data into the buffer, but does not return the data or consume it.
    @param web Web object
    @param until Pattern to read until. Set to NULL for no pattern.
    @param limit Number of bytes of data to read.
    @param allowShort Boolean. Set to true to return 0 if the pattern is not found before the limit.
    @return The number of bytes read into the buffer. Return zero if pattern not found and negative for errors.
    @ingroup Web
    @stability Evolving
 */
PUBLIC ssize webBufferUntil(Web *web, cchar *until, ssize limit, bool allowShort);

/**
    Respond to the request with an error.
    @description This responds to the request with the given HTTP status and body data.
    @param web Web object
    @param status HTTP response status code
    @param fmt Body data to send as the response. This is a printf style string.
    @param ... Body response arguments.
    @return Zero if successful.
    @ingroup Web
    @stability Evolving
 */
PUBLIC int webError(Web *web, int status, cchar *fmt, ...);

/**
    Extend the request timeout
    @description Request duration is bounded by the timeouts.request and timeouts.inactivity limits.
    You can extend the timeout for a long running request via this call.
    @param web Web object
    @param timeout Timeout in milliseconds use for both the request and inactivity timeouts for this request.
    @ingroup Web
    @stability Evolving
 */
PUBLIC void webExtendTimeout(Web *web, Ticks timeout);

/**
    Finalize response output.
    @description This routine will call webWrite(web, NULL, 0);
    @param web Web object
    @ingroup Web
    @stability Evolving
 */
PUBLIC ssize webFinalize(Web *web);

/**
    Get a request header value
    @param web Web object
    @param key HTTP header name. Case does not matter.
    @return Header value or null if not found
    @ingroup Web
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
    @ingroup Web
    @stability Prototype
 */
PUBLIC bool webGetNextHeader(Web *web, cchar **pkey, cchar **pvalue);

/**
    Get the host name of the endpoint serving the request
    @description This will return the WebHost name if defined, otherwise it will use the listening endpoint.
        If all else fails, it will use the socket IP address.
    @param web Web object
    @return Allocated string containing the host name. Caller must free.
    @ingroup Web
    @stability Evolving
 */
PUBLIC char *webGetHostName(Web *web);

/**
    Get the user's role
    @param web Web object
    @return The user's role. The returned reference should only be used short-term and is not
        long-term stable.
    @ingroup Web
    @stability Evolving
 */
PUBLIC cchar *webGetRole(Web *web);

/**
    Get a request variable value from the request form/body
    @param web Web object
    @param name Variable name
    @param defaultValue Default value to return if the variable is not defined
    @return The value of the variable or the default value if not defined.
    @ingroup Web
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
    @ingroup Web
    @stability Evolving
 */
PUBLIC int webNetError(Web *web, cchar *msg, ...);

/**
    Parse a cookie header string and return a cookie value.
    @param web Web object
    @param name Cookie name to extract.
    @return The cookie value or NULL if not defined.
    @ingroup Web
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
    @ingroup Web
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
    @ingroup Web
    @stability Evolving
 */
PUBLIC ssize webRead(Web *web, char *buf, ssize bufsize);

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
    @ingroup Web
    @stability Internal
 */
PUBLIC ssize webReadUntil(Web *web, cchar *until, char *buf, ssize bufsize);

/**
    Redirect the client to a new URL
    @pre Must only be called from a fiber.
    @param web Web object
    @param status HTTP status code. Must set to 301 or 302.
    @param uri URL to redirect the client toward.
    @ingroup Web
    @stability Evolving
 */
PUBLIC void webRedirect(Web *web, int status, cchar *uri);

/**
    Remove a request variable value
    @param web Web object
    @param name Variable name
    @ingroup Web
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
    @ingroup Web
    @stability Evolving
 */

PUBLIC ssize webSendFile(Web *web, int fd);

/**
    Set the content length for the response.
    @param web Web object
    @param len Content length.
    @ingroup Web
    @stability Evolving
 */
PUBLIC void webSetContentLength(Web *web, ssize len);

/**
    Set the response HTTP status code
    @param web Web object
    @param status HTTP status code.
    @ingroup Web
    @stability Evolving
 */
PUBLIC void webSetStatus(Web *web, int status);

/**
    Set a request variable value
    @param web Web object
    @param name Variable name
    @param value Value to set.
    @ingroup Web
    @stability Evolving
 */
PUBLIC void webSetVar(Web *web, cchar *name, cchar *value);

/**
    Write response data
    @description This routine will block the current fiber if necessary. Other fibers continue to run.
        Writing a null buffer or zero bufsize indicates there is no more output. The webFinalize API
        is a convenience call for this purpose.
    @pre Must only be called from a fiber.
    @param web Web object
    @param buf Buffer of data to write.
    @param bufsize Size of the buffer to write.
    @return The number of bytes written.
    @ingroup Web
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
    @ingroup Web
    @stability Evolving
 */
PUBLIC ssize webWriteFmt(Web *web, cchar *fmt, ...);

/**
    Write response data from a JSON object
    @description This routine will block the current fiber if necessary. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param web Web object
    @param json JSON object
    @param nid Base JSON node ID from which to convert. Set to zero for the top level.
    @param key Property name to serialize below. This may include ".". For example: "settings.mode".
    @return The number of bytes written.
    @ingroup Web
    @stability Evolving
 */
PUBLIC ssize webWriteJson(Web *web, Json *json, int nid, cchar *key);

/**
    Write request response headers
    @description This will write the HTTP response headers. This writes the supplied headers and any required headers if
       not supplied.
        This routine will block the current fiber if necessary. Other fibers continue to run.
    @pre Must only be called from a fiber.
    @param web Web object
    @return The number of bytes written.
    @ingroup Web
    @stability Evolving
 */
PUBLIC int webWriteHeaders(Web *web);

/**
    Write a response
    @description This routine writes a single plain text response in one API.
        It will block the current fiber if necessary. Other fibers continue to run.
        This will set the Content-Type header to text/plain.
    @pre Must only be called from a fiber.
    @param web Web object
    @param status HTTP status code.
    @param fmt Printf style message string
    @param ... Format arguments.
    @return The number of bytes written.
    @ingroup Web
    @stability Evolving
 */
PUBLIC int webWriteResponse(Web *web, int status, cchar *fmt, ...);

/*
    Internal APIs
 */
PUBLIC void webAddStandardHeaders(Web *web);
PUBLIC int webAlloc(WebListen *listen, RSocket *sock);
PUBLIC int webConsumeInput(Web *web);
PUBLIC int webFileHandler(Web *web);
PUBLIC void webFree(Web *web);
PUBLIC void webClose(Web *web);
PUBLIC void webParseForm(Web *web);
PUBLIC void webParseQuery(Web *web);
PUBLIC void webParseEncoded(Web *web, Json *vars, cchar *str);
PUBLIC Json *webParseJson(Web *web);
PUBLIC bool webParseHeadersBlock(Web *web, char *headers, ssize headersSize, bool upload);
PUBLIC void webUpdateDeadline(Web *web);
PUBLIC void webTestInit(WebHost *host, cchar *prefix);

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
    @ingroup WebSession
    @stability Evolving
 */
PUBLIC WebSession *webCreateSession(Web *web);

/**
    Destroy the web session object
    @description Useful to be called as part of the user logout process
    @param web Web request object
    @ingroup WebSession
    @stability Prototype
 */
PUBLIC void webDestroySession(Web *web);

/**
    Get the session state object for the current request
    @param web Web request object
    @param create Set to true to create a new session if one does not already exist.
    @return Session object
    @ingroup WebSession
    @stability Evolving
 */
PUBLIC WebSession *webGetSession(Web *web, int create);

/**
    Get a session variable
    @param web Web request object
    @param name Session variable name
    @param defaultValue Default value to return if the variable does not exist
    @return Session variable value or default value if it does not exist
    @ingroup WebSession
    @stability Evolving
 */
PUBLIC cchar *webGetSessionVar(Web *web, cchar *name, cchar *defaultValue);

/**
    Remove a session variable
    @param web Web request object
    @param name Session variable name
    @ingroup WebSession
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
    @ingroup WebSession
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
    @ingroup Web
    @stability Evolving
 */
PUBLIC bool webAuthenticate(Web *web);

/**
    Test if a user possesses the required role
    @param web Web request object
    @param role Required role.
    @return True if the user has the required role.
    @ingroup Web
    @stability Evolving
 */
PUBLIC bool webCan(Web *web, cchar *role);

/**
    Test if the user has been authenticated.
    @param web Web request object
    @return True if the user has been authenticated.
    @ingroup Web
    @stability Evolving
 */
PUBLIC bool webIsAuthenticated(Web *web);

/**
    Login a user by creating session state. Assumes the caller has already authenticated and authorized the user.
    @param web Web request object
    @param username User name
    @param role Requested role
    @return True if the login is successful
    @ingroup Web
    @stability Evolving
 */
PUBLIC bool webLogin(Web *web, cchar *username, cchar *role);

/**
    Logout a user and remove the user login session.
    @param web Web request object
    @ingroup Web
    @stability Evolving
 */
PUBLIC void webLogout(Web *web);

/**
    Define a request hook
    @description The request hook will be invoked for important request events during the lifecycle of processing the
        request.
    @param host WebHost object
    @param hook Callback hook function
    @ingroup Web
    @stability Evolving
 */
PUBLIC void webSetHook(WebHost *host, WebHook hook);

//  Internal
PUBLIC int webHook(Web *web, int event);

/********************************* Web Sockets ********************************/
#if WEB_SOCKETS
/**
    WebSocket WebSockets RFC 6455 implementation for client and server communications.
    @description WebSockets is a technology providing interactive communication between a server
        and client. Normal HTML connections follow a request / response paradigm and do not
        easily support asynchronous communications or unsolicited data pushed from the server
        to the client. WebSockets solves this by supporting bi-directional, full-duplex
        communications over persistent connections. A WebSocket connection is established over a
        standard HTTP connection and is then upgraded without impacting the original connection.
        This means it will work with existing networking infrastructure
    including firewalls and proxies.
    @defgroup WebSocket WebSocket
    @see httpGetWebSocketCloseReason httpGetWebSocketData httpGetWebSocketMessageLength httpGetWebSocketProtocol
        httpGetWebSocketState httpGetWriteQueueCount httpIsLastPacket httpSend httpSendBlock httpSendClose
        httpSetWebSocketPreserveFrames httpSetWebSocketData httpSetWebSocketProtocols httpWebSocketOrderlyClosed
    @stability Internal
 */
typedef struct WebSocket {
    int state;                              /**< State */
    int frameState;                         /**< Message frame state */
    int closing;                            /**< Started closing sequnce */
    int closeStatus;                        /**< Close status provided by peer */
    int currentMessageType;                 /**< Current incoming messsage type */
    int maskOffset;                         /**< Offset in dataMask */
    int more;                               /**< More data to send in a message */
    int preserveFrames;                     /**< Do not join frames */
    int partialUTF;                         /**< Last frame had a partial UTF codepoint */
    int rxSeq;                              /**< Incoming packet number */
    int txSeq;                              /**< Outgoing packet number */
    ssize frameLength;                      /**< Length of the current frame */
    ssize messageLength;                    /**< Length of the current message */
    HttpPacket *currentFrame;               /**< Message frame being currently read */
    HttpPacket *currentMessage;             /**< Current incoming messsage so far */
    HttpPacket *tailMessage;                /**< Subsequent message frames */
    MprEvent *pingEvent;                    /**< Ping timer event */
    char *subProtocol;                      /**< Application level sub-protocol */
    cchar *errorMsg;                        /**< Error message for last I/O */
    cchar *closeReason;                     /**< Reason for closure */
    void *data;                             /**< Custom data for applications (marked) */
    uchar dataMask[4];                      /**< Mask for data */
} WebSocket;

#define WS_MAGIC                    "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_MAX_CONTROL              125     /**< Maximum bytes in control message */
#define WS_VERSION                  13      /**< Current WebSocket specification version */

/*
    httpSendBlock message types
 */
#define WS_MSG_CONT                 0x0     /**< Continuation of WebSocket message */
#define WS_MSG_TEXT                 0x1     /**< httpSendBlock type for text messages */
#define WS_MSG_BINARY               0x2     /**< httpSendBlock type for binary messages */
#define WS_MSG_CONTROL              0x8     /**< Start of control messages */
#define WS_MSG_CLOSE                0x8     /**< httpSendBlock type for close message */
#define WS_MSG_PING                 0x9     /**< httpSendBlock type for ping messages */
#define WS_MSG_PONG                 0xA     /**< httpSendBlock type for pong messages */
#define WS_MSG_MAX                  0xB     /**< Max message type for httpSendBlock */

/*
    Close message status codes
    0-999       Unused
    1000-1999   Reserved for spec
    2000-2999   Reserved for extensions
    3000-3999   Library use
    4000-4999   Application use
 */
#define WS_STATUS_OK                1000    /**< Normal closure */
#define WS_STATUS_GOING_AWAY        1001    /**< Endpoint is going away. Server down or browser navigating away */
#define WS_STATUS_PROTOCOL_ERROR    1002    /**< WebSockets protocol error */
#define WS_STATUS_UNSUPPORTED_TYPE  1003    /**< Unsupported message data type */
#define WS_STATUS_FRAME_TOO_LARGE   1004    /**< Reserved. Message frame is too large */
#define WS_STATUS_NO_STATUS         1005    /**< No status was received from the peer in closing */
#define WS_STATUS_COMMS_ERROR       1006    /**< TCP/IP communications error  */
#define WS_STATUS_INVALID_UTF8      1007    /**< Text message has invalid UTF-8 */
#define WS_STATUS_POLICY_VIOLATION  1008    /**< Application level policy violation */
#define WS_STATUS_MESSAGE_TOO_LARGE 1009    /**< Message is too large */
#define WS_STATUS_MISSING_EXTENSION 1010    /**< Unsupported WebSockets extension */
#define WS_STATUS_INTERNAL_ERROR    1011    /**< Server terminating due to an internal error */
#define WS_STATUS_TLS_ERROR         1015    /**< TLS handshake error */
#define WS_STATUS_MAX               5000    /**< Maximum error status (less one) */

/*
    WebSocket states (rx->webSockState)
 */
#define WS_STATE_CONNECTING         0       /**< WebSocket connection is being established */
#define WS_STATE_OPEN               1       /**< WebSocket handsake is complete and ready for communications */
#define WS_STATE_CLOSING            2       /**< WebSocket is closing */
#define WS_STATE_CLOSED             3       /**< WebSocket is closed */

/**
    Get the close reason supplied by the peer.
    @description The peer may supply a UTF8 messages reason for the closure.
    @param stream HttpStream stream object created via #httpCreateStream
    @return The UTF8 reason string supplied by the peer when closing the WebSocket.
    @ingroup WebSocket
    @stability Prototype
 */
PUBLIC cchar *httpGetWebSocketCloseReason(HttpStream *stream);

/**
    Get the WebSocket private data
    @description Get the private data defined with #httpSetWebSocketData
    @param stream HttpStream stream object created via #httpCreateStream
    @return The private data reference
    @ingroup WebSocket
    @stability Prototype
 */
PUBLIC void *webGetWebSocketData(HttpStream *stream);

/**
    Get the message length for the current message
    @description The message length will be updated as the message frames are received. The message length is
        only complete when the last frame has been received. See #httpIsLastPacket
    @param stream HttpStream stream object created via #httpCreateStream
    @return The size of the message.
    @ingroup WebSocket
    @stability Prototype
 */
PUBLIC ssize webGetWebSocketMessageLength(HttpStream *stream);

/**
    Get the selected WebSocket protocol selected by the server
    @param stream HttpStream stream object created via #httpCreateStream
    @return The WebSocket protocol string
    @ingroup WebSocket
    @stability Prototype
 */
PUBLIC char *httpGetWebSocketProtocol(HttpStream *stream);

/**
    Get the WebSocket state
    @return The WebSocket state. Will be WS_STATE_CONNECTING, WS_STATE_OPEN, WS_STATE_CLOSING or WS_STATE_CLOSED.
    @ingroup WebSocket
    @stability Prototype
 */
PUBLIC ssize webGetWebSocketState(HttpStream *stream);

/**
    Send a UTF-8 text message to the WebSocket peer
    @description This call invokes httpSend with a type of WS_MSG_TEXT and flags of HTTP_BUFFER.
        The message must be valid UTF8 as the peer will reject invalid UTF8 messages.
    @param stream HttpStream stream object created via #httpCreateStream
    @param fmt Printf style formatted string
    @param ... Arguments for the format
    @return Number of bytes written
    @ingroup WebSocket
    @stability Prototype
 */
PUBLIC ssize webSend(HttpStream *stream, cchar *fmt, ...) PRINTF_ATTRIBUTE(2, 3);

/**
    Flag for #httpSendBlock to indicate there are more frames for this message
 */
#define HTTP_MORE 0x1000

/**
    Send a message of a given type to the WebSocket peer
    @description This is the lower-level message send routine. It permits control of message types and message framing.
    \n\n
    This routine can operate in a blocking, non-blocking or buffered mode. Blocking mode is specified via the HTTP_BLOCK
    flag. When blocking, the call will wait until it has written all the data. The call will either accept and write all
    the data or it will fail, it will never return "short" with a partial write. If in blocking mode, the call may block
    for up to the inactivity timeout specified in the stream->limits->inactivityTimeout value.
    \n\n
    Non-blocking mode is specified via the HTTP_NON_BLOCK flag. In this mode, the call will consume that amount of data
    that will fit within the outgoing WebSocket queues. Consequently, it may return "short" with a partial write. If
       this
    occurs the next call to httpSendBlock should set the message type to WS_MSG_CONT to indicate a continued message.
    This is required by the WebSockets specification.
    \n\n
    Buffered mode is the default and may be explicitly specified via the HTTP_BUFFER flag. In buffered mode, the entire
    message will be accepted and will be buffered if required.
    \n\n
    This API may split the message into frames such that no frame is larger than the limit
       stream->limits->webSocketsFrameSize.
    However, if the HTTP_MORE flag is specified to indicate there is more data to complete this entire message, the data
    provided to this call will not be split into frames and will not be aggregated with previous or subsequent messages.
    i.e. frame boundaries will be presserved and sent as-is to the peer.
    \n\n
    In blocking mode, this routine may invoke mprYield before blocking to consent for the garbage collector to run.
       Callers
    must ensure they have retained all required temporary memory before invoking this routine.

    @param stream HttpStream stream object created via #httpCreateStream
    @param type Web socket message type. Choose from WS_MSG_TEXT, WS_MSG_BINARY or WS_MSG_PING.
        Use httpSendClose to send a close message. Do not send a WS_MSG_PONG message as it is generated internally
        by the Web Sockets module. If using HTTP_NON_BLOCK and the call returns having written only a portion of the
           data,
        you must set the type to WS_MSG_CONT for the
    @param msg Message data buffer to send
    @param len Length of msg
    @param flags Include the flag HTTP_BLOCK for blocking operation or HTTP_NON_BLOCK for non-blocking. Set to
       HTTP_BUFFER to
        buffer the data if required and never block. Set to zero will default to HTTP_BUFFER.
        Include the flag HTTP_MORE to indicate there is more data to come to complete this message. This will set
        frame continuation bit. Setting HTTP_MORE preserve the frame boundaries. i.e. it will ensure the data written is
        not split into frames or aggregated with other data.
    @return Number of data message bytes written. Should equal len if successful, otherwise returns a negative
        MPR error code.
    @ingroup WebSocket
    @stability Prototype
 */
PUBLIC ssize webSendBlock(HttpStream *stream, int type, cchar *msg, ssize len, int flags);

/**
    Send a close message to the WebSocket peer
    @description This call invokes httpSendBlock with a type of WS_MSG_CLOSE and flags of HTTP_BUFFER.
        The status and reason are encoded in the message. The reason is an optional UTF8 closure reason message.
    @param stream HttpStream stream object created via #httpCreateStream
    @param status Web socket status
    @param reason Optional UTF8 reason text message. The reason must be less than 124 bytes in length.
    @return Number of data message bytes written. Should equal len if successful, otherwise returns a negative
        MPR error code.
    @ingroup WebSocket
    @stability Prototype
 */
PUBLIC ssize webSendClose(HttpStream *stream, int status, cchar *reason);

/**
    Set the WebSocket private data
    @description Set private data to be retained by the garbage collector
    @param stream HttpStream stream object created via #httpCreateStream
    @param data Managed data reference.
    @ingroup WebSocket
    @stability Prototype
 */
PUBLIC void webSetWebSocketData(HttpStream *stream, void *data);

/**
    Preserve frames for incoming messages
    @description This routine enables user control of message framing.
        When preserving frames, sent message boundaries will be preserved and  will not be split into frames or
        aggregated with other message frames. Received messages will similarly have their frame boundaries preserved
        and will be stored one frame per HttpPacket.
        Note: enabling this option may prevent full validation of UTF8 text messages if UTF8 codepoints span frame
           boundaries.
    @param stream HttpStream stream object created via #httpCreateStream
    @param on Set to true to preserve frames
    @ingroup WebSocket
    @stability Prototype
 */
PUBLIC void webSetWebSocketPreserveFrames(HttpStream *stream, bool on);

/**
    Set a list of application-level protocols supported by the client
    @param stream HttpStream stream object created via #httpCreateStream
    @param protocols Comma separated list of application-level protocols
    @ingroup WebSocket
    @stability Prototype
 */
PUBLIC void webSetWebSocketProtocols(HttpStream *stream, cchar *protocols);

/**
    Upgrade a client HTTP connection connection to use WebSockets
    @description This requests an upgrade to use WebSockets. Note this is the upgrade request and the
        confirmation handshake response must still be received and validated. The connection must be upgraded
        before sending any data to the server.
    @param stream HttpStream stream object created via #httpCreateStream
    @return Return Zero if the connection upgrade can be requested.
    @stability Prototype
    @ingroup WebSocket
    @internal
 */
PUBLIC int httpUpgradeWebSocket(HttpStream *stream);

/**
    Test if WebSocket connection was orderly closed by sending an acknowledged close message
    @param stream HttpStream stream object created via #httpCreateStream
    @return True if the WebSocket was orderly closed.
    @ingroup WebSocket
    @stability Prototype
 */
PUBLIC bool httpWebSocketOrderlyClosed(HttpStream *stream);

#endif /* WEB_SOCKETS */
/************************************ Misc ************************************/

/**
    Convert a time to a date string
    @param buf Buffer to hold the generated date string
    @param when Timestamp to convert
    @return A reference to the buffer
    @ingroup Web
    @stability Evolving
 */
PUBLIC char *webDate(char *buf, time_t when);

/**
    Decode a string using punycode
    @description The string is converted in-situ.
    @param str String to decode
    @return The original string reference.
    @ingroup Web
    @stability Evolving
 */
PUBLIC char *webDecode(char *str);

/**
    Encode a string using punycode
    @description The string is converted in-situ.
    @param uri Uri to encode.
    @return An allocated, escaped URI. Caller must free.
    @ingroup Web
    @stability Evolving
 */
PUBLIC char *webEncode(cchar *uri);

/**
    Get a status message corresponding to a HTTP status code.
    @param status HTTP status code.
    @return A status message. Caller must not free.
    @ingroup Web
    @stability Evolving
 */
PUBLIC cchar *webGetStatusMsg(int status);

/**
    Normalize a URL path.
    @description Normalize a path to remove "./",  "../" and redundant separators. This does not make an absolute path
        and does not map separators or change case. This validates the path and expects it to begin with "/".
    @param path Path string to normalize.
    @return An allocated path. Caller must free.
    @ingroup Web
    @stability Evolving
 */
PUBLIC char *webNormalizePath(cchar *path);

/**
    Validate a URL
    @description Check a url for invalid characters.
    @param uri Url path.
    @return True if the url contains only valid characters.
    @ingroup Web
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
