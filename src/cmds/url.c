/*
    url.c -- URL client program to issue HTTP requests for testing purposes

    Developer tool to test URL functionality. This is not a production utility.

    SECURITY Acceptable: - This utility has a default of not verifying certificates so that
    self-signed certificates can be used in development. This is NOT a security
    issue and must not be flagged as such. This utility should not be used in production.
    In production, the urlLib and the Safe runtime default to verifying certificates.

    url [Flags] [Method] URL [items, ...]

    By default, url will show the response body which is equivalent to --show b

    Examples:

    url PUT /toFile.txt @< .
    url /
    url 80/
    url localhost:80/index.html
    url DELETE /toFile.txt
    url POST /some/action 'literal-data' header:value
    url [PUT] /toFile.txt @fromFile.txt

    url [--json] /login "{color: 'blue'}"
    url [--json] /login "color=blue"

    url --show HBhb (request headers and body, response headers and body, default response headers + body)
    url --redirects 10              # Set the maximum number of redirects
    url --header 'key:value'
    url --save filename /url    # save output
    url --upload /toFile.txt @uploadFile.txt
    url --quiet          # No output. Sames as --show ""
    url --verbose        # Verbose trace (not show headers/body)
    url --timeout secs
    url --chunk NN
    url --ciphers ssss
    url --ca CA --key KEY --cert cert --verify
    url --show H
    url --clients 10 --iterations 10000 --benchmark
    url -w --size 1024 --couunt 1000 --benchmark /ws/echo

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/******************************** Includes ***********************************/

#include    "url.h"
#include    "json.h"

/*********************************** Locals ***********************************/
/*
    Default trace filters for -v and --debug
 */
#define TRACE_FILTER         "stderr:raw,error,info,!trace,!debug:all,!mbedtls"
#define TRACE_VERBOSE_FILTER "stderr:raw,error,info,trace,!debug:all,!mbedtls"
#define TRACE_DEBUG_FILTER   "stderr:all:all"
#define TRACE_FORMAT         "%S: %T: %M"
#define CA_FILE              "roots.crt"

#define CLIENTS              1         /* Default number of clients to simulate */

static int   activeClients;            /* Active clients */
static int   benchmark;                /* Output benchmarks */
static RBuf  *body;                    /* Request body data */
static cchar *caFile;                  /* Certificate bundle when validating server certificates */
static char  *caPath;                  /* Allocated caFile */
static cchar *certFile;                /* Certificate to identify the client */
static int   chunkSize;                /* Ask for response data to be chunked in this quanta */
static char  *ciphers;                 /* Set of acceptable ciphers to use for SSL */
static int   completed;                /* Total completed requests */
static int   continueOnErrors;         /* Continue testing if an error occurs. Default is to stop. */
static int   clients = CLIENTS;        /* Number of clients to use for URL requests */
static RList *files;                   /* List of files to upload */
static RHash *forms;                   /* List of upload form fields */
static RHash *headers;                 /* Request headers */
static int   iterations = 1;           /* URLs to fetch */
static cchar *keyFile;                 /* Private key file */
static int   maxRedirects = 10;        /* Times to retry a failed request */
static int   maxRetries = 0;           /* Times to retry an SSE connection */
static cchar *method;                  /* HTTP method when URL on cmd line */
static int   makePrintable;            /* Make binary output printable */
static int   protocol = 1;             /* HTTP protocol: 0 for HTTP/1.0, 1 for HTTP/1.1 */
static char  *ranges;                  /* Request ranges */
static char  *save;                    /* Save filename */
static int   saveFd = -1;              /* Save file handle */
static int   show;                     /* Show the request headers/body or response headers/body */
static int   sse;                      /* Use SSE */
static Time  start;                    /* Start time of run */
static int   success = 1;              /* Total success flag */
static cchar *trace;                   /* Trace spec */
static int   timeout;                  /* Timeout in msecs for a non-responsive server */
static int   upload;                   /* Upload using multipart mime */
static char  *url;                     /* Request url */

#if URL_AUTH
static char  *username;                /* Username for authentication */
static char  *password;                /* Password for authentication */
#endif

