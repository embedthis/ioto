/*
    runtime.tst.c - Unit tests for the runtime lifecycle and the filesystem helpers

    Covers r.c (state, app name, pid file) and the parts of fs.c that file.tst.c and
    fdpath.tst.c do not reach: directory walking, the "@token" path namespace, and the tiny
    accessors.

    The TestMe comparison macros expand their arguments twice, so every call with a side effect
    is bound to a local before it is asserted.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

#if ME_WIN_LIKE
    #include <crtdbg.h>
#endif

/*********************************** Locals ***********************************/

static char tempDir[ME_MAX_FNAME];     /* Unique per process - tests run in parallel */
static int  walkCount;
static char walkLast[ME_MAX_FNAME];
static int  walkDirs;
static int  walkFiles;

/************************************ Code ************************************/

static void makeFile(cchar *path, cchar *contents)
{
    ssize written;

    written = rWriteFile(path, contents, slen(contents), 0644);
    teqz(written, (ssize) slen(contents));
}


/*
    Lifecycle state. rSetState is the only writer; rGracefulStop and rStop are named wrappers
    for two of the values.
 */
static void lifecycle()
{
    cchar *name;
    int   state;

    name = rGetAppName();
    tnotnull(name);
    ttrue(slen(name) > 0);

    state = rGetState();
    teqi(state, R_INITIALIZED);

    rSetState(R_READY);
    state = rGetState();
    teqi(state, R_READY);

    rGracefulStop();
    state = rGetState();
    teqi(state, R_STOPPING);

    rStop();
    state = rGetState();
    teqi(state, R_STOPPED);

    /*
        Restore, so nothing later in the file runs against a stopped runtime.
     */
    rSetState(R_INITIALIZED);
    state = rGetState();
    teqi(state, R_INITIALIZED);
}


/*
    The default pid file is only written when running as root. Under any other user rWritePid
    must decline rather than attempt a write it cannot make. With a path set it writes for any
    user, refuses to overwrite a live process and replaces a stale one.
 */
static void pidFile()
{
    char *buf;
    int  rc;

#if ME_UNIX_LIKE
    if (getuid() != 0) {
        rc = rWritePid();
        teqi(rc, R_ERR_CANT_WRITE);
    }
    mkdir(".test", 0755);
    unlink(".test/runtime.pid");
    rSetPidPath(".test/runtime.pid");
    rc = rWritePid();
    teqi(rc, 0);
    buf = rReadFile(".test/runtime.pid", NULL);
    tnotnull(buf);
    teqi(atoi(buf), (int) getpid());
    rFree(buf);

    //  The file now names this live process, so a second write is refused
    rc = rWritePid();
    teqi(rc, R_ERR_ALREADY_EXISTS);

    //  A stale pid is replaced
    rWriteFile(".test/runtime.pid", "2147483646\n", 11, 0600);
    rc = rWritePid();
    teqi(rc, 0);

    //  A path that cannot be created
    rSetPidPath(".test/missing-dir/runtime.pid");
    rc = rWritePid();
    teqi(rc, R_ERR_CANT_OPEN);

    rSetPidPath(NULL);
    unlink(".test/runtime.pid");
    if (getuid() != 0) {
        rc = rWritePid();
        teqi(rc, R_ERR_CANT_WRITE);
    }
#else
    rc = rWritePid();
    teqi(rc, R_ERR_BAD_STATE);
#endif
}


/*
    The tiny filesystem accessors.
 */
