/*
    log.tst.c - Unit tests for the logging module

    Covers the filter, format, handler, destination and rotation surface. Everything that would
    otherwise reach stdout is captured by a test handler, so a passing run is silent.

    The TestMe comparison macros expand their arguments twice, so every call with a side effect
    is bound to a local before it is asserted.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/*********************************** Locals ***********************************/

#define LOG_SIZE (2 * 1024 * 1024)     /* ME_R_LOG_SIZE - the rotation threshold */
#define LOG_COUNT 5                    /* ME_R_LOG_COUNT - the number of backups kept */

#if ME_WIN_LIKE
    #define NL "\r\n"                  /* Log files are written in text mode */
#else
    #define NL "\n"
#endif

static RBuf *captured;                 /* Messages seen by the capture handler */
static char lastType[64];
static char lastSource[64];
static int  captureCount;

static char logPath[ME_MAX_FNAME];     /* Unique per process - tests run in parallel */
static char backupPath[ME_MAX_FNAME];

/************************************ Code ************************************/

static void captureHandler(cchar *type, cchar *source, cchar *msg)
{
    captureCount++;
    scopy(lastType, sizeof(lastType), type ? type : "");
    scopy(lastSource, sizeof(lastSource), source ? source : "");
    rFlushBuf(captured);
    rPutStringToBuf(captured, msg ? msg : "");
}


static void resetCapture(void)
{
    captureCount = 0;
    lastType[0] = '\0';
    lastSource[0] = '\0';
    rFlushBuf(captured);
}


static cchar *capturedText(void)
{
    return rBufToString(captured);
}


/*
    Error code names. The table is indexed by the absolute value of the code, so both ends and
    everything outside the table must be handled.
 */
static void errorCodes(void)
{
    cchar *msg;

    msg = rGetError(0);
    tmatch(msg, "R_ERR_OK");
    msg = rGetError(-R_ERR_BAD_ARGS);
    tmatch(msg, "R_ERR_BAD_ARGS");
    msg = rGetError(-R_ERR_WONT_FIT);
    tmatch(msg, "R_ERR_WONT_FIT");

    /*
        Past both ends of the table.
     */
    msg = rGetError(-1);
    tmatch(msg, "Unknown error");
    msg = rGetError(9999);
    tmatch(msg, "Unknown error");
}


/*
    The OS error accessors. rGetOsError maps to a portable code; on Unix it is errno unchanged.
 */
static void osErrors(void)
{
    int err;

    /*
        rGetRawOsError is the unmapped code. On Unix it is errno, so it agrees with rGetOsError;
        on Windows the two differ where a native code maps onto a Posix one.
     */
    //  rSetOsError takes the platform's own code, so on Windows the native one must be set
#if ME_WIN_LIKE
    rSetOsError(ERROR_FILE_NOT_FOUND);
#else
    rSetOsError(ENOENT);
#endif
    err = rGetOsError();
    teqi(err, ENOENT);
    err = rGetRawOsError();
#if !ME_WIN_LIKE
    teqi(err, ENOENT);
#endif

#if ME_WIN_LIKE
    rSetOsError(ERROR_ACCESS_DENIED);
#else
    rSetOsError(EPERM);
#endif
    err = rGetOsError();
    teqi(err, EPERM);

    rSetOsError(0);
    err = rGetOsError();
    teqi(err, 0);
}


static void timeouts(void)
{
    bool on;

    on = rGetTimeouts();
    ttrue(on);

    rSetTimeouts(0);
    on = rGetTimeouts();
    tfalse(on);

    rSetTimeouts(1);
    on = rGetTimeouts();
    ttrue(on);
}


/*
    rEmitLog is the gate every log call passes through. A type or source is enabled by name, by
    the "all" wildcard, or disabled by a leading "!" which must beat the wildcard.
 */
