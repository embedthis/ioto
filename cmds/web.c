/*
    web.c - Web server test main program

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#define ME_COM_WEB 1

#include "web.h"

/*********************************** Locals ***********************************/

/*
    Default trace filters
 */
#define TRACE_FILTER         "stderr:raw,error,info,!debug:all,!mbedtls"
#define TRACE_VERBOSE_FILTER "stderr:raw,error,info,trace,!debug:all,!mbedtls"
#define TRACE_DEBUG_FILTER   "stderr:all:all,!mbedtls"
#define TRACE_FORMAT         "%S: %T: %M"
#define DEFAULT_CONFIG \
        "{\
    web: {\
        documents: 'web',\
        listen: ['http://:80', 'https://:443'],\
        routes: [\
            { match: '', handler: 'file' }\
        ],\
        show: 'hH'\
    }\
}"

static WebHost *host;               /* Single serving host */
static cchar   *trace = 0;          /* Module trace logging */
static Json    *config;             /* web.json5 contents */
static cchar   *event;              /* Exit event */
static char    *endpoint;           /* Listen endpoint override */
static int     show;                /* Request / response elements to display */

/*********************************** Forwards *********************************/

static void dropPrivileges(cchar *username, cchar *groupname);
static void onExit(void *data, void *arg);
static int parseShow(cchar *arg);
static void setEvent(void);
static void setLog(Json *config);
static void start(void);
static void stop(void);

/************************************* Code ***********************************/

static int usage(void)
{
    rFprintf(stderr, "\nweb usage:\n\n"
             "  web [options] [endpoint]...\n"
             "  Options:\n"
             "    --debug                  # Emit debug logging\n"
             "    --exit event|seconds     # Exit on event or after 'seconds'\n"
             "    --quiet                  # Don't output headers. Alias for --show ''\n"
             "    --show [HBhb]            # Show request headers/body (HB) and response headers/body (hb).\n"
             "    --timeouts               # Disable timeouts for debugging\n"
             "    --trace file[:type:from] # Trace to file (stdout:all:all)\n"
             "    --verbose                # Verbose operation. Alias for --show Hhb plus module trace.\n"
             "    --version                # Output version information\n\n");
    return R_ERR_BAD_ARGS;
}

int main(int argc, char **argv)
{
    cchar *arg, *argp;
    int   argind;

    endpoint = 0;
    event = 0;
    trace = TRACE_FILTER;

    for (argind = 1; argind < argc; argind++) {
        argp = argv[argind];
        if (*argp != '-') {
            break;

        } else if (smatch(argp, "--debug") || smatch(argp, "-d")) {
            trace = TRACE_DEBUG_FILTER;

        } else if (smatch(argp, "--exit")) {
            if (argind + 1 >= argc) {
                return usage();
            }
            event = argv[++argind];

        } else if (smatch(argp, "--quiet") || smatch(argp, "-q")) {
            show = WEB_SHOW_NONE;

        } else if (smatch(argp, "--listen") || smatch(argp, "-l")) {
            if (argind + 1 >= argc) {
                return usage();
            } else {
                //  Review Acceptable: repeat --listen override previous listen endpoints
                arg = argv[++argind];
                if (scontains(arg, "http://")) {
                    endpoint = sfmt("%s", argv[argind]);
                } else if (snumber(arg)) {
                    endpoint = sfmt("http://:%s", argv[argind]);
                } else {
                    endpoint = sfmt("http://%s", argv[argind]);
                }
            }
        } else if (smatch(argp, "--show") || smatch(argp, "-s")) {
            if (argind + 1 >= argc) {
                return usage();
            } else {
                show = parseShow(argv[++argind]);
            }

        } else if (smatch(argp, "--timeouts") || smatch(argp, "-T")) {
            //  Disable timeouts for debugging
            rSetTimeouts(0);

        } else if (smatch(argp, "--trace") || smatch(argp, "-t")) {
            if (argind + 1 >= argc) {
                return usage();
            }
            trace = argv[++argind];

        } else if (smatch(argp, "--verbose") || smatch(argp, "-v")) {
            if (argind + 1 >= argc) {
                return usage();
            }
            trace = TRACE_VERBOSE_FILTER;
            show = WEB_SHOW_REQ_HEADERS | WEB_SHOW_RESP_HEADERS;

        } else if (smatch(argp, "--version") || smatch(argp, "-V")) {
            rPrintf("%s\n", ME_VERSION);
            exit(0);

        } else {
            return usage();
        }
    }
    if (argind < argc) {
        endpoint = sfmt("%s", argv[argind++]);
    }
    if (rInit((RFiberProc) start, 0) < 0) {
        rFprintf(stderr, "web: Cannot initialize runtime\n");
        exit(1);
    }
    setEvent();
    rServiceEvents();

    rFree(endpoint);
    stop();
    rTerm();
    return 0;
}