static void fileAccessors()
{
    char  path[ME_MAX_FNAME];
    cchar *ext;
    ssize size;
    bool  exists, isAbs;
    int   rc, fd;

    sfmtbuf(path, sizeof(path), "data/temp-runtime-%d.dat", (int) getpid());
    unlink(path);

    ext = rGetFileExt("file.txt");
    tmatch(ext, "txt");
    ext = rGetFileExt("archive.tar.gz");
    tmatch(ext, "gz");
    ext = rGetFileExt("dir.d/file");
    tmatch(ext, "d/file");
    ext = rGetFileExt("noextension");
    tnull(ext);
    ext = rGetFileExt("");
    tnull(ext);

    isAbs = rIsFileAbs("/absolute");
    ttrue(isAbs);
    isAbs = rIsFileAbs("relative");
    tfalse(isAbs);

    /*
        A missing file has no size and does not exist. Both must say so rather than report zero
        for a file that is not there.
     */
    size = rGetFileSize(path);
    teqz(size, R_ERR_CANT_FIND);
    exists = rFileExists(path);
    tfalse(exists);

    makeFile(path, "0123456789");
    size = rGetFileSize(path);
    teqz(size, 10);
    exists = rFileExists(path);
    ttrue(exists);

    rc = rAccessFile(path, R_OK);
    teqi(rc, 0);
    rc = rAccessFile("data/no-such-file", R_OK);
    ttrue(rc < 0);

    /*
        rFlushFile on a real descriptor succeeds; on a closed one it reports the error.
     */
    fd = open(path, O_RDWR, 0644);
    ttrue(fd >= 0);
    rc = rFlushFile(fd);
    teqi(rc, 0);
    close(fd);
    rc = rFlushFile(fd);
    ttrue(rc != 0);

    unlink(path);
}


/*
    rJoinFileBuf is the buffer form of rJoinFile. It must refuse rather than truncate when the
    joined path does not fit.
 */
static void joinPaths()
{
    char  buf[64];
    char  tiny[8];
    char  *result;

    result = rJoinFileBuf(buf, sizeof(buf), "dir", "file");
    tnotnull(result);
    //  The join inserts the native separator
#if ME_WIN_LIKE
    tmatch(buf, "dir\\file");
#else
    tmatch(buf, "dir/file");
#endif

    /*
        A trailing separator on the left is not doubled.
     */
    result = rJoinFileBuf(buf, sizeof(buf), "dir/", "file");
    tnotnull(result);
    tmatch(buf, "dir/file");
#if ME_WIN_LIKE
    //  A trailing native separator is not doubled either
    result = rJoinFileBuf(buf, sizeof(buf), "dir\\", "file");
    tnotnull(result);
    tmatch(buf, "dir\\file");
#endif

    /*
        An absolute right-hand side replaces the left entirely.
     */
    result = rJoinFileBuf(buf, sizeof(buf), "dir", "/abs/file");
    tnotnull(result);
    tmatch(buf, "/abs/file");

    /*
        An empty or "." right-hand side leaves the path alone.
     */
    result = rJoinFileBuf(buf, sizeof(buf), "dir", "");
    tnotnull(result);
    tmatch(buf, "dir");
    result = rJoinFileBuf(buf, sizeof(buf), "dir", ".");
    tnotnull(result);
    tmatch(buf, "dir");

    result = rJoinFileBuf(buf, sizeof(buf), "", "file");
    tnotnull(result);
    tmatch(buf, "file");

    /*
        Too long to fit: NULL, not a truncated path.
     */
    result = rJoinFileBuf(tiny, sizeof(tiny), "directory", "filename");
    tnull(result);

    result = rJoinFileBuf(NULL, sizeof(buf), "dir", "file");
    tnull(result);
    result = rJoinFileBuf(buf, 0, "dir", "file");
    tnull(result);
}


/*
    The "@token" path namespace. rAddDirectory registers a token, rGetFilePath expands it, and
    an unregistered token expands to its own name rather than failing.
 */
static void directoryTokens()
{
    char *result;

    rAddDirectory("cache", "/var/cache/app");

    result = rGetFilePath("@cache");
    tmatch(result, "/var/cache/app");
    rFree(result);

    result = rGetFilePath("@cache/file.dat");
    tmatch(result, "/var/cache/app/file.dat");
    rFree(result);

    /*
        A path with no token is returned unchanged.
     */
    result = rGetFilePath("plain/path");
    tmatch(result, "plain/path");
    rFree(result);

    /*
        An unregistered token becomes a literal directory of that name.
     */
    result = rGetFilePath("@unknown/file");
    tmatch(result, "unknown/file");
    rFree(result);

    result = rGetFilePath(NULL);
    tnull(result);

    /*
        Re-registering a token replaces it.
     */
    rAddDirectory("cache", "/tmp/other");
    result = rGetFilePath("@cache/x");
    tmatch(result, "/tmp/other/x");
    rFree(result);
}


