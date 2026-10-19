/*
    web.c - Web server test main program

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#define ME_COM_WEB 1

#include "web.h"

#if MACOSX
//  Darwin declares the initgroups group as int
    #define INITGROUPS_GID(gid) ((int) (gid))
#else
    #define INITGROUPS_GID(gid) (gid)
#endif

/*********************************** Locals ***********************************/

/*
    Default trace filters
 */
#define TRACE_FILTER         "stderr:raw,error,info,!debug:all,!mbedtls"
#define TRACE_VERBOSE_FILTER "stderr:raw,error,info,trace,!debug:all,!mbedtls"
#define TRACE_DEBUG_FILTER   "stderr:all:all,!mbedtls"
#define TRACE_FORMAT         "%S: %T: %M"

#define WEB_FIBER_RESERVE    8      /* Fibers reserved beyond web.limits.connections */

static WebHost *host;               /* Single serving host */
static cchar   *trace = 0;          /* Module trace logging specification */
static Json    *config;             /* Parsed web.json5 configuration */
static cchar   *event;              /* Exit event trigger (file path or seconds) */
static char    *endpoint;           /* Listen endpoint override from command line */
static int     show;                /* Request/response display flags (WEB_SHOW_*) */
static bool    background;          /* Run as background daemon process */
static char    *homeDir;            /* Working directory to change to on startup */
static char    *configPath;         /* Custom path to web.json5 config file */
static char    *profile;            /* Execution profile (dev, prod) for config selection */
static int     exitStatus;          /* Process exit status, set non-zero when the host cannot start */
#if ME_WEB_TEST_ACTIONS
static bool testActions;            /* Register the unauthenticated /test action table */
#endif

/*********************************** Forwards *********************************/

#if ME_UNIX_LIKE
static void dropPrivileges(cchar *username, cchar *groupname);
#endif
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
             "    --background             # Daemonize and run in the background\n"
             "    --config path            # Set the path for the web.json5 config\n"
             "    --debug                  # Emit debug logging\n"
             "    --exit event|seconds     # Exit on event or after 'seconds'\n"
             "    --home directory         # Change to directory to run\n"
             "    --profile profile        # Select execution profile for web.json5 (dev,prod)\n"
             "    --quiet                  # Don't output headers. Alias for --show ''\n"
             "    --show [HBhb]            # Show request headers/body (HB) and response headers/body (hb).\n"
#if ME_WEB_TEST_ACTIONS
             "    --test-actions           # Register the UNAUTHENTICATED /test routes. Never on a network.\n"
#endif
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

    //  Initialize command-line option variables
    endpoint = 0;
    event = 0;
    trace = TRACE_FILTER;
    background = 0;
    homeDir = 0;
    configPath = 0;
    profile = 0;

    //  Parse command-line arguments
    for (argind = 1; argind < argc; argind++) {
        argp = argv[argind];
        if (*argp != '-') {
            break;

        } else if (smatch(argp, "--background") || smatch(argp, "-b")) {
            background = 1;

        } else if (smatch(argp, "--config") || smatch(argp, "-c")) {
            if (argind + 1 >= argc) {
                return usage();
            }
            configPath = argv[++argind];

        } else if (smatch(argp, "--debug") || smatch(argp, "-d")) {
            trace = TRACE_DEBUG_FILTER;

        } else if (smatch(argp, "--exit")) {
            if (argind + 1 >= argc) {
                return usage();
            }
            event = argv[++argind];

        } else if (smatch(argp, "--home") || smatch(argp, "-h")) {
            if (argind + 1 >= argc) {
                return usage();
            }
            homeDir = argv[++argind];

        } else if (smatch(argp, "--profile") || smatch(argp, "-p")) {
            if (argind + 1 >= argc) {
                return usage();
            }
            profile = argv[++argind];

        } else if (smatch(argp, "--quiet") || smatch(argp, "-q")) {
            show = WEB_SHOW_NONE;

        } else if (smatch(argp, "--listen") || smatch(argp, "-l")) {
            if (argind + 1 >= argc) {
                return usage();
            } else {
                //  SECURITY Acceptable:: repeat --listen override previous listen endpoints
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

#if ME_WEB_TEST_ACTIONS
        } else if (smatch(argp, "--test-actions")) {
            testActions = 1;
#endif

        } else if (smatch(argp, "--timeouts") || smatch(argp, "-T")) {
            //  Disable timeouts for debugging
            rSetTimeouts(0);

        } else if (smatch(argp, "--trace") || smatch(argp, "-t")) {
            if (argind + 1 >= argc) {
                return usage();
            }
            trace = argv[++argind];

        } else if (smatch(argp, "--verbose") || smatch(argp, "-v")) {
            trace = TRACE_VERBOSE_FILTER;
            show = WEB_SHOW_REQ_HEADERS | WEB_SHOW_RESP_HEADERS;

        } else if (smatch(argp, "--version") || smatch(argp, "-V")) {
            rPrintf("%s\n", ME_VERSION);
            exit(0);

        } else {
            return usage();
        }
    }
    //  Remaining argument is endpoint override
    if (argind < argc) {
        endpoint = sfmt("%s", argv[argind++]);
    }

    //  Change to home directory if specified
    if (homeDir && chdir(homeDir) < 0) {
        rFprintf(stderr, "web: Cannot change to directory: %s\n", homeDir);
        exit(1);
    }

#if ME_UNIX_LIKE
    //  Daemonize process if running in background
    if (background) {
        rDaemonize();
    }
#endif
    //  Initialize runtime and start web server fiber
    if (rInit((RFiberProc) start, 0) < 0) {
        rFprintf(stderr, "web: Cannot initialize runtime\n");
        exit(1);
    }

    //  Setup exit event handler and run event loop
    setEvent();
    rServiceEvents();

    //  Cleanup on exit
    rFree(endpoint);
    stop();
    rTerm();
    return exitStatus;
}

