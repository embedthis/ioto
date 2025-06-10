/*
    main.c - Ioto Agent main program

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

/*********************************** Locals ***********************************/

static cchar *trace;        /* General log trace */
static cchar *exitEvent;    /* Exit event */

/*
    Default trace filters
 */
#define TRACE_FILTER         "stderr:raw,error,info,!debug:all,!mbedtls"
#define TRACE_VERBOSE_FILTER "stdout:raw,error,info,trace,!debug:all,!mbedtls"
#define TRACE_DEBUG_FILTER   "stdout:all:all,!mbedtls"
#define TRACE_FORMAT         "%S: %T: %M"
#define IOTO_TEST_DURATION   "180"

#define usage() showUsage(__LINE__)

/************************************ Forwards ********************************/

static void onExit(void *data, void *arg);
static void setEvent(cchar *event);

/************************************* Code ***********************************/

static void showUsage(int line)
{
    rPrintf("\nIoto Agent usage:\n\n"
            "  ioto [options]\n"
            "  Options:\n"
            "    --account ID              # Manager account for self-claiming\n"
            "    --background              # Daemonize and run in the background\n"
            "    --cloud ID                # Cloud ID for self-claiming\n"
            "    --config dir              # Set the directory for config files and ioto.json5\n"
            "    --count Num               # Count of unit test iterations\n"
            "    --debug                   # Emit debug tracing\n"
            "    --exit event|seconds      # Exit on event or after 'seconds'\n"
            "    --gen                     # Generate a UID \n"
            "    --home directory          # Change to directory to run\n"
            "    --id UCI                  # Device claim ID. Overrides device.json5\n"
            "    --ioto path               # Set the path for the ioto.json5 config\n"
            "    --nosave                  # Run in-memory and do not save state\n"
            "    --product Token           # Product claim ID. Overrides device.json5\n"
            "    --profile profile         # Select execution profile from ioto.json5 (dev,prod)\n"
            "    --quiet                   # Don't show web server headers. Alias for --show ''\n"
            "    --reset                   # Reset state to factory defaults\n"
            "    --show [HBhb]             # Show request headers/body (HB) and response headers/body (hb).\n"
            "    --state dir               # Set the state directory\n"
            "    --sync up|down|both       # Force a database sycn with the cloud\n"
            "    --test suite              # Run Unit test suite in the Unit app (see test.json5)\n"
            "    --timeouts                # Disable timeouts for debugging\n"
            "    --trace file[:type:from]  # Trace to file (stdout:all:all)\n"
            "    --verbose                 # Verbose operation. Alias for --show Hhb plus module trace.\n"
            "    --version                 # Output version information\n\n");
    exit(1);
}

/*
    Must not block or call any fiber functions here
 */