/* Reject oversized directory tokens before expansion. */
static void overlongDirectoryToken()
{
    char path[ME_MAX_FNAME * 2 + 8];
    char *result;

    //  "@" followed by a token longer than a file name, with no separator
    memset(path, 'A', sizeof(path));
    path[0] = '@';
    path[sizeof(path) - 1] = '\0';
    result = rGetFilePath(path);
    tnull(result);

    //  The same token followed by a separator and a remainder
    memset(path, 'A', sizeof(path));
    path[0] = '@';
    path[sizeof(path) - 4] = '/';
    path[sizeof(path) - 3] = 'x';
    path[sizeof(path) - 2] = '\0';
    result = rGetFilePath(path);
    tnull(result);

    //  A token of exactly ME_MAX_FNAME does not fit, since the buffer needs a terminator
    memset(path, 'B', sizeof(path));
    path[0] = '@';
    path[ME_MAX_FNAME + 1] = '\0';
    result = rGetFilePath(path);
    tnull(result);

    //  The longest token that does fit is expanded like any other
    path[ME_MAX_FNAME] = '\0';
    result = rGetFilePath(path);
    tnotnull(result);
    teqz(slen(result), ME_MAX_FNAME - 1);
    rFree(result);
}


static int walkCallback(void *arg, cchar *path, int flags)
{
    walkCount++;
    scopy(walkLast, sizeof(walkLast), path);
    if (flags & R_WALK_DIRS) {
        walkDirs++;
    }
    if (flags & R_WALK_FILES) {
        walkFiles++;
    }
    return 0;
}


static void resetWalk()
{
    walkCount = 0;
    walkDirs = 0;
    walkFiles = 0;
    walkLast[0] = '\0';
}


/*
    rWalkDir drives the pattern matcher. The fixture is built and removed here so the tree is
    known exactly and nothing is left behind for file.tst.c to count.
 */
static void walkDirectory()
{
    char sub[ME_MAX_FNAME], a[ME_MAX_FNAME], b[ME_MAX_FNAME], deep[ME_MAX_FNAME];
    int  rc;

    sfmtbuf(tempDir, sizeof(tempDir), "data/temp-walk-%d", (int) getpid());
    sfmtbuf(sub, sizeof(sub), "%s/sub", tempDir);
    sfmtbuf(a, sizeof(a), "%s/one.txt", tempDir);
    sfmtbuf(b, sizeof(b), "%s/two.dat", tempDir);
    sfmtbuf(deep, sizeof(deep), "%s/three.txt", sub);

    mkdir(tempDir, 0755);
    mkdir(sub, 0755);
    makeFile(a, "one");
    makeFile(b, "two");
    makeFile(deep, "three");

    /*
        A literal pattern matches exactly one file.
     */
    resetWalk();
    rc = rWalkDir(tempDir, "one.txt", walkCallback, NULL, R_WALK_FILES);
    teqi(rc, 1);
    teqi(walkCount, 1);

    /*
        A wildcard matches by extension at one level.
     */
    resetWalk();
    rc = rWalkDir(tempDir, "*.txt", walkCallback, NULL, R_WALK_FILES);
    teqi(rc, 1);
    teqi(walkCount, 1);

    /*
        "**" descends. Both .txt files are found, at either depth.
     */
    resetWalk();
    rc = rWalkDir(tempDir, "**.txt", walkCallback, NULL, R_WALK_FILES);
    teqi(rc, 2);
    teqi(walkCount, 2);

    /*
        Directories only, and files only, are separable. The return counts matched files, so a
        directories-only walk reports zero even though the callback ran.
     */
    resetWalk();
    rc = rWalkDir(tempDir, "*", walkCallback, NULL, R_WALK_DIRS);
    teqi(rc, 0);
    teqi(walkCount, 1);

    resetWalk();
    rc = rWalkDir(tempDir, "*", walkCallback, NULL, R_WALK_FILES);
    teqi(rc, 2);
    teqi(walkCount, 2);

    /*
        With neither flag, both are included.
     */
    resetWalk();
    rc = rWalkDir(tempDir, "*", walkCallback, NULL, 0);
    teqi(rc, 2);
    teqi(walkCount, 3);

    /*
        Relative results are reported without the walked prefix.
     */
    resetWalk();
    rc = rWalkDir(tempDir, "one.txt", walkCallback, NULL, R_WALK_FILES | R_WALK_RELATIVE);
    teqi(rc, 1);
    teqi(walkCount, 1);
    tfalse(sstarts(walkLast, tempDir) != 0);

    /*
        A pattern that matches nothing is not an error.
     */
    resetWalk();
    rc = rWalkDir(tempDir, "*.nomatch", walkCallback, NULL, R_WALK_FILES);
    teqi(rc, 0);
    teqi(walkCount, 0);

    /*
        A pattern with a directory prefix is split so the prefix joins the path.
     */
    resetWalk();
    rc = rWalkDir(tempDir, "sub/*.txt", walkCallback, NULL, R_WALK_FILES);
    teqi(rc, 1);
    teqi(walkCount, 1);

    /*
        Depth-first ordering visits the child before its parent.
     */
    resetWalk();
    rc = rWalkDir(tempDir, "**", walkCallback, NULL, R_WALK_DEPTH_FIRST);
    ttrue(rc >= 3);
    ttrue(walkCount >= 4);

    /*
        Bad arguments are rejected rather than walked.
     */
    rc = rWalkDir(NULL, "*", walkCallback, NULL, 0);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = rWalkDir("", "*", walkCallback, NULL, 0);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = rWalkDir(tempDir, NULL, walkCallback, NULL, 0);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = rWalkDir(tempDir, "", walkCallback, NULL, 0);
    teqi(rc, R_ERR_BAD_ARGS);

    /*
        A missing directory is an error unless R_WALK_MISSING says otherwise.
     */
    resetWalk();
    rc = rWalkDir("data/no-such-directory", "*", walkCallback, NULL, R_WALK_MISSING);
    teqi(walkCount, 0);

    unlink(deep);
    unlink(a);
    unlink(b);
    rmdir(sub);
    rmdir(tempDir);
}