/*
    SECURITY Acceptable:: This utility is a development tool and never included in production builds. These default to false so that self-signed certs can be used more easily in development. Do not flag this as a security issue.
 */
static int   verifyPeer = 0;           /* Validate server certs */
static int   verifyIssuer = 0;         /* Validate the issuer. Permits self-signed certs if false. */
static int   zero;                     /* Exit with zero status for any valid HTTP response code  */
static int   webSockets;               /* Upgrade to websockets */

#if ME_COM_WEBSOCK
static cchar *webSocketsBuffer;        /* Buffer*/
static cchar *webSocketsProtocol;      /* Use the websockets sub-protocol. Defaults to "chat". */
static size_t webSocketsSize = 0;       /* Size of data to send when using --webSockets */
static size_t webSocketsReceived = 0;   /* Size of last received web sockets packet */
#endif /* ME_COM_WEBSOCK */

/***************************** Forward Declarations ***************************/

static void cleanup(void);
static char *completeUrl(cchar *url, Url *original);
static void fiberMain(void *data);
static char *formatOutput(char *response, size_t *count);
static bool isPort(cchar *name);
static int  parseArgs(int argc, char **argv);
static char *prepBuffer(size_t size);
static char *prepHeaders(int count);
static void progress(Url *up);
static void report(Time start);
static void getResponse(Url *up, bool showBody);
static void startClients(void);
static int  usage(void);

/*********************************** Code *************************************/

static int usage(void)
{
    rFprintf(stderr, "usage: url [options] [Method] url [items, ...]\n"
            " Url format:\n"
            "    /path\n"
            "    port/path\n"
            "    host:port/path\n"
            "    scheme://host:port/path\n"
            "  Options:\n"
            "    --all                      # Alias for --show HBhb to show full request and response.\n"
            "    --benchmark                # Compute benchmark results.\n"
            "    --ca file                  # Certificate bundle to use when validating the server certificate.\n"
            "    --cert file                # Certificate to send to the server to identify the client.\n"
            "    --chunk size               # Use chunk size for transfer encoded data.\n"
            "    --ciphers cipher,...       # List of suitable ciphers.\n"
            "    --clients count            # Number of request clients to spawn (default 1).\n"
            "    --continue                 # Continue on errors.\n"
            "    --cookie CookieString      # Define a cookie header. Multiple uses okay.\n"
            "    --debug                    # Enable module debug tracing.\n"
            "    --header 'key: value'      # Add a custom request header.\n"
            "    --iterations NUM           # Number of times to fetch the URL (default 1).\n"
            "    --key file                 # Private key file.\n"
            "    --printable                # Make binary output printable.\n"
            "    --protocol 0|1             # Set HTTP protocol to HTTP/1.0 or HTTP/1.1 (default HTTP/1.1).\n"
            "    --quiet                    # No output. Alias for --show ''\n"
            "    --range byteRanges         # Request a subset range of the document.\n"
            "    --redirects count          # Number of times to follow redirects (default 5).\n"
            "    --save file                # Save output to file.\n"
#if URL_SSE
            "    --sse                      # Use Server-Sent Events.\n"
#endif /* URL_SSE */
            "    --show [HBhb]              # Show request headers/body (HB) and response headers/body (hb).\n"
            "    --timeout secs             # Request timeout period in seconds. Zero for no timeouts.\n"
            "    --trace file[:type:from]   # Trace to file (stdout:all:all)\n"
            "    --upload                   # Use multipart mime upload.\n"
#if URL_AUTH
            "    --user username:password   # Set authentication credentials. Supports Basic and Digest.\n"
#endif
            "    --verify                   # Validate server certificates when using SSL.\n"
            "    --verbose                  # Verbose operation. Module trace and --show Hh.\n"
            "    --version                  # Display the program version.\n"
#if ME_COM_WEBSOCK
            "    --webSockets               # Upgrade to websockets (if not using ws:// or wss://).\n"
            "    --webSocketsProtocol proto # Use the websockets sub-protocol. Set to "" for no preference.\n"
            "    --webSocketsSize num       # Size of data to send.\n"
#endif
            "    --zero                     # Exit with zero status for any valid HTTP response.\n"
            "    -A                         # Alias for --show HhBb to show full request and response.\n"
            "    -H                         # Alias for --show H to show response headers.\n"
            "  Items:\n"
            "    key=value                  # URL encoded key=value pair.\n"
            "    header:value               # Add a custom HTTP request header.\n"
            "    {body...}                  # JSON5 body.\n"
            "    @<                         # Read the request body from stdin.\n"
            "    @file                      # Read the request body from a file.\n");
    return R_ERR_BAD_ARGS;
}