static void filters(void)
{
    bool emit;

    rSetLogFilter("error,info,!debug", "app,!noisy", 1);

    emit = rEmitLog("error", "app");
    ttrue(emit);
    emit = rEmitLog("info", "app");
    ttrue(emit);
    emit = rEmitLog("debug", "app");
    tfalse(emit);

    /*
        A type that is not named at all and has no "all" fallback is off.
     */
    emit = rEmitLog("trace", "app");
    tfalse(emit);

    /*
        A disabled source rejects an otherwise enabled type.
     */
    emit = rEmitLog("error", "noisy");
    tfalse(emit);
    emit = rEmitLog("error", "unknown");
    tfalse(emit);

    /*
        "raw" is always registered by rSetLogFilter so raw output is never filtered out.
     */
    emit = rEmitLog("raw", "app");
    ttrue(emit);

    /*
        Null type and source fall back to "info" and "app".
     */
    emit = rEmitLog(NULL, NULL);
    ttrue(emit);

    /*
        With "all" as the wildcard, an unnamed source is enabled but an excluded one is not.
     */
    rSetLogFilter("all,!debug", "all,!mbedtls", 1);
    emit = rEmitLog("error", "anything");
    ttrue(emit);
    emit = rEmitLog("anything", "app");
    ttrue(emit);
    emit = rEmitLog("debug", "app");
    tfalse(emit);
    emit = rEmitLog("error", "mbedtls");
    tfalse(emit);

    /*
        Neither list given is a no-op, not a reset: the prior filter still applies.
     */
    rSetLogFilter(NULL, NULL, 1);
    emit = rEmitLog("error", "anything");
    ttrue(emit);
    emit = rEmitLog("debug", "app");
    tfalse(emit);
}


/*
    Every log entry point routes through the installed handler.
 */
static void handlers(void)
{
    RLogHandler prior, current;

    rSetLogFilter("all", "all", 1);

    prior = rSetLogHandler(captureHandler);
    tnotnull(prior);
    current = rGetLogHandler();
    ttrue(current == captureHandler);

    resetCapture();
    rLog("info", "test", "hello %s", "world");
    teqi(captureCount, 1);
    tmatch(lastType, "info");
    tmatch(lastSource, "test");
    tmatch(capturedText(), "hello world");

    resetCapture();
    rError("test", "failed %d", 42);
    teqi(captureCount, 1);
    tmatch(lastType, "error");
    tmatch(capturedText(), "failed 42");

    resetCapture();
    rInfo("test", "informational");
    teqi(captureCount, 1);
    tmatch(lastType, "info");

    resetCapture();
    rTrace("test", "tracing");
    teqi(captureCount, 1);
    tmatch(lastType, "trace");

    /*
        rDebug compiles away unless ME_R_DEBUG_LOGGING is set. The tests are built without
        ME_DEBUG so they see r.h as an embedding application does, and there the macro is empty.
        The "debug" type still has to reach the handler when it is logged directly.
     */
#if ME_R_DEBUG_LOGGING
    resetCapture();
    rDebug("test", "debugging");
    teqi(captureCount, 1);
    tmatch(lastType, "debug");
#endif

    resetCapture();
    rLog("debug", "test", "debugging");
    teqi(captureCount, 1);
    tmatch(lastType, "debug");

    /*
        A filtered-out message must not reach the handler at all.
     */
    rSetLogFilter("error", "all", 1);
    resetCapture();
    rLog("info", "test", "suppressed");
    teqi(captureCount, 0);
    rLog("error", "test", "delivered");
    teqi(captureCount, 1);

    rSetLogHandler(prior);
    current = rGetLogHandler();
    ttrue(current == prior);
    rSetLogHandler(captureHandler);
}


static void logvWrapper(cchar *type, cchar *source, cchar *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    rLogv(type, source, fmt, args);
    va_end(args);
}


static void logv(void)
{
    rSetLogFilter("all", "all", 1);

    resetCapture();
    logvWrapper("info", "test", "value=%d", 7);
    teqi(captureCount, 1);
    tmatch(capturedText(), "value=7");

    /*
        rLogv is gated by the same filter as rLog.
     */
    rSetLogFilter("error", "all", 1);
    resetCapture();
    logvWrapper("info", "test", "suppressed");
    teqi(captureCount, 0);
    rSetLogFilter("all", "all", 1);
}


/*
    rFormatLog expands the format set by rSetLogFormat. Each specifier is checked on its own so
    a failure names the one that broke.
 */