static int abortCallback(void *arg, cchar *path, int flags)
{
    walkCount++;
    return R_ERR_ABORTED;
}


/*
    Count this process's open descriptors. Used to prove a walk releases its directory handle.
 */
static bool fdIsOpen(int fd)
{
#if ME_WIN_LIKE
    bool open;
    int  mode;

    /*
        Probing a closed descriptor is the whole point of this helper, and the debug CRT asserts
        on it. Silence the report around just this call: it is expected, not a fault.
     */
    mode = _CrtSetReportMode(_CRT_ASSERT, 0);
    open = (HANDLE) _get_osfhandle(fd) != INVALID_HANDLE_VALUE;
    _CrtSetReportMode(_CRT_ASSERT, mode);
    return open;
#else
    return fcntl(fd, F_GETFD) >= 0;
#endif
}

static int countOpenFds(void)
{
    int fd, count;

    count = 0;
    for (fd = 0; fd < 256; fd++) {
        if (fdIsOpen(fd)) {
            count++;
        }
    }
    return count;
}


/*
    A callback that returns negative aborts the walk. The walk must report that error and must
    release everything it holds - above all the open directory handle.

    It did not. The three abort paths in dirWalk returned directly, past closeDirList and the
    two frees, leaking a descriptor per level of recursion. rGetFilesEx's own callback returns
    R_ERR_MEMORY when rAddItem fails, so this was reachable from shipped code; on a device with
    a tiny descriptor limit, repeated aborts exhaust it. Found by sec-scan's clang-analyzer
    pass (unix.Malloc on 'thisPat'), which saw the memory leak but not the descriptor.
 */