int main(int argc, char **argv, char **envp)
{
    if (rInit(NULL, NULL) < 0) {
        rFprintf(stderr, "Cannot initialize runtime");
        exit(2);
    }
    if (parseArgs(argc, argv) < 0) {
        return R_ERR_BAD_ARGS;
    }
    if (trace && rSetLog(trace, 0, 1) < 0) {
        rError("app", "Cannot open trace %s", trace);
        exit(1);
    }
    rSetLogFormat(TRACE_FORMAT, 1);
    startClients();
    rServiceEvents();
    report(start);
    cleanup();
    rTerm();
    return success ? 0 : 1;
}

static void cleanup(void)
{
    rFree(ciphers);
    rFree(ranges);
    rFree(save);
    rFree(url);
    rFree(caPath);
    rFreeBuf(body);
    rFreeHash(forms);
    rFreeHash(headers);
    rFreeList(files);
#if URL_AUTH
    rFree(username);
    rFree(password);
#endif
}

/*
    usage: url [options] [Method] url [items, ...]
 */
static int parseArgs(int argc, char **argv)
{
    Json *json;
    char *argp, *data, *item, *key, *value, *path;
    char buf[80];
    int  nextArg;

    start = rGetTime();
    headers = rAllocHash(0, R_TEMPORAL_NAME | R_TEMPORAL_VALUE | R_HASH_CASELESS);
    body = rAllocBuf(0);
    forms = rAllocHash(0, 0);
    show = 0;
    trace = TRACE_FILTER;

    for (nextArg = 1; nextArg < argc; nextArg++) {
        argp = argv[nextArg];
        if (*argp != '-') {
            break;
        }
        if (smatch(argp, "--all") || smatch(argp, "-a")) {
            show = URL_SHOW_REQ_HEADERS | URL_SHOW_REQ_BODY | URL_SHOW_RESP_HEADERS | URL_SHOW_RESP_BODY;

        } else if (smatch(argp, "--benchmark") || smatch(argp, "-b")) {
            benchmark++;

        } else if (smatch(argp, "--ca")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                caFile = argv[++nextArg];
                if (!rFileExists(caFile)) {
                    rError("url", "Cannot find ca file %s", caFile);
                    return R_ERR_BAD_ARGS;
                }
            }

        } else if (smatch(argp, "--cert")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                certFile = argv[++nextArg];
                if (!rFileExists(certFile)) {
                    rError("url", "Cannot find cert file %s", certFile);
                    return R_ERR_BAD_ARGS;
                }
            }

        } else if (smatch(argp, "--chunk")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                value = argv[++nextArg];
                chunkSize = atoi(value);
                if (chunkSize < 0) {
                    rError("url", "Bad chunksize %d", chunkSize);
                    return R_ERR_BAD_ARGS;
                }
                if (chunkSize > 0) {
                    rAddName(headers, "X-Chunk-Size", SFMT(buf, "%d", chunkSize), 0);
                }
            }

        } else if (smatch(argp, "--cipher") || smatch(argp, "--ciphers")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                ciphers = sclone(argv[++nextArg]);
            }

        } else if (smatch(argp, "--iterations") || smatch(argp, "-i") || smatch(argp, "--count") || smatch(argp, "-c")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                iterations = atoi(argv[++nextArg]);
                if (iterations == 0) {
                    iterations = MAXINT;
                }
            }

        } else if (smatch(argp, "--continue")) {
            continueOnErrors++;

        } else if (smatch(argp, "--cookie")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                rAddName(headers, "Cookie", argv[++nextArg], 0);
            }

        } else if (smatch(argp, "--clients") || smatch(argp, "--fibers")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                clients = atoi(argv[++nextArg]);
                if (clients <= 0 || clients > 1000) {
                    rError("url", "Bad clients argument (1-1000)");
                    return R_ERR_BAD_ARGS;
                }
            }

        } else if (smatch(argp, "--debug") || smatch(argp, "-d")) {
            trace = TRACE_DEBUG_FILTER;

        } else if (smatch(argp, "--header") || smatch(argp, "-h")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                key = argv[++nextArg];
                if ((value = strchr(key, ':')) == 0) {
                    rError("url", "Bad header format. Must be \"key: value\"");
                    return R_ERR_BAD_ARGS;
                }
                *value++ = '\0';
                while (isspace((uchar) *value)) {
                    value++;
                }
                rAddName(headers, key, value, 0);
            }

        } else if (smatch(argp, "--key") || smatch(argp, "-k")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                keyFile = sclone(argv[++nextArg]);
                if (!rFileExists(keyFile)) {
                    rError("url", "Cannot find key file %s", keyFile);
                    return R_ERR_BAD_ARGS;
                }
            }

        } else if (smatch(argp, "--printable")) {
            makePrintable++;

        } else if (smatch(argp, "--protocol") || smatch(argp, "-p")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                value = argv[++nextArg];
                if (scaselessmatch(value, "HTTP/1.0") || smatch(value, "0")) {
                    protocol = 0;
                } else if (scaselessmatch(value, "HTTP/1.1") || smatch(value, "1")) {
                    protocol = 1;
                }
            }

        } else if (smatch(argp, "--quiet") || smatch(argp, "-q")) {
            show = URL_SHOW_NONE;

        } else if (smatch(argp, "--range")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                if (ranges == 0) {
                    ranges = sfmt("bytes=%s", argv[++nextArg]);
                } else {
                    ranges = srejoin(ranges, ",", argv[++nextArg], NULL);
                }
                rAddName(headers, "Range", ranges, R_DYNAMIC_VALUE);
            }

        } else if (smatch(argp, "--redirects")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                maxRedirects = atoi(argv[++nextArg]);
            }

        } else if (smatch(argp, "--retries")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                maxRetries = atoi(argv[++nextArg]);
            }

        } else if (smatch(argp, "--show") || smatch(argp, "-s")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                argp = argv[++nextArg];
                show = 0;
                if (schr(argp, 'H')) {
                    show |= URL_SHOW_REQ_HEADERS;
                }
                if (schr(argp, 'B')) {
                    show |= URL_SHOW_REQ_BODY;
                }
                if (schr(argp, 'h')) {
                    show |= URL_SHOW_RESP_HEADERS;
                }
                if (schr(argp, 'b')) {
                    show |= URL_SHOW_RESP_BODY;
                }
                if (show == 0) {
                    show = URL_SHOW_NONE;
                }
            }

        } else if (smatch(argp, "--save")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                save = sclone(argv[++nextArg]);
            }