static void formats(void)
{
    RBuf  *buf;
    cchar *text;
    char  host[256];

    buf = rAllocBuf(512);
    tnotnull(buf);

    rSetLogFormat("%T", 1);
    rFormatLog(buf, "info", "src", "message");
    text = rBufToString(buf);
    tmatch(text, "info");

    rSetLogFormat("%S", 1);
    rFormatLog(buf, "info", "src", "message");
    text = rBufToString(buf);
    tmatch(text, "src");

    rSetLogFormat("%A", 1);
    rFormatLog(buf, "info", "src", "message");
    text = rBufToString(buf);
    tmatch(text, rGetAppName());

    /*
        %M appends a newline only when the message does not already end in one.
     */
    rSetLogFormat("%M", 1);
    rFormatLog(buf, "info", "src", "message");
    text = rBufToString(buf);
    tmatch(text, "message\n");

    rFormatLog(buf, "info", "src", "message\n");
    text = rBufToString(buf);
    tmatch(text, "message\n");

    rFormatLog(buf, "info", "src", "");
    text = rBufToString(buf);
    tmatch(text, "");

    rSetLogFormat("%P", 1);
    rFormatLog(buf, "info", "src", "message");
    text = rBufToString(buf);
    ttrue(snumber(text));

    rSetLogFormat("%C", 1);
    rFormatLog(buf, "info", "src", "message");
    text = rBufToString(buf);
    ttrue(snumber(text));

    rSetLogFormat("%D", 1);
    rFormatLog(buf, "info", "src", "message");
    text = rBufToString(buf);
    ttrue(slen(text) > 0);

    rSetLogFormat("%H", 1);
    rFormatLog(buf, "info", "src", "message");
    text = rBufToString(buf);
    if (gethostname(host, sizeof(host)) == 0) {
        tmatch(text, host);
    }

    /*
        An unknown specifier is emitted literally, and a trailing "%" is dropped rather than
        read past the end of the format.
     */
    rSetLogFormat("%Z", 1);
    rFormatLog(buf, "info", "src", "message");
    text = rBufToString(buf);
    tmatch(text, "Z");

    rSetLogFormat("end%", 1);
    rFormatLog(buf, "info", "src", "message");
    text = rBufToString(buf);
    tmatch(text, "end");

    rSetLogFormat("[%T] %S: %M", 1);
    rFormatLog(buf, "error", "mod", "boom");
    text = rBufToString(buf);
    tmatch(text, "[error] mod: boom\n");

    /*
        The "raw" type bypasses the format entirely.
     */
    rFormatLog(buf, "raw", "mod", "verbatim");
    text = rBufToString(buf);
    tmatch(text, "verbatim");

    rFreeBuf(buf);
    rSetLogFormat(R_LOG_FORMAT, 1);
}


/*
    Writing to a real file, through the default handler, and reading it back.
 */
static void logToFile(void)
{
    RLogHandler prior;
    char        *contents;
    bool        isSet;
    int         rc, fd;

    sfmtbuf(logPath, sizeof(logPath), "data/temp-log-%d.log", (int) getpid());
    sfmtbuf(backupPath, sizeof(backupPath), "data/temp-log-%d-0.log", (int) getpid());
    unlink(logPath);
    unlink(backupPath);

    rc = rSetLogPath(logPath, 1);
    teqi(rc, 0);
    isSet = rIsLogSet();
    ttrue(isSet);
    fd = rGetLogFile();
    ttrue(fd > 2);

    rSetLogFormat("%T %S %M", 1);
    rSetLogFilter("all", "all", 1);
    prior = rSetLogHandler(rDefaultLogHandler);

    rLog("info", "mod", "written to file");
    rLog("raw", "mod", "raw line\n");

    rSetLogHandler(prior);

    contents = rReadFile(logPath, NULL);
    tnotnull(contents);
    ttrue(scontains(contents, "info mod written to file") != 0);
    ttrue(scontains(contents, "raw line") != 0);
    rFree(contents);

    /*
        A path of "none" is ignored rather than treated as a filename, and "stdout"/"stderr"
        select the standard descriptors rather than creating files of that name.
     */
    rc = rSetLog("none", NULL, 1);
    teqi(rc, 0);
    fd = rGetLogFile();
    ttrue(fd > 2);

    rc = rSetLogPath("stderr", 1);
    teqi(rc, 0);
    fd = rGetLogFile();
    teqi(fd, 2);

    rc = rSetLogPath("stdout", 1);
    teqi(rc, 0);
    fd = rGetLogFile();
    teqi(fd, 1);

    /*
        A null path clears the destination, so rIsLogSet reports false.
     */
    rc = rSetLogPath(NULL, 1);
    teqi(rc, 0);
    isSet = rIsLogSet();
    tfalse(isSet);

    /*
        A path that cannot be opened is reported rather than silently accepted.
     */
    rc = rSetLogPath("data/no-such-directory/x.log", 1);
    teqi(rc, R_ERR_CANT_OPEN);

    rSetLogPath("stdout", 1);
    rSetLogHandler(captureHandler);
}