static void walkAbortReleasesHandles()
{
    char dir[ME_MAX_FNAME], sub[ME_MAX_FNAME], path[ME_MAX_FNAME];
    int  before, after, i, rc;

    sfmtbuf(dir, sizeof(dir), "data/temp-abort-%d", (int) getpid());
    sfmtbuf(sub, sizeof(sub), "%s/sub", dir);
    mkdir(dir, 0755);
    mkdir(sub, 0755);
    for (i = 0; i < 3; i++) {
        sfmtbuf(path, sizeof(path), "%s/f%d.txt", dir, i);
        makeFile(path, "x");
        sfmtbuf(path, sizeof(path), "%s/f%d.txt", sub, i);
        makeFile(path, "x");
    }

    /*
        The abort is reported to the caller rather than swallowed.
     */
    walkCount = 0;
    rc = rWalkDir(dir, "**.txt", abortCallback, NULL, R_WALK_FILES);
    teqi(rc, R_ERR_ABORTED);
    teqi(walkCount, 1);

    /*
        Repeat enough times that a leaked handle per walk would be unmistakable.
     */
    before = countOpenFds();
    for (i = 0; i < 40; i++) {
        walkCount = 0;
        rc = rWalkDir(dir, "**.txt", abortCallback, NULL, R_WALK_FILES);
        teqi(rc, R_ERR_ABORTED);
    }
    after = countOpenFds();
    teqi(after, before);

    for (i = 0; i < 3; i++) {
        sfmtbuf(path, sizeof(path), "%s/f%d.txt", sub, i);
        unlink(path);
        sfmtbuf(path, sizeof(path), "%s/f%d.txt", dir, i);
        unlink(path);
    }
    rmdir(sub);
    rmdir(dir);
}


/*
    rMatchFile is the pattern matcher on its own, without touching the filesystem.
 */
static void matchPatterns()
{
    bool match;

    match = rMatchFile("file.txt", "*.txt");
    ttrue(match);
    match = rMatchFile("file.dat", "*.txt");
    tfalse(match);
    match = rMatchFile("file.txt", "file.txt");
    ttrue(match);
    match = rMatchFile("dir/file.txt", "dir/*.txt");
    ttrue(match);
    match = rMatchFile("a/b/c.txt", "**.txt");
    ttrue(match);
    match = rMatchFile("file.txt", "file.?xt");
    ttrue(match);

    /*
        "*" matches an empty path as well as a non-empty one.
     */
    match = rMatchFile("", "*");
    ttrue(match);
    match = rMatchFile("", "file.txt");
    tfalse(match);

    /*
        "**" matches zero or more directories.
     */
    ttrue(rMatchFile("a/b", "a/**/b"));
    ttrue(rMatchFile("a/x/y/b", "a/**/b"));
    tfalse(rMatchFile("a/x/y/c", "a/**/b"));
    ttrue(rMatchFile("a/b/c", "a/**"));
    ttrue(rMatchFile("x.c", "**/*.c"));
    ttrue(rMatchFile("deep/er/path/file.js", "deep/**/*.js"));
    tfalse(rMatchFile("deep/file.jsx", "deep/**/*.js"));
    ttrue(rMatchFile("x/.git/y", "**/.git/**"));
    tfalse(rMatchFile("a/b/c/d", "**/x/**"));
    ttrue(rMatchFile("abc", "a*b*c"));
    tfalse(rMatchFile("ab", "a*c"));
}

/*
    Neither many "*" in one segment nor many "**" segments may make matching exponential.
 */
static void matchPatternCost()
{
    char   path[256], pattern[256];
    Ticks  start;
    int    i;

    start = rGetTicks();
    tfalse(rMatchFile("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "*a*a*a*a*a*a*a*a*a*a*a*a*b"));

    path[0] = pattern[0] = '\0';
    for (i = 0; i < 40; i++) {
        scopy(&path[slen(path)], sizeof(path) - slen(path), "a/");
    }
    scopy(&path[slen(path)], sizeof(path) - slen(path), "b");
    for (i = 0; i < 12; i++) {
        scopy(&pattern[slen(pattern)], sizeof(pattern) - slen(pattern), "**/*/");
    }
    scopy(&pattern[slen(pattern)], sizeof(pattern) - slen(pattern), "c");
    tfalse(rMatchFile(path, pattern));
    pattern[slen(pattern) - 1] = 'b';
    ttrue(rMatchFile(path, pattern));
    ttrue(rGetTicks() - start < 2 * TPS);
}


int main(void)
{
    rInit(0, 0);

    lifecycle();
    pidFile();
    fileAccessors();
    joinPaths();
    directoryTokens();
    overlongDirectoryToken();
    walkDirectory();
    walkAbortReleasesHandles();
    matchPatterns();
    matchPatternCost();

    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