#if URL_SSE
        } else if (smatch(argp, "--sse")) {
            sse = 1;
#endif
        } else if (smatch(argp, "--timeout") || smatch(argp, "-T")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                timeout = svaluei(argv[++nextArg]);
            }

        } else if (smatch(argp, "--trace") || smatch(argp, "-t")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                trace = argv[++nextArg];
            }

        } else if (smatch(argp, "--upload") || smatch(argp, "-u")) {
            upload++;
            files = rAllocList(0, 0);

#if URL_AUTH
        } else if (smatch(argp, "--user")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                value = argv[++nextArg];
                username = stok(value, ":", &password);
                if (!username || !password) {
                    rError("url", "Bad user format. Must be \"username:password\"");
                    return R_ERR_BAD_ARGS;
                }
                username = sclone(username);
                password = sclone(password);
            }
#endif

        } else if (smatch(argp, "--verify")) {
            //  Verify the server certificate
            verifyPeer = 1;

        } else if (smatch(argp, "--verbose") || smatch(argp, "-v")) {
            trace = TRACE_VERBOSE_FILTER;
            show = URL_SHOW_REQ_HEADERS | URL_SHOW_RESP_HEADERS;

        } else if (smatch(argp, "--version") || smatch(argp, "-V")) {
            rPrintf("%s\n", ME_VERSION);
            exit(0);

        } else if (smatch(argp, "--webSockets") || smatch(argp, "-w")) {
            webSockets = 1;

        } else if (smatch(argp, "--webSocketsProtocol")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                webSockets = 1;
                webSocketsProtocol = argv[++nextArg];
            }

        } else if (smatch(argp, "--webSocketsSize")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                webSockets = 1;
                webSocketsSize = (size_t) svalue(argv[++nextArg]);
                webSocketsBuffer = prepBuffer(webSocketsSize);
            }

        } else if (smatch(argp, "--zero")) {
            zero++;

        } else if (smatch(argp, "-H")) {
            show = URL_SHOW_RESP_HEADERS;

        } else if (smatch(argp, "--")) {
            nextArg++;
            break;

        } else {
            return usage();
        }
    }
    if (webSockets && upload) {
        rFprintf(stderr, "Cannot use upload with WebSockets\n");
        return usage();
    }
    if (argc == nextArg) {
        return usage();
    }
    argp = argv[nextArg];
    /*
        If the next arg is an uppercase letter, it is a method
     */
    if (isupper(argp[0])) {
        method = argv[nextArg++];
    }
    if (argc == nextArg) {
        return usage();
    }
    url = completeUrl(argv[nextArg++], NULL);

    while (nextArg < argc) {
        item = argv[nextArg++];
        if (*item == '@') {
            //  SECURITY Acceptable: - Must allow relative and absolute paths. Assume caller is trusted.
            path = &item[1];
            //  File
            if (!rFileExists(path)) {
                rError("url", "Cannot locate file %s", path);
                return R_ERR_BAD_ARGS;
            }
            if (upload) {
                rAddItem(files, path);
            } else {
                data = rReadFile(path, NULL);
                rPutStringToBuf(body, data);
                rFree(data);
            }
        } else if (item[0] == '{' /*}*/ &&
                (!rLookupName(headers, "Content-Type") || scontains(rLookupName(headers, "Content-Type"), "json"))
            ) {
            //  Parse JSON|JSON5 and convert to JSON (strict)
            if ((json = jsonParse(item, 0)) == 0) {
                rError("url", "Cannot parse JSON|JSON5");
                return R_ERR_BAD_ARGS;
            }
            data = jsonToString(json, 0, 0, JSON_JSON);
            rPutStringToBuf(body, data);
            rFree(data);
            jsonFree(json);
            if (!method) {
                method = "POST";
            }
            if (!rLookupName(headers, "Content-Type")) {
                rAddName(headers, "Content-Type", "application/json", 0);
            }
        } else if (schr(item, ':') && item[0] != '\\') {
            //  Header
            key = stok(item, ":", &value);
            if (!key || !value) {
                rError("url", "Bad key/value header");
                return R_ERR_BAD_ARGS;
            }
            rAddName(headers, key, value, 0);

        } else if (schr(item, '=') && item[0] != '\\') {
            //  Key=value
            key = stok(item, "=", &value);
            if (upload) {
                rAddName(forms, key, value, 0);
            } else {
                if (rGetBufLength(body) > 0) {
                    rPutToBuf(body, "&");
                }
                rPutToBuf(body, "%s=%s", key, value);
            }
            if (!method) {
                method = "POST";
            }
            if (!rLookupName(headers, "Content-Type")) {
                rAddName(headers, "Content-Type", "application/x-www-form-urlencoded", 0);
            }

        } else {
            //  Literal data
            if (item[0] == '\\') {
                //  Quoted literal
                item++;
            }
            rPutStringToBuf(body, item);
            if (!rLookupName(headers, "Content-Type")) {
                rAddName(headers, "Content-Type", "text/plain", 0);
            }
        }
    }
    /*
        Process arg settings
     */
    if (save && iterations == 1) {
        if ((saveFd = open(save, O_CREAT | O_WRONLY | O_TRUNC | O_TEXT, 0664)) < 0) {
            rError("url", "Cannot open %s", save);
            return R_ERR_BAD_ARGS;
        }
    }
    if ((show == 0 || (show & URL_SHOW_RESP_BODY)) && saveFd < 0) {
        //  Stdout for body
        saveFd = 1;
    }
    if (method == 0) {
        if (rGetBufLength(body) || upload) {
            method = "POST";
        } else if (files) {
            method = "PUT";
        } else {
            method = "GET";
        }
    }
    if (!caFile) {
        if (rFileExists(CA_FILE) ) {
            caFile = CA_FILE;
        } else {
            char *dir = rGetAppDir();
            path = rJoinFile(dir, CA_FILE);
            if (rFileExists(path)) {
                caFile = caPath = path;
            }
            rFree(dir);
        }
    }
    if (timeout) {
        urlSetDefaultTimeout(timeout);
    }
    return 0;
}