/*
    Start the web server fiber
    This is the main server initialization routine called from the runtime fiber
 */
static void start(void)
{
    cchar  *path;
    char   *errorMsg = 0;
    size_t stackInitial, stackMax, stackGrow, stackReset;
    int    maxFibers, poolMin, poolMax;

    //  A missing or unparseable config is fatal. Its routes are the only authorization gate.
    path = configPath ? configPath : "web.json5";
    if ((config = jsonParseFile(path, &errorMsg, 0)) == NULL) {
        rError("web", "Cannot load config file \"%s\": %s", path, errorMsg ? errorMsg : "cannot read file");
        rFree(errorMsg);
        exit(1);
    }

    //  Set execution profile (dev, prod) if specified
    if (profile) {
        jsonSetString(config, 0, "profile", profile);
    }
    maxFibers = webGetFiberLimit(config, WEB_FIBER_RESERVE);
    poolMin = (int) svalue(jsonGet(config, 0, "limits.fiberPoolMin", "2"));
    poolMax = (int) svalue(jsonGet(config, 0, "limits.fiberPoolMax", "4"));
    rSetFiberLimits(maxFibers, poolMin, poolMax);

    //  Configure fiber stack limits if specified. A value of zero keeps the default.
    stackInitial = (size_t) svalue(jsonGet(config, 0, "limits.fiberStack", "0"));
    if (stackInitial == 0) {
        // Backwards compatibility with old "limits.stack" property.
        stackInitial = (size_t) svalue(jsonGet(config, 0, "limits.stack", "0"));
    }
    stackMax = (size_t) svalue(jsonGet(config, 0, "limits.fiberStackMax", "0"));
    stackGrow = (size_t) svalue(jsonGet(config, 0, "limits.fiberStackGrow", "0"));
    stackReset = (size_t) svalue(jsonGet(config, 0, "limits.fiberStackReset", "0"));
    rSetFiberStackLimits(stackInitial, stackMax, stackGrow, stackReset);

    //  Override listen endpoints if specified on command line
    if (endpoint) {
        jsonSetJsonFmt(config, 0, "web", "{listen: ['%s']}", endpoint);
    }

    //  Configure logging based on config or command-line trace
    setLog(config);

    //  Determine which request/response elements to display
    if (!show) {
        show = parseShow(jsonGet(config, 0, "log.show", getenv("WEB_SHOW")));
        if (!show) {
            show = WEB_SHOW_NONE;
        }
    }

    //  Initialize web module and create host
    webInit();
    if ((host = webAllocHost(config, show)) == NULL) {
        rError("web", "Cannot allocate host");
        exit(1);
    }

    /*
        The /test action table is unauthenticated by construction - every action is added with a
        null role - and it dumps request headers including Authorization and Cookie, crashes the
        process on demand, and writes uploads relative to the working directory.

        It used to be registered whenever ME_DEBUG was set, which the documented "make" defaults
        to, so building this tool the ordinary way and running it produced a server exposing all
        of that to any unauthenticated client. Being compiled in is not a reason to be reachable:
        it now takes an explicit --test-actions, and the test suite calls webTestInit itself.
     */
#if ME_WEB_TEST_ACTIONS
    if (testActions) {
        rError("web", "Registering the UNAUTHENTICATED /test routes - do not expose this server");
        webTestInit(host, "/test");
    }
#endif

    //  Start listening and accepting connections
    if (webStartHost(host) < 0) {
        rError("web", "Cannot start host");

        exitStatus = 1;
        rStop();
        return;
    }

#if ME_UNIX_LIKE
    //  Drop root privileges after binding to privileged ports. A setuid-root start has euid 0 only.
    if (geteuid() == 0) {
        cchar *user = jsonGet(config, 0, "web.user", ME_WEB_USER);
        //  No default group: an unconfigured group is the user's own, which dropPrivileges resolves
        cchar *group = jsonGet(config, 0, "web.group", NULL);
        if (user || group) {
            rInfo("web", "Dropping privileges to %s:%s", user, group ? group : "the user's own group");
            dropPrivileges(user, group);
        }
    }
#else

    if (jsonGet(config, 0, "web.user", NULL) || jsonGet(config, 0, "web.group", NULL)) {
        rError("web", "web.user and web.group are not supported on this platform and are NOT applied");
    }
#endif
}