/*
    rSetLog parses "path:types:sources" in one string.
 */
static void combinedSpec(void)
{
    bool emit;
    int  rc, fd;

    rc = rSetLog("stdout:error,!info:app,!other", "%M", 1);
    teqi(rc, 0);
    fd = rGetLogFile();
    teqi(fd, 1);

    emit = rEmitLog("error", "app");
    ttrue(emit);
    emit = rEmitLog("info", "app");
    tfalse(emit);
    emit = rEmitLog("error", "other");
    tfalse(emit);

    /*
        With no source field, the sources default to everything but mbedtls.
     */
    rc = rSetLog("stdout:error", "%M", 1);
    teqi(rc, 0);
    emit = rEmitLog("error", "anything");
    ttrue(emit);
    emit = rEmitLog("error", "mbedtls");
    tfalse(emit);

    /*
        A null path is ignored.
     */
    rc = rSetLog(NULL, NULL, 1);
    teqi(rc, 0);
}


/*
    Forced settings are sticky: a later unforced call must not undo a command-line override.
 */
static void stickySettings(void)
{
    bool emit;

    rSetLogFilter("error", "all", 1);
    emit = rEmitLog("error", "app");
    ttrue(emit);

    rSetLogFilter("info", "all", 0);
    emit = rEmitLog("error", "app");
    ttrue(emit);
    emit = rEmitLog("info", "app");
    tfalse(emit);

    rSetLogFormat("%A ignored", 0);
    rSetLogPath("stderr", 0);
    teqi(rGetLogFile(), 1);

    rSetLogFilter("all", "all", 1);
}


/*
    Rotation. rBackupLog is a no-op until the file reaches the threshold, then renames it aside
    and reopens an empty one.
 */
static void rotation(void)
{
    char  *block;
    bool  exists;
    ssize written;
    int   fd, i, rc;

    rc = rSetLogPath(logPath, 1);
    teqi(rc, 0);
    fd = rGetLogFile();
    ttrue(fd > 2);

    /*
        Below the threshold nothing moves.
     */
    rBackupLog();
    exists = rFileExists(backupPath);
    tfalse(exists);

    block = rAlloc(65536);
    tnotnull(block);
    memset(block, 'x', 65536);
    for (i = 0; i < (LOG_SIZE / 65536) + 1; i++) {
        written = write(fd, block, 65536);
        teqz(written, 65536);
    }
    rFree(block);

    rBackupLog();
    exists = rFileExists(backupPath);
    ttrue(exists);

    /*
        The reopened log is empty and still usable.
     */
    fd = rGetLogFile();
    ttrue(fd > 2);
    written = write(fd, "after rotation\n", 15);
    teqz(written, 15);

    rSetLogPath("stdout", 1);
    unlink(logPath);
    unlink(backupPath);
}


/*
    Size of a file, or -1 if it does not exist.
 */
static ssize fileSize(cchar *path)
{
    struct stat info;

    if (stat(path, &info) != 0) {
        return -1;
    }
    return (ssize) info.st_size;
}


static void removeLogFiles(cchar *base)
{
    char path[ME_MAX_FNAME];
    int  i;

    sfmtbuf(path, sizeof(path), "%s.log", base);
    unlink(path);
    for (i = 0; i < LOG_COUNT + 2; i++) {
        sfmtbuf(path, sizeof(path), "%s-%d.log", base, i);
        unlink(path);
    }
}