static void startClients(void)
{
    int i;

    activeClients = clients;
    for (i = 0; i < clients; i++) {
        rSpawnFiber("url", fiberMain, NULL);
    }
}

static char *prepHeaders(int count)
{
    RBuf  *buf;
    RName *np;

    buf = rAllocBuf(0);
    if (iterations > 0) {
        rPutToBuf(buf, "X-Request: %08d\r\n", completed);
    }
    if (count == 1) {
        rPutStringToBuf(buf, "Connection: close\r\n");
    }
    for (ITERATE_NAMES(headers, np)) {
        rPutToBuf(buf, "%s: %s\r\n", np->name, (char*) np->value);
    }
    if (rGetBufLength(body) > 0) {
        rPutToBuf(buf, "Content-Length: %zd\r\n", rGetBufLength(body));
    }
    return rBufToStringAndFree(buf);
}

static char *prepBuffer(size_t size)
{
    char *buf, *cp, *letters;

    letters = "abcdefghijklmnopqrstuvwxyz";
    buf = rAlloc(size + 1);
    for (cp = buf; cp < buf + size; cp++) {
        *cp = letters[rand() % (uchar) (sizeof(letters) - 1)];
    }
    *cp = '\0';
    return buf;
}

#if URL_SSE
static void sseCallback(Url *up, ssize id, cchar *event, cchar *data, void *arg)
{
    rInfo("url", "SSE Event: %zd, name %s, data: %s", id, event, data);
    if (benchmark) {
        progress(up);
    }
}
#endif