static void start(void)
{
    if ((config = jsonParseFile("web.json5", NULL, 0)) == NULL) {
        if ((config = jsonParse(DEFAULT_CONFIG, 0)) == 0) {
            rError("web", "Cannot parse config file \"web.json5\"");
            exit(1);
        }
    }
    if (endpoint) {
        jsonSetJsonFmt(config, 0, "web", "{listen: ['%s']}", endpoint);
    }
    setLog(config);
    if (!show) {
        show = parseShow(jsonGet(config, 0, "log.show", getenv("WEB_SHOW")));
        if (!show) {
            show = WEB_SHOW_NONE;
        }
    }
    webInit();
    if ((host = webAllocHost(config, show)) == NULL) {
        rError("web", "Cannot allocate host");
        exit(1);
    }
#if ME_DEBUG
    webTestInit(host, "/test");
#endif
    webStartHost(host);

    //  Drop privileges after binding to ports
    if (getuid() == 0) {
        cchar *user = jsonGet(config, 0, "web.user", "nobody");
        cchar *group = jsonGet(config, 0, "web.group", "nobody");
        if (user || group) {
            rInfo("web", "Dropping privileges to %s:%s", user, group);
            dropPrivileges(user, group);
        }
    }
}

static void stop(void)
{
    webStopHost(host);
    webFreeHost(host);
    webTerm();
    jsonFree(config);
}

static void setEvent(void)
{
    Ticks delay;

    if (!event) return;
    if (snumber(event)) {
        delay = (int) stoi(event) * TPS;
        if (delay == 0) {
            rStop();
            exit(0);
        } else {
            rStartEvent((REventProc) onExit, 0, stoi(event) * TPS);
        }
    } else {
        rWatch(event, (RWatchProc) onExit, 0);
    }
}

static void setLog(Json *config)
{
    cchar *format, *path, *sources, *types;

    if (trace) {
        if (rSetLog(trace, NULL, 1) < 0) {
            rError("web", "Cannot open log %s", trace);
            exit(1);
        }
        rSetLogFormat(TRACE_FORMAT, 1);
    } else {
        path = jsonGet(config, 0, "log.path", 0);
        format = jsonGet(config, 0, "log.format", 0);
        types = jsonGet(config, 0, "log.types", 0);
        sources = jsonGet(config, 0, "log.sources", 0);
        if (path && rSetLogPath(path, 1) < 0) {
            rError("web", "Cannot open log %s", path);
            exit(1);
        }
        if (types || sources) {
            rSetLogFilter(types, sources, 0);
        }
        if (format) {
            rSetLogFormat(format, 0);
        }
    }
}

static int parseShow(cchar *arg)
{
    int show;

    show = 0;
    if (!arg) {
        return show;
    }
    if (schr(arg, 'H')) {
        show |= WEB_SHOW_REQ_HEADERS;
    }
    if (schr(arg, 'B')) {
        show |= WEB_SHOW_REQ_BODY;
    }
    if (schr(arg, 'h')) {
        show |= WEB_SHOW_RESP_HEADERS;
    }
    if (schr(arg, 'b')) {
        show |= WEB_SHOW_RESP_BODY;
    }
    return show;
}

static void onExit(void *data, void *arg)
{
    rInfo("main", "Exiting");
    rWatchOff(event, (RWatchProc) onExit, 0);
    rStop();
}

static void dropPrivileges(cchar *username, cchar *groupname)
{
#if !ME_WIN_LIKE
    struct group  *grp;
    struct passwd *pwd;

    if (groupname) {
        grp = getgrnam(groupname);
        if (grp == NULL) {
            rError("web", "Cannot find group '%s'", groupname);
            exit(1);
        }
        if (setgid(grp->gr_gid) < 0) {
            rError("web", "Cannot set group to '%s'. See error %d", groupname, errno);
            exit(1);
        }
    }
    if (username) {
        pwd = getpwnam(username);
        if (pwd == NULL) {
            rError("web", "Cannot find user '%s'", username);
            exit(1);
        }
        if (setuid(pwd->pw_uid) < 0) {
            rError("web", "Cannot set user to '%s'. See error %d", username, errno);
            exit(1);
        }
    }
#else
    rWarn("web", "Cannot drop privileges on this platform");
#endif
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