static void rotationDefault(void)
{
    RLogHandler prior;
    char        base[ME_MAX_FNAME], path[ME_MAX_FNAME], backup[ME_MAX_FNAME];
    ssize       size;
    int         i;

    sfmtbuf(base, sizeof(base), "data/temp-flood-%d", (int) getpid());
    removeLogFiles(base);
    sfmtbuf(path, sizeof(path), "%s.log", base);
    sfmtbuf(backup, sizeof(backup), "%s-0.log", base);

    teqi(rSetLogPath(path, 1), 0);
    rSetLogFormat("%M", 1);
    rSetLogFilter("all", "all", 1);
    prior = rSetLogHandler(rDefaultLogHandler);

    //  About 2.5MB of 200 byte messages, which is more than the limit
    for (i = 0; i < 12500; i++) {
        rLog("info", "flood", "%0199d", i);
    }
    rSetLogHandler(prior);

    ttrue(rFileExists(backup));
    size = fileSize(path);
    ttrue(size >= 0 && size < LOG_SIZE);
    size = fileSize(backup);
    ttrue(size >= LOG_SIZE && size < LOG_SIZE + 256);

    rSetLogPath("stdout", 1);
    removeLogFiles(base);
}



static void rotationLimits(void)
{
    RLogHandler prior;
    char        base[ME_MAX_FNAME], path[ME_MAX_FNAME], backup[LOG_COUNT + 2][ME_MAX_FNAME];
    ssize       size, total;
    char        *last, *text;
    int         i;

    sfmtbuf(base, sizeof(base), "data/temp-limits-%d", (int) getpid());
    removeLogFiles(base);
    sfmtbuf(path, sizeof(path), "%s.log", base);
    for (i = 0; i < LOG_COUNT + 2; i++) {
        sfmtbuf(backup[i], sizeof(backup[i]), "%s-%d.log", base, i);
    }
    rSetLogFormat("%M", 1);
    rSetLogFilter("all", "all", 1);
    prior = rSetLogHandler(rDefaultLogHandler);

    //  A small limit and two backups: 1000 messages of 200 bytes make about 49 rotations
    rSetLogRotation(4096, 2);
    teqi(rSetLogPath(path, 1), 0);
    for (i = 0; i < 1000; i++) {
        rLog("info", "flood", "%0199d", i);
    }
    ttrue(rFileExists(backup[0]));
    ttrue(rFileExists(backup[1]));
    tfalse(rFileExists(backup[2]));
    total = 0;
    for (i = 0; i < 2; i++) {
        size = fileSize(backup[i]);
        ttrue(size >= 4096 && size < 4096 + 256);
        total += size;
    }
    size = fileSize(path);
    ttrue(size >= 0 && size < 4096);
    total += size;
    ttrue(total < 3 * (4096 + 256));


    text = rReadFile(path, NULL);
    tnotnull(text);
    last = rReadFile(backup[0], NULL);
    tnotnull(last);
    ttrue(scontains(text, "0999" NL) != 0 || scontains(last, "0999" NL) != 0);
    tfalse(scontains(text, "0000" NL) != 0 || scontains(last, "0000" NL) != 0);
    rFree(text);
    rFree(last);

    //  A count below one is one backup
    rSetLogPath("stdout", 1);
    removeLogFiles(base);
    rSetLogRotation(4096, 0);
    teqi(rSetLogPath(path, 1), 0);
    for (i = 0; i < 100; i++) {
        rLog("info", "flood", "%0199d", i);
    }
    ttrue(rFileExists(backup[0]));
    tfalse(rFileExists(backup[1]));

    //  A zero size disables rotation
    rSetLogPath("stdout", 1);
    removeLogFiles(base);
    rSetLogRotation(0, 2);
    teqi(rSetLogPath(path, 1), 0);
    for (i = 0; i < 100; i++) {
        rLog("info", "flood", "%0199d", i);
    }
    tfalse(rFileExists(backup[0]));
    size = fileSize(path);
    ttrue(size >= 100 * 200);


    rSetLogPath("stdout", 1);
    rSetLogRotation(4096, 2);
    teqi(rSetLogPath(path, 1), 0);
    rLog("info", "flood", "after reopen");
    ttrue(rFileExists(backup[0]));
    size = fileSize(path);
    ttrue(size >= 0 && size < 4096);

    rSetLogHandler(prior);
    rSetLogRotation(LOG_SIZE, LOG_COUNT);
    rSetLogPath("stdout", 1);
    removeLogFiles(base);
}