/*
    Stop and cleanup the web server
 */
static void stop(void)
{
    webStopHost(host);
    webFreeHost(host);
    webTerm();
    jsonFree(config);
}

/*
    Setup exit event handler
    Supports exit after a timeout in seconds or when a watched file/event changes
 */
static void setEvent(void)
{
    Ticks delay;

    if (!event) return;

    if (snumber(event)) {
        //  Exit after specified number of seconds
        delay = (int) stoi(event) * TPS;
        if (delay == 0) {
            rStop();
            exit(0);
        } else {
            rStartEvent((REventProc) onExit, 0, stoi(event) * TPS);
        }
    } else {
        //  Exit when watched file/event changes
        rWatch(event, (RWatchProc) onExit, 0);
    }
}

/*
    Configure logging from config file or command-line trace specification
    Command-line trace takes precedence over config file settings
 */
static void setLog(Json *config)
{
    cchar *format, *path, *sources, *types;

    if (trace) {
        //  Use command-line trace specification
        if (rSetLog(trace, NULL, 1) < 0) {
            rError("web", "Cannot open log %s", trace);
            exit(1);
        }
        rSetLogFormat(TRACE_FORMAT, 1);
    } else {
        //  Use configuration file log settings
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

/*
    Parse --show argument to determine which request/response elements to display
    H = request headers, B = request body, h = response headers, b = response body
 */
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

/*
    Exit event handler callback
 */
static void onExit(void *data, void *arg)
{
    (void) data;
    (void) arg;

    rInfo("main", "Exiting");
    rWatchOff(event, (RWatchProc) onExit, 0);
    rStop();
}

/*
    Drop root privileges to specified user and group
    Should be called after binding to privileged ports (< 1024)
 */
#if ME_UNIX_LIKE
static void dropPrivileges(cchar *username, cchar *groupname)
{
    struct group  *grp;
    struct passwd *pwd;
    gid_t         gid;
    bool          haveGid;

    pwd = NULL;
    gid = 0;
    haveGid = 0;

    if (username) {
        if ((pwd = getpwnam(username)) == NULL) {
            rError("web", "Cannot find user '%s'", username);
            exit(1);
        }
        /*
            An unconfigured group is the user's own primary group, not a group that happens to
            share the user's name. Debian and Ubuntu give "nobody" the group "nogroup" and define
            no "nobody" group at all, so assuming the latter stopped a root-started server from
            starting on the commonest Linux family. Red Hat and the BSDs do define nobody:nobody,
            which is why it went unnoticed.
         */
        gid = pwd->pw_gid;
        haveGid = 1;
    }
    if (groupname) {
        if ((grp = getgrnam(groupname)) == NULL) {
            rError("web", "Cannot find group '%s'", groupname);
            exit(1);
        }
        gid = grp->gr_gid;
        haveGid = 1;
    }
    /* Replace root's supplementary groups before dropping privileges. */
    if (pwd ? initgroups(pwd->pw_name, INITGROUPS_GID(gid)) < 0 : (haveGid && setgroups(1, &gid) < 0)) {
        rError("web", "Cannot set supplementary groups. See error %d", errno);
        exit(1);
    }
    //  Group first: after setuid the process can no longer change its group
    if (haveGid && setgid(gid) < 0) {
        rError("web", "Cannot set group to gid %d. See error %d", (int) gid, errno);
        exit(1);
    }
    if (pwd && setuid(pwd->pw_uid) < 0) {
        rError("web", "Cannot set user to '%s'. See error %d", username, errno);
        exit(1);
    }
    //  The drop must be irreversible
    if (pwd && pwd->pw_uid != 0 && setuid(0) == 0) {
        rError("web", "Privileges could be regained after dropping to '%s'", username);
        exit(1);
    }
}
#endif

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