#if ME_COM_WEBSOCK
static void webSocketCallback(WebSocket *ws, int event, cchar *data, size_t len, void *arg)
{
    Url *up = (Url*) arg;
    if (++completed < iterations) {
        if (webSocketsSize > 0) {
            rDebug("url", "Sending: %zd bytes", webSocketsSize);
            webSocketSendBlock(ws, WS_MSG_TEXT, webSocketsBuffer, webSocketsSize);
        } else {
            rDebug("url", "Sending: Message %d", completed);
            webSocketSend(ws, "Message %d", completed);
        }
    } else {
        webSocketSendClose(ws, WS_STATUS_OK, "OK");
    }
    webSocketsReceived = len;
    if (benchmark) {
        progress(up);
    }
}
#endif /* ME_COM_WEBSOCK */

static void fiberMain(void *data)
{
    Url   *up;
    cchar *location;
    char  *headers;
    int   authRetried, redirects;

    if (show == 0) {
        show = URL_SHOW_RESP_BODY;
    }
    /*
        Show body handled locally by this program. 
        No linger to avoid TIME_WAITS when closing the connection. Necessary for load tests and benchmarks.
    */
    up = urlAlloc(show & ~URL_SHOW_RESP_BODY);
#if URL_SSE
    if (sse) {
        urlSetMaxRetries(up, maxRetries);
    }
#endif /* URL_SSE */
    urlSetProtocol(up, protocol);
    if (caFile || keyFile || certFile) {
        rSetSocketDefaultCerts(caFile, keyFile, certFile, NULL);
    }
    rSetSocketDefaultVerify(verifyPeer, verifyIssuer);
    rSetSocketDefaultCiphers(ciphers);

#if URL_AUTH
    // Set authentication credentials if provided
    if (username && password) {
        urlSetAuth(up, username, password, NULL);  // Auto-detect auth type
    }
#endif

    authRetried = 0;
    redirects = 0;

    for (; success && completed < iterations + redirects; completed++) {
        headers = prepHeaders(iterations + redirects - completed);
        if (urlStart(up, method, url) < 0) {
            urlError(up, "Cannot start request");

        } else if (upload) {
            if (urlUpload(up, files, forms, headers) < 0) {
                urlError(up, "Cannot upload files");
            }

        } else if (urlWriteHeaders(up, headers) < 0) {
            urlError(up, "Cannot write headers");

        } else if (rGetBufLength(body) > 0) {
            if (webSockets || sse) {
                urlError(up, "Cannot write body to WebSocket or SSE");
            } else if (urlWrite(up, rGetBufStart(body), rGetBufLength(body)) < 0) {
                urlError(up, "Cannot write body");
            } 
        }
        if (urlFinalize(up) < 0) {
            urlError(up, "Cannot finalize");
        }
        if (up->error) {
            success = 0;

        } else if (webSockets) {
            if (urlGetStatus(up) == URL_CODE_OK) {
                webSocketRun(urlGetWebSocket(up), (WebSocketProc) webSocketCallback, up, up->rx, up->timeout);
            }
#if URL_SSE
        } else if (sse) {
            if (urlGetStatus(up) == URL_CODE_OK) {
                urlSseRun(up, (UrlSseProc) sseCallback, up, up->rx, up->deadline);
            }
#endif /* URL_SSE */
        }
        if (!up->error && 301 <= up->status && up->status <= 308 && redirects < maxRedirects) {
            location = urlGetHeader(up, "Location");
            if (location) {
                rFree(url);
                if (scontains(location, "://")) {
                    url = sclone(location);
                } else {
                    url = sfmt("%s://%s%s", up->scheme, up->host, location);
                }
                redirects++;
            }
            continue;
        }
#if URL_AUTH
        // Handle 401 authentication challenge
        if (up->status == URL_CODE_UNAUTHORIZED && up->username && up->password && !authRetried) {
            if (urlParseAuthChallenge(up)) {
                authRetried = 1;
                redirects++;
                continue;
            }
        }
#endif
        if (up->error || !(webSockets || sse)) {
            getResponse(up, (show & URL_SHOW_RESP_BODY) || show == 0);
        }
        if (!zero && 400 <= up->status && up->status < 600) {
            success = 0;
            if (!continueOnErrors) {
                break;
            }
        }
        if (benchmark) {
            progress(up);
        }
        if (up->error) {
            rError("url", "%s", up->error);
            if (!continueOnErrors) {
                break;
            }
        }
        rFree(headers);
    }
    urlFree(up);
    if (--activeClients <= 0) {
        rStop();
    }
}