/*
    Check that request-controlled log bytes cannot create records or terminal escapes.
 */
static void controlBytes(void)
{
    RBuf  *buf;
    cchar *text;

    buf = rAllocBuf(512);
    tnotnull(buf);
    rSetLogFormat("%M", 1);

    rFormatLog(buf, "error", "web", "A\x1b[2J\x07\x0b" "B\x7f" "C");
    text = rBufToString(buf);
    tmatch(text, "A\\x1b[2J\\x07\\x0bB\\x7fC\n");


    rFormatLog(buf, "error", "web", "\x01" "a\x1f" "b");
    text = rBufToString(buf);
    tmatch(text, "\\x01a\\x1fb\n");

    //  A carriage return alone could overwrite the line on a terminal
    rFormatLog(buf, "error", "web", "real\rforged");
    text = rBufToString(buf);
    tmatch(text, "real\\x0dforged\n");

    //  Ordinary text is unchanged, including line structure, tabs and UTF-8
    rFormatLog(buf, "info", "web", "one\ttwo\nthree\r\nfour \xc3\xa9 \\x41 [1m");
    text = rBufToString(buf);
    tmatch(text, "one\ttwo\nthree\r\nfour \xc3\xa9 \\x41 [1m\n");


    rFormatLog(buf, "error", "web", "\x1b");
    text = rBufToString(buf);
    tmatch(text, "\\x1b\n");

    rFormatLog(buf, "error", "web", "\x1b" "x\x1b");
    text = rBufToString(buf);
    tmatch(text, "\\x1bx\\x1b\n");

    //  The raw type bypasses the format but not the escaping
    rFormatLog(buf, "raw", "web", "GET / HTTP/1.1\r\nHost: a\x1b]0;x\x07\r\n");
    text = rBufToString(buf);
    tmatch(text, "GET / HTTP/1.1\r\nHost: a\\x1b]0;x\\x07\r\n");

    rFreeBuf(buf);
    rSetLogFormat(R_LOG_FORMAT, 1);
}



static void controlBytesInFile(void)
{
    RLogHandler prior;
    char        *contents;
    size_t      length;

    sfmtbuf(logPath, sizeof(logPath), "data/temp-ctl-%d.log", (int) getpid());
    unlink(logPath);
    teqi(rSetLogPath(logPath, 1), 0);
    rSetLogFormat("%M", 1);
    rSetLogFilter("all", "all", 1);
    prior = rSetLogHandler(rDefaultLogHandler);

    rLog("error", "web", "Origin %s rejected", "\x1b]0;owned\x07");
    rSetLogHandler(prior);
    rSetLogPath("stdout", 1);

    contents = rReadFile(logPath, &length);
    tnotnull(contents);
    ttrue(memchr(contents, 0x1b, length) == NULL);
    ttrue(memchr(contents, 0x07, length) == NULL);
    tcontains(contents, "Origin \\x1b]0;owned\\x07 rejected" NL);
    rFree(contents);
    unlink(logPath);
}



static void logAllRequest(void)
{
    bool emit;

    rSetLogFilter("error", "app", 1);
    emit = rEmitLog("debug", "other");
    tfalse(emit);

    rRequestLogAll();
    emit = rEmitLog("debug", "other");
    ttrue(emit);

    //  The request is consumed, so a later filter change is not undone
    rSetLogFilter("error", "app", 1);
    emit = rEmitLog("debug", "other");
    tfalse(emit);
    rSetLogFilter("all", "all", 1);
}


/*
    rMetrics writes an embedded-metric-format record straight to the log descriptor. Only string
    values are used: the function walks its varargs twice and the first pass reads every value as
    a pointer, so a non-pointer value would be read through the wrong type.
 */