int main(int argc, char **argv)
{
    cchar *argp, *home, *show;
    int   argind, background;

    background = 0;
    home = 0;

    /*
        Initialize the safe runtime
     */
    if (rInit(NULL, NULL) < 0) {
        rFprintf(stderr, "Cannot initialize runtime");
        exit(2);
    }
    /*
        Allocate the primary Ioto control object
     */
    ioAlloc();

    ioto->cmdProfile = getenv("IOTO_PROFILE");
    show = getenv("IOTO_SHOW");

    /*
        Parse command line arguments
     */
    for (argind = 1; argind < argc; argind++) {
        argp = argv[argind];
        if (*argp != '-') {
            break;
        }
        if (smatch(argp, "--background") || smatch(argp, "-b")) {
            background = 1;

        } else if (smatch(argp, "--config")) {
            if (argind + 1 >= argc) {
                usage();
            }
            ioto->cmdConfigDir = sclone(argv[++argind]);

        } else if (smatch(argp, "--count") || smatch(argp, "-c")) {
            if (argind + 1 >= argc) {
                usage();
            }
            ioto->cmdCount = (int) stoi(argv[++argind]);

        } else if (smatch(argp, "--debug") || smatch(argp, "-d")) {
            trace = TRACE_DEBUG_FILTER;
            show = "hH";

        } else if (smatch(argp, "--exit")) {
            if (argind + 1 >= argc) {
                usage();
            }
            exitEvent = argv[++argind];

        } else if (smatch(argp, "--id")) {
            if (argind + 1 >= argc) {
                usage();
            }
            ioto->cmdId = sclone(argv[++argind]);
            if (slen(ioto->cmdId) > 20) {
                rError("main", "Device ID must be less than 20 characters");
                exit(1);
            }

        } else if (smatch(argp, "--ioto")) {
            if (argind + 1 >= argc) {
                usage();
            }
            ioto->cmdIotoFile = argv[++argind];

        } else if (smatch(argp, "--gen")) {
            rPrintf("%s\n", cryptID(10));
            exit(0);

        } else if (smatch(argp, "--home")) {
            if (argind + 1 >= argc) {
                usage();
            }
            home = argv[++argind];

        } else if (smatch(argp, "--nosave")) {
            ioto->nosave = 1;

        } else if (smatch(argp, "--product")) {
            if (argind + 1 >= argc) {
                usage();
            }
            ioto->cmdProduct = argv[++argind];

        } else if (smatch(argp, "--profile")) {
            if (argind + 1 >= argc) {
                usage();
            }
            ioto->cmdProfile = argv[++argind];

        } else if (smatch(argp, "--quiet") || smatch(argp, "-q")) {
            show = "";

        } else if (smatch(argp, "--reset")) {
            ioto->cmdReset = 1;

        } else if (smatch(argp, "--show") || smatch(argp, "-s")) {
            //  Show (trace) HTTP requests and responses
            if (argind + 1 >= argc) {
                usage();
            } else {
                show = argv[++argind];
            }

        } else if (smatch(argp, "--state")) {
            //  Set an alternate state directory
                if (argind + 1 >= argc) {
                usage();
            }
            ioto->cmdStateDir = sclone(argv[++argind]);

        } else if (smatch(argp, "--sync")) {
            if (argind + 1 >= argc) {
                usage();
            }
            ioto->cmdSync = argv[++argind];

        } else if (smatch(argp, "--test")) {
            if (argind + 1 >= argc) {
                usage();
            }
            ioto->cmdTest = argv[++argind];
            if (!exitEvent) {
                exitEvent = IOTO_TEST_DURATION;
            }

        } else if (smatch(argp, "--timeouts") || smatch(argp, "-T")) {
            //  Disable timeouts for debugging
            rSetTimeouts(0);

        } else if (smatch(argp, "--trace") || smatch(argp, "-t")) {
            if (argind + 1 >= argc) {
                usage();
            }
            trace = argv[++argind];

        } else if (smatch(argp, "--verbose") || smatch(argp, "-v")) {
            if (!smatch(trace, TRACE_DEBUG_FILTER)) {
                trace = TRACE_VERBOSE_FILTER;
                show = "hH";
            }

        } else if (smatch(argp, "--version") || smatch(argp, "-V")) {
            rPrintf("%s\n", ME_VERSION);
            exit(0);

#if SERVICES_CLOUD
        } else if (smatch(argp, "--account")) {
            //  Define a manager account to auto-register the device with
            if (argind + 1 >= argc) {
                usage();
            }
            ioto->cmdAccount = argv[++argind];

        } else if (smatch(argp, "--cloud")) {
            //  Define a builder cloud to auto-register the device with
            if (argind + 1 >= argc) {
                usage();
            }
            ioto->cmdCloud = argv[++argind];
#endif
        } else {
            usage();
        }
    }
    ioto->cmdAIShow = show;
    ioto->cmdWebShow = show;

    setEvent(exitEvent);

    if (home && chdir(home) < 0) {
        rFprintf(stderr, "ioto: Cannot change directory to %s\n", home);
        exit(1);
    }
    umask(077);

    /*
        Logging specified in ioto.json5 may be overridden via the command line.
        Until ioUpdateLog is called, trace goes to stdout by default.
     */
    if (trace) {
        if (rSetLog(trace, 0, 1) < 0) {
            rError("app", "Cannot open trace %s", trace);
            exit(1);
        }
        rSetLogFormat(TRACE_FORMAT, 1);
    }
    if (background) {
        rDaemonize();
    }

    /*
        Service events until instructed to stop. Handles restarts.
     */
    ioRun(ioStart);

    rTerm();
    return 0;
}

/*
    Define an exit event (condition). Used just for testing.
 */
static void setEvent(cchar *event)
{
    int delay;

    if (event) {
        if (snumber(event)) {
            delay = (int) stoi(event) * TPS;
            if (delay == 0) {
                ioTerm();
                rStop();
                exit(0);
            } else {
                rStartEvent((REventProc) onExit, 0, stoi(event) * TPS);
            }
        } else {
            rWatch(event, (RWatchProc) onExit, 0);
        }
    }
}

/*
    OnExit event handler
 */
static void onExit(void *data, void *arg)
{
#if SERVICES_SYNC
    ioFlushSync(1);
#endif
    rWatchOff(exitEvent, (RWatchProc) onExit, 0);
    rInfo("main", "Exiting");
    rStop();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