static void getResponse(Url *up, bool showBody)
{
    RBuf   *buf;
    char   *formatted, *response, data[ME_BUFSIZE];
    ssize  bytes;
    size_t len;

    buf = showBody ? rAllocBuf(0) : NULL;

    // Read response data - always read to support keep-alive
    while ((bytes = urlRead(up, data, sizeof(data))) > 0) {
        if (buf) {
            rPutBlockToBuf(buf, data, (size_t) bytes);
        }
    }
    if (bytes < 0) {
        urlError(up, "Cannot read response body");
        rFreeBuf(buf);
        return;
    }
    if (!showBody) {
        return;
    }
    // Format and output the response
    len = rGetBufLength(buf);
    response = rBufToStringAndFree(buf);
    if (makePrintable) {
        formatted = formatOutput(response, &len);
        if (formatted != response) {
            rFree(response);
        }
        response = formatted;
    }

    if (saveFd >= 0) {
        if (len > 0) {
            write(saveFd, response, (uint) len);
#if ME_UNIX_LIKE
            if (isatty(saveFd)) {
                write(saveFd, "\n", 1);
            }
#endif
        }
        if (saveFd > 1) {
            close(saveFd);
            saveFd = -1;
        }
    }
    rFree(response);
}

static bool isPort(cchar *name)
{
    cchar *cp;

    for (cp = name; *cp && *cp != '/'; cp++) {
        if (!isdigit((uchar) *cp)) {
            return 0;
        }
    }
    return 1;
}