static void metrics(void)
{
    char *contents;
    int  rc;

    rc = rSetLogPath(logPath, 1);
    teqi(rc, 0);

    rMetrics("MetricMessage", "TestNamespace", "Device=test", "values",
             "Requests", "string", "17", NULL, NULL, NULL);

    rSetLogPath("stdout", 1);

    contents = rReadFile(logPath, NULL);
    tnotnull(contents);
    ttrue(scontains(contents, "MetricMessage") != 0);
    ttrue(scontains(contents, "TestNamespace") != 0);
    ttrue(scontains(contents, "Device=test") != 0);
    ttrue(scontains(contents, "Requests") != 0);
    rFree(contents);
    unlink(logPath);
}


/*
    The remaining entry points. rLogConfig and rAssert both emit through the handler, so the
    capture handler keeps the run quiet and lets the output be asserted.
 */
static void diagnostics(void)
{
    rSetLogFilter("all", "all", 1);
    rSetLogHandler(captureHandler);

    resetCapture();
    rLogConfig();
    ttrue(captureCount > 0);
    tmatch(lastSource, "app");

    resetCapture();
    rAssert("test/log.tst.c:1", "condition failed");
    ttrue(captureCount > 0);
    tmatch(lastType, "error");
    tmatch(lastSource, "assert");
    ttrue(scontains(capturedText(), "condition failed") != 0);

    resetCapture();
    rAssert(NULL, "no location");
    ttrue(captureCount > 0);
    ttrue(scontains(capturedText(), "no location") != 0);

    /*
        rBreakpoint is a no-op unless DEBUG_PAUSE is configured. Calling it must be safe.
     */
    rBreakpoint();
}


/*
    rMetrics formats each value according to the type name beside it. Only the string form was
    exercised, so the int, int64 and boolean branches - each of which pulls a different type off
    the varargs - were never run and a wrong va_arg there would not have been noticed.
 */
static void metricTypes(void)
{
    char *contents;

    teqi(rSetLogPath(logPath, 1), 0);

    rMetrics("Typed", "TestNamespace", "Device=test", "values",
             "Count", "int", 42,
             "Bytes", "int64", (int64) 9223372036854775807LL,
             "Enabled", "boolean", "true",
             "Label", "string", "text",
             NULL, NULL, NULL);

    rSetLogPath("stdout", 1);

    contents = rReadFile(logPath, NULL);
    tnotnull(contents);
    tcontains(contents, "\"Count\": 42");
    tcontains(contents, "\"Bytes\": 9223372036854775807");
    tcontains(contents, "\"Enabled\": true");
    tcontains(contents, "\"Label\": \"text\"");
    rFree(contents);
    unlink(logPath);

    //  A metric with no dimensions omits the Dimensions clause rather than emitting an empty one
    teqi(rSetLogPath(logPath, 1), 0);
    rMetrics("Bare", "TestNamespace", NULL, "values", "One", "int", 1, NULL, NULL, NULL);
    rSetLogPath("stdout", 1);

    contents = rReadFile(logPath, NULL);
    tnotnull(contents);
    tcontains(contents, "Bare");
    rFree(contents);
    unlink(logPath);
}


/*
    print and dump are the bare diagnostic helpers. Neither returns anything and both write
    straight to the output, so the assertion is that they run and do not corrupt what follows.
 */
static void diagnosticHelpers(void)
{
    uchar bytes[] = { 0x00, 0x7F, 0x80, 0xFF };

    print("print: %s %d", "formatted", 7);
    print("%s", "");
    dump("dump:", bytes, sizeof(bytes));
    dump("dump-empty:", bytes, 0);

    //  Still usable afterwards
    rSetLogFilter("all", "all", 1);
    ttrue(rEmitLog("error", "app"));
}


int main(void)
{
    rInit(0, 0);

    captured = rAllocBuf(1024);
    tnotnull(captured);

    errorCodes();
    osErrors();
    timeouts();
    filters();
    handlers();
    logv();
    formats();
    logToFile();
    combinedSpec();
    stickySettings();
    rotation();
    rotationDefault();
    rotationLimits();
    controlBytes();
    controlBytesInFile();
    logAllRequest();
    metrics();
    metricTypes();
    diagnosticHelpers();
    diagnostics();

    rSetLogHandler(rDefaultLogHandler);
    rSetLogPath("stdout", 1);
    rFreeBuf(captured);

    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