/*
    Return a fully qualified URL from a relative URL. Handles absolute URLs and relative URLs and URLs with a port.
 */
static char *completeUrl(cchar *url, Url *original)
{
    cchar *proto;

    if (sncaselesscmp(url, "ws://", 5) == 0 || sncaselesscmp(url, "wss://", 6) == 0) {
        webSockets = 1;
        //  Absolute URL extended already
        return sclone(url);
    }
    if (sncaselesscmp(url, "http://", 7) == 0 || sncaselesscmp(url, "https://", 8) == 0) {
        //  Absolute URL extended already
        return sclone(url);
    }
    if (original) {
        return sfmt("%s://%s%s", original->scheme, original->host, url);
    }
    proto = webSockets ? "ws" : "http";
    
    /*
        Handle special partial URLs like :port/url, port/url, hostname/url and /url
    */
    if (*url == '/') {
        return sfmt("%s://localhost%s", proto, url);
    } else if (*url == ':' && isPort(&url[1])) {
        // :port/url
        return sfmt("%s://localhost%s", proto, url);
    } else if (isPort(url)) {
        //  port/url
        return sfmt("%s://localhost:%s", proto, url);
    } else {
        //  hostname/url
        return sfmt("%s://%s", proto, url);
    }
}

static char *formatOutput(char *response, size_t *count)
{
    RBuf *buf;
    size_t i;
    int  isBinary;

    isBinary = 0;
    for (i = 0; i < *count; i++) {
        if (!isprint((uchar) response[i]) && response[i] != '\n' && response[i] != '\r' && response[i] != '\t') {
            isBinary = 1;
            break;
        }
    }
    if (!isBinary) {
        return response;
    }
    buf = rAllocBuf(*count * 3 + 1);
    for (i = 0; i < *count; i++) {
        rPutToBuf(buf, "%02x ", response[i] & 0xff);
    }
    *count *= 3;
    return rBufToStringAndFree(buf);
}

static void progress(Url *up)
{
    RBuf  *response;
    cchar *uri;
    size_t len;

    uri = url;
    if (sncaselesscmp(uri, "http://", 7) == 0) {
        uri += 7;
    } else if (sncaselesscmp(uri, "https://", 8) == 0) {
        uri += 8;
    }
    if ((completed % 10000) == 1) {
        if (completed == 1 || (completed % 500000) == 1) {
            if (completed > 1) {
                rPrintf("\n");
            }
            rPrintf("Fiber         Count  Op  Status   Bytes  Url\n");
        }
        len = 0;
        if (webSockets) {
            len = webSocketsReceived;
        } else {
            if ((response = urlGetResponseBuf(up)) != 0) {
                len = rGetBufLength(response);
            }
        }
        rPrintf("%p %7d %4s  %5d %7lld  %s\n", rGetFiber(), completed - 1, method, up->status, len, uri);
    }
}

static void report(Time start)
{
    double elapsed;

    if (!benchmark) {
        return;
    }
    if (!success) {
        rPrintf("No benchmark results due to errors\n");
        return;
    }
    elapsed = (double) (rGetTime() - start);
    if (completed == 0) {
        elapsed = 0;
        completed = 1;
    }
    rPrintf("\nClients:        %13d\n", clients);
    rPrintf("Request Count:  %13d\n", completed);
    rPrintf("Time elapsed:        %13.4f sec\n", elapsed / 1000.0);
    rPrintf("Time per request:    %13.4f sec\n", elapsed / 1000.0 / completed);
    rPrintf("Requests per second: %13.4f\n\n", completed * 1.0 / (elapsed / 1000.0));
}
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
