/*
    file.tst.c - Unit tests for the file module

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/************************************ Code ************************************/

static void checkList(RList *list, cchar *expect)
{
    char *actual;

    actual = rListToString(list, ",");
    tmatch(actual, expect);
    rFree(actual);
}


static void globFile()
{
    RList *list;
    char  home[ME_MAX_PATH], *pattern, *path;
    cchar *dir;
    bool  ok;
    int   next;

    dir = "data";
    getcwd(home, sizeof(home));

    //  Null args
    list = rGetFiles(NULL, NULL, 0);
    checkList(list, "");
    rFreeList(list);

    //  Empty args
    list = rGetFiles(NULL, "", 0);
    checkList(list, "");
    rFreeList(list);

    list = rGetFiles("", "", 0);
    checkList(list, "");
    rFreeList(list);

    //  Empty should match nothing
    list = rGetFiles(".", "", 0);
    checkList(list, "");
    rFreeList(list);

    //  Single wildcard
    list = rGetFiles(".", "*", 0);
    tneqz(rGetListLength(list), 0);
    rFreeList(list);

    //  Test known directory contents
    list = rGetFiles(dir, "*", 0);
    teqz(rGetListLength(list), 6);
    rFreeList(list);

    //  Test with path in pattern
    list = rGetFiles(".", "data/*", 0);
    teqz(rGetListLength(list), 6);
    rFreeList(list);

    //  Pass absolute path -- absolute results
    list = rGetFiles(home, "**", 0);
    tneqz(rGetListLength(list), 0);
    ttrue(rIsFileAbs(rGetItem(list, 0)));
    ok = 1;
    for (ITERATE_ITEMS(list, path, next)) {
        if (!rIsFileAbs(path)) {
            ok = 0;
        }
    }
    ttrue(ok, "All paths should be absolute");
    rFreeList(list);

    //  Relative results with an absolute path
    list = rGetFiles(home, "**", R_WALK_RELATIVE);
    ok = 1;
    for (ITERATE_ITEMS(list, path, next)) {
        if (rIsFileAbs(path)) {
            ok = 0;
        }
    }
    ttrue(ok, "All paths should be relative");
    rFreeList(list);

#if ME_UNIX_LIKE
    //  Relative of root
    list = rGetFiles("/", "*", R_WALK_RELATIVE);
    tneqz(rGetListLength(list), 0);
    rFreeList(list);
#endif

    //  Relative with a directory
    list = rGetFiles(dir, "mid/*.dat", R_WALK_RELATIVE);
    teqz(rGetListLength(list), 1);
    tcontains(rGetItem(list, 0), "middle.dat");
    rFreeList(list);

    //  With hidden files (one more file -- 7)
    list = rGetFiles(dir, "*", R_WALK_HIDDEN);
    teqz(rGetListLength(list), 7);
    rFreeList(list);

    //  Double wild - recursive
    list = rGetFiles(dir, "**", 0);
    teqz(rGetListLength(list), 13);
    rFreeList(list);

    //  Double wild suffix
    list = rGetFiles(dir, "m**", 0);
    teqz(rGetListLength(list), 8);
    rFreeList(list);

    //  Double wild prefix
    list = rGetFiles(dir, "**/*.dat", 0);
    teqz(rGetListLength(list), 10);
    rFreeList(list);

    //  Embedded double wild
    list = rGetFiles(dir, "mid/**/leaf*", 0);
    teqz(rGetListLength(list), 4);
    rFreeList(list);

    //  Embedded double wild
    list = rGetFiles(dir, "mid/**/*.dat", 0);
    teqz(rGetListLength(list), 5);
    rFreeList(list);

    //  Trailing double wild
    list = rGetFiles(dir, "mid/sub*/**", 0);
    teqz(rGetListLength(list), 6);
    rFreeList(list);

    //  No directories
    list = rGetFiles(dir, "mid", R_WALK_FILES);
    teqz(rGetListLength(list), 0);
    rFreeList(list);
    list = rGetFiles(dir, "mid/**", R_WALK_FILES);
    teqz(rGetListLength(list), 5);
    rFreeList(list);

    //  Pattern with absolute path
    list = rGetFiles(home, home, 0);
    teqz(rGetListLength(list), 1);
    rFreeList(list);

    list = rGetFiles(".", home, 0);
    teqz(rGetListLength(list), 1);
    rFreeList(list);

    //  Pattern with absolute path
    pattern = rJoinFile(home, "data");
    list = rGetFiles(home, pattern, 0);
    teqz(rGetListLength(list), 1);
    rFree(pattern);
    rFreeList(list);

    //  Relative files
    list = rGetFiles(dir, "**", R_WALK_RELATIVE);
    teqz(rGetListLength(list), 13);
    tfalse(rIsFileAbs(rGetItem(list, 0)));
    rFreeList(list);

    //  Absolute pattern with relative results using absolute directory
    pattern = rJoinFile(home, "data/**");
    list = rGetFiles(home, pattern, R_WALK_RELATIVE);
    rFree(pattern);
    teqz(rGetListLength(list), 13);
    ok = 1;
    for (ITERATE_ITEMS(list, path, next)) {
        if (rIsFileAbs(path)) {
            ok = 0;
        }
    }
    ttrue(ok, "All paths should be relative");
    rFreeList(list);

    //  Absolute pattern with "."
    pattern = rJoinFile(home, "data/**");
    list = rGetFiles(".", pattern, 0);
    rFree(pattern);
    ttrue(rIsFileAbs(rGetItem(list, 0)));
    teqz(rGetListLength(list), 13);
    rFreeList(list);

    //  Absolute pattern outside home
    list = rGetFiles(home, "/tmp/nothing", 0);
    teqz(rGetListLength(list), 0);
    rFreeList(list);

    // Stress and regression
    pattern = rJoinFile(home, "data/a.dat");
    list = rGetFiles(home, pattern, 0);
    rFree(pattern);
    ttrue(rIsFileAbs(rGetItem(list, 0)));
    teqz(rGetListLength(list), 1);
    rFreeList(list);

    pattern = rJoinFile(home, "data/**/leaf1.dat");
    list = rGetFiles(home, pattern, 0);
    rFree(pattern);
    ttrue(rIsFileAbs(rGetItem(list, 0)));
    teqz(rGetListLength(list), 1);
    rFreeList(list);

    list = rGetFiles(dir, "**/*.dat", 0);
    teqz(rGetListLength(list), 10);
    rFreeList(list);

    list = rGetFiles(dir, "mid/middle.dat", R_WALK_RELATIVE);
    tcontains(rGetItem(list, 0), "middle.dat");
    rFreeList(list);
}


static void globDirs()
{
    RList *list;

    list = rGetFiles(".", "data/**", R_WALK_DIRS);
    teqz(rGetListLength(list), 3);
    rFreeList(list);

    list = rGetFiles(".", "data/**", R_WALK_FILES);
    teqz(rGetListLength(list), 10);
    rFreeList(list);
}


/*
    Abort a nested walk in both traversal orders to check cleanup of each recursion frame.
 */
static int abortOnLeaf(void *arg, cchar *path, int flags)
{
    int *count;

    count = (int*) arg;
    (*count)++;
    if (sends(path, "leaf1.dat")) {
        return -1;
    }
    return 0;
}


static void walkAbortTest()
{
    int count, rc, i;

    for (i = 0; i < 100; i++) {
        count = 0;
        rc = rWalkDir("data", "**", abortOnLeaf, &count, R_WALK_FILES | R_WALK_DIRS);
        ttrue(rc < 0);
        ttrue(count > 0);

        count = 0;
        rc = rWalkDir("data", "**", abortOnLeaf, &count, R_WALK_FILES | R_WALK_DIRS | R_WALK_DEPTH_FIRST);
        ttrue(rc < 0);
        ttrue(count > 0);
    }
}


static void joinFile()
{
    char *joined;

#if ME_WIN_LIKE
    joined = rJoinFile("\\tmp", "Makefile");
    tmatch(joined, "\\tmp\\Makefile");
    rFree(joined);

    joined = rJoinFile("\\tmp", "\\Makefile");
    tmatch(joined, "\\Makefile");
    rFree(joined);

    joined = rJoinFile("\\tmp", NULL);
    tmatch(joined, "\\tmp");
    rFree(joined);

    joined = rJoinFile("\\tmp", ".");
    tmatch(joined, "\\tmp");
    rFree(joined);
#else
    joined = rJoinFile("/tmp", "Makefile");
    tmatch(joined, "/tmp/Makefile");
    rFree(joined);

    joined = rJoinFile("/tmp", "/Makefile");
    tmatch(joined, "/Makefile");
    rFree(joined);

    joined = rJoinFile("/tmp", NULL);
    tmatch(joined, "/tmp");
    rFree(joined);

    joined = rJoinFile("/tmp", ".");
    tmatch(joined, "/tmp");
    rFree(joined);
#endif
    joined = rJoinFile("", "Makefile");
    tmatch(joined, "Makefile");
    rFree(joined);
}


static void matchFile()
{
    //  Null args
    tfalse(rMatchFile("abc", NULL));
    tfalse(rMatchFile(NULL, "abc"));

    //  Empty args
    ttrue(rMatchFile("", ""));
    tfalse(rMatchFile("abc", ""));
    tfalse(rMatchFile("", "abc"));

    //  Substrings
    ttrue(rMatchFile("abc", "abc"));
    tfalse(rMatchFile("abc", "abcd"));
    tfalse(rMatchFile("abc", "ab"));

    //  Subpaths
    ttrue(rMatchFile("/a/b", "/a/b"));
    tfalse(rMatchFile("/a/b", "/a/b/c"));
    tfalse(rMatchFile("/a/b", "/a/"));

    //  Trailing separators
    ttrue(rMatchFile("/a/b/c", "/a/b/c/"));
    ttrue(rMatchFile("/a/b/c/", "/a/b/c"));

    //  Wild
    ttrue(rMatchFile("abc", "*"));
    ttrue(rMatchFile("abc", "a*"));
    ttrue(rMatchFile("abc", "*c"));
    ttrue(rMatchFile("abc", "a*c"));
    tfalse(rMatchFile("abc", "a*d"));

    //  Single char
    ttrue(rMatchFile("abc", "???"));
    tfalse(rMatchFile("abc", "??"));
    tfalse(rMatchFile("abc", "?"));
    ttrue(rMatchFile("abc", "?*"));
    ttrue(rMatchFile("abc", "*?"));

    //  Double wild
    ttrue(rMatchFile("a/b/c", "**"));
    ttrue(rMatchFile("a/b/c", "**c"));
    ttrue(rMatchFile("a/b/c", "**/c"));
    ttrue(rMatchFile("a/b/c", "**/*c"));
    ttrue(rMatchFile("a/b/c", "a/**c"));
    ttrue(rMatchFile("a/b/c", "a/**/*c"));

    tfalse(rMatchFile("a/b/c", "a/**/d"));
    tfalse(rMatchFile("a/b/c", "b/**"));
    tfalse(rMatchFile("a/b/c", "**/x/c"));

    //  Non-canonical separators
    ttrue(rMatchFile("a////b", "a/b"));

    //  Trailing separators
    ttrue(rMatchFile("a/b/", "a/b"));
    ttrue(rMatchFile("a/b", "a/b/"));

    //  Quad wild
    ttrue(rMatchFile("a/b/c", "****c"));
    ttrue(rMatchFile("a/b/c", "**/**c"));

    //  Pattern in directory
    ttrue(rMatchFile("a.c", "**/a.c"));
    ttrue(rMatchFile("a.c/a.c", "**/a.c"));
    ttrue(rMatchFile("a.c/a.c/a.c", "**/a.c"));
    ttrue(rMatchFile("a.c/a.c/a.c", "**/a.c/a.c"));
    tfalse(rMatchFile("a.c", "**/a.c/a.c"));
    ttrue(rMatchFile("a.c/a.c", "**/a.c/a.c"));

    //  Stress
    ttrue(rMatchFile("/a/b/c/d/e/f/g", "/a**"));
    ttrue(rMatchFile("/a/b/c/d/e/f/g", "/a**/c/**/g"));
    tfalse(rMatchFile("/a/b/c/d/e/f/g", "/a**/c/**/h"));
    tfalse(rMatchFile("/a/b/c/d/e/f/g", "/a**/k/**/g"));
}


static void regressFile()
{
    RList *list;

#if ME_UNIX_LIKE
    list = rGetFiles("/", "/dev", 0);
    teqz(rGetListLength(list), 1);
    checkList(list, "/dev");
    rFreeList(list);

    list = rGetFiles("/", "/dev", R_WALK_RELATIVE);
    teqz(rGetListLength(list), 1);
    checkList(list, "dev");
    rFreeList(list);
#endif

    list = rGetFiles(".", "data/**", R_WALK_FILES);
    teqz(rGetListLength(list), 10);
    rFreeList(list);

    list = rGetFilesEx(0, ".", "**leaf*", R_WALK_FILES);
    teqz(rGetListLength(list), 4);
    rFreeList(list);
}

static void miscFile(void)
{
    cchar  *spath;
    char   *data, *path;
    size_t len;
    ssize  size;
    int    status;

    path = rGetCwd();
    tcontains(path, "test");
    rFree(path);

    path = rGetAppDir();
    tcontains(path, ".testme");
    rFree(path);

    spath = rBasename("/tmp/unknown.txt");
    tmatch(spath, "unknown.txt");

    path = rGetAppDir();
    path = rDirname(path);
    tnotnull(path);
#if ME_WIN_LIKE
    teqi(path[2], '\\');
#else
    teqi(path[0], '/');
#endif
    rFree(path);

    path = rGetTempFile("data", "TEST");
    tnotnull(path);
#if ME_UNIX_LIKE
    tcontains(path, "TEST-");
#endif
    unlink(path);

    data = rReadFile("data/file.dat", &len);
    tnotnull(data);
    teqz(len, 29);
    tmatch(data, "Tue Feb 21 11:27:27 PST 2012\n");

    // Use unique filenames to avoid conflicts during concurrent testing
    char  tempFile[256], backupFile[256];
    pid_t pid = getpid();
    snprintf(tempFile, sizeof(tempFile), "data/temp-%d.tmp", pid);
    snprintf(backupFile, sizeof(backupFile), "data/temp-%d-0.tmp", pid);

    size = rWriteFile(tempFile, data, slen(data), 0);
    unlink(tempFile);
    teqz((size_t) size, len);

    size = rCopyFile("data/file.dat", tempFile, 0);
    teqz((size_t) size, len);

    status = rBackupFile(tempFile, 0);
    teqi(status, 0);
    unlink(backupFile);
    unlink(tempFile);
}


static void tempFileOpen(void)
{
    char  *cp, *path, *path2;
    ssize rc;
    int   fd, fd2;

#if ME_UNIX_LIKE
    struct stat sbuf;
#endif

    fd = rOpenTempFile(".test", "OPEN", &path);
    ttrue(fd >= 0);
    tnotnull(path);
    ttrue(rFileExists(path));
#if ME_UNIX_LIKE
    teqi(fstat(fd, &sbuf), 0);
    teqz((sbuf.st_mode & 0777), 0600);
#endif

    rc = write(fd, "hello temp", 10);
    teqz(rc, 10);
    close(fd);
    cp = rReadFile(path, NULL);
    tnotnull(cp);
    tmatch(cp, "hello temp");
    rFree(cp);
    unlink(path);
    rFree(path);

    fd = rOpenTempFile(".test", "DUP", &path);
    fd2 = rOpenTempFile(".test", "DUP", &path2);
    ttrue(fd >= 0);
    ttrue(fd2 >= 0);
    tnotnull(path);
    tnotnull(path2);
    ttrue(!smatch(path, path2));
    close(fd);
    close(fd2);
    unlink(path);
    unlink(path2);
    rFree(path);
    rFree(path2);

    path = (char*) 1;
    fd = rOpenTempFile(".test/no-such-dir", "pre", &path);
    ttrue(fd < 0);
    tnull(path);
}


/*
    Error paths through the file primitives. Every one of these was at zero coverage: the suite
    exercised each function on a path that worked and never on one that did not, so the trace,
    the close and the error return were all untested.
 */
static void fileErrors(void)
{
    char  buf[ME_MAX_PATH], tiny[8];
    char  *cp;
    ssize rc;
    int   fd;

    //  Self-contained: the agent vendors this file into a tree where nothing has made .test
#if ME_WIN_LIKE
    _mkdir(".test");
#else
    mkdir(".test", 0755);
#endif

    //  rReadFile: cannot open, and a path that opens but will not read
    tnull(rReadFile(".test/no-such-file-at-all", NULL));
    tnull(rReadFile(".test", NULL));

    //  rWriteFile: a length at the ceiling, and a directory that does not exist
    rc = rWriteFile(".test/written.dat", "x", SIZE_MAX, 0);
    teqz(rc, R_ERR_CANT_OPEN);
    rc = rWriteFile(".test/no-such-dir/written.dat", "x", 1, 0);
    teqz(rc, R_ERR_CANT_OPEN);

    //  A zero mode defaults to 0644 rather than creating an unreadable file
    rc = rWriteFile(".test/mode.dat", "abc", 3, 0);
    teqz(rc, 3);
    cp = rReadFile(".test/mode.dat", NULL);
    tmatch(cp, "abc");
    rFree(cp);

    //  rCopyFile: an absent source, then a destination that cannot be created
    rc = rCopyFile(".test/no-such-source", ".test/copy.dat", 0644);
    teqz(rc, R_ERR_CANT_OPEN);
    rc = rCopyFile(".test/mode.dat", ".test/no-such-dir/copy.dat", 0644);
    teqz(rc, R_ERR_CANT_CREATE);
    rc = rCopyFile(".test/mode.dat", ".test/copy.dat", 0644);
    teqz(rc, 3);
    unlink(".test/copy.dat");

    //  rBackupFile: an absent path is refused; a name with no extension still rotates
    teqi(rBackupFile(".test/no-such-file-at-all", 3), R_ERR_BAD_ARGS);
    teqi(rBackupFile(NULL, 3), R_ERR_BAD_ARGS);

    rWriteFile(".test/noext", "one", 3, 0);
    teqi(rBackupFile(".test/noext", 3), 0);
    ttrue(rFileExists(".test/noext-0"));
    tfalse(rFileExists(".test/noext"));

    //  A second backup rotates -0 to -1 rather than overwriting it
    rWriteFile(".test/noext", "two", 3, 0);
    teqi(rBackupFile(".test/noext", 3), 0);
    ttrue(rFileExists(".test/noext-1"));
    cp = rReadFile(".test/noext-0", NULL);
    tmatch(cp, "two");
    rFree(cp);
    cp = rReadFile(".test/noext-1", NULL);
    tmatch(cp, "one");
    rFree(cp);
    unlink(".test/noext-0");
    unlink(".test/noext-1");

    //  rGetTempFile: a directory that does not exist has no temporary file in it
    tnull(rGetTempFile(".test/no-such-dir", "pre"));

    cp = rGetTempFile(".test", NULL);
    tnotnull(cp);
    ttrue(rFileExists(cp));
    tcontains(cp, "tmp-");
    unlink(cp);
    rFree(cp);

    //  A null directory means the platform temporary directory, not the current one
    cp = rGetTempFile(NULL, "rtest");
    tnotnull(cp);
    ttrue(rFileExists(cp));
    unlink(cp);
    rFree(cp);

    tempFileOpen();

    //  rGetFdPath: bad arguments, a closed descriptor, and a buffer too tiny to hold the name
    teqz(rGetFdPath(0, NULL, sizeof(buf)), R_ERR_BAD_ARGS);
    teqz(rGetFdPath(0, buf, 0), R_ERR_BAD_ARGS);
    teqz(rGetFdPath(-1, buf, sizeof(buf)), R_ERR_BAD_ARGS);

    fd = open(".test/mode.dat", O_RDONLY);
    ttrue(fd >= 0);
    rc = rGetFdPath(fd, buf, sizeof(buf));
    ttrue(rc > 0);
    tcontains(buf, "mode.dat");
    teqz(rGetFdPath(fd, tiny, sizeof(tiny)), R_ERR_WONT_FIT);
    close(fd);
    teqz(rGetFdPath(fd, buf, sizeof(buf)), R_ERR_CANT_FIND);

    unlink(".test/mode.dat");

    //  rWalkDir rejects an empty path or pattern before it allocates anything
    teqi(rWalkDir(NULL, "*", NULL, NULL, 0), R_ERR_BAD_ARGS);
    teqi(rWalkDir(".", NULL, NULL, NULL, 0), R_ERR_BAD_ARGS);
    teqi(rWalkDir("", "*", NULL, NULL, 0), R_ERR_BAD_ARGS);
    teqi(rWalkDir(".", "", NULL, NULL, 0), R_ERR_BAD_ARGS);

    //  rGetAppDir is a real directory whatever the platform resolves it to
    cp = rGetAppDir();
    tnotnull(cp);
    ttrue(*cp != '\0');
}

/* rAccessFile must honor mode. ESP32/VxWorks check owner bits; other platforms ask the OS. */
static void accessFile(void)
{
    char *path;
    int  fd;

    fd = rOpenTempFile(".test", "ACCESS", &path);
    ttrue(fd >= 0);
    if (fd < 0) {
        return;
    }
    close(fd);

    teqi(rAccessFile(path, F_OK), 0);
    teqi(rAccessFile(path, R_OK), 0);
    teqi(rAccessFile(path, W_OK), 0);
    teqi(rAccessFile(path, R_OK | W_OK), 0);
    ttrue(rAccessFile(".test/no-such-file", F_OK) < 0);
    ttrue(rAccessFile(".test/no-such-file", R_OK) < 0);
    ttrue(rAccessFile(".test/no-such-file", W_OK) < 0);

#if ME_UNIX_LIKE
    if (geteuid() == 0) {
        tinfo("Skipped: The super user can access any file");
    } else {
        //  Read only
        chmod(path, 0400);
        teqi(rAccessFile(path, R_OK), 0);
        ttrue(rAccessFile(path, W_OK) < 0, "A read only file must not be writable");
        ttrue(rAccessFile(path, R_OK | W_OK) < 0);
        teqi(rAccessFile(path, F_OK), 0);

        //  Write only
        chmod(path, 0200);
        teqi(rAccessFile(path, W_OK), 0);
        ttrue(rAccessFile(path, R_OK) < 0, "A write only file must not be readable");

        //  Execute
        chmod(path, 0700);
        teqi(rAccessFile(path, X_OK), 0);
        chmod(path, 0600);
        ttrue(rAccessFile(path, X_OK) < 0, "A file without execute permission must not be executable");
    }
#else
    tinfo("Skipped: Permission bits are not modelled on this platform");
#endif
    unlink(path);
    rFree(path);
}


static void largeFile(void)
{
    char  *path;
    ssize size;
    int   fd;
    bool  created;

    fd = rOpenTempFile(".test", "LARGE", &path);
    ttrue(fd >= 0);
    if (fd < 0) {
        return;
    }
    created = 0;
#if ME_UNIX_LIKE
    created = ftruncate(fd, (off_t) 3 * 1024 * 1024 * 1024) == 0;
    close(fd);
#elif ME_WIN_LIKE
    {
        HANDLE         handle;
        LARGE_INTEGER  size3GB;
        DWORD          returned;

        close(fd);
        size3GB.QuadPart = 3LL * 1024 * 1024 * 1024;
        handle = CreateFileA(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (handle != INVALID_HANDLE_VALUE) {
            //  A sparse file takes no disk space. Not every file system can make one.
            created = DeviceIoControl(handle, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &returned, NULL) &&
                      SetFilePointerEx(handle, size3GB, NULL, FILE_BEGIN) && SetEndOfFile(handle);
            CloseHandle(handle);
        }
    }
#else
    close(fd);
#endif
    if (!created) {
        tinfo("Skipped: Cannot make a sparse file of this size here");
    } else {
        size = rGetFileSize(path);
        ttrue(size == 3LL * 1024 * 1024 * 1024, "The size of a file over 2GB must not be truncated");
        ttrue(rFileExists(path), "A file over 2GB must exist");
    }
    unlink(path);
    rFree(path);
}


int main(void)
{
    rInit(0, 0);

    joinFile();
    globFile();
    globDirs();
    walkAbortTest();
    regressFile();
    matchFile();
    miscFile();
    fileErrors();
    accessFile();
    largeFile();
    rTerm();

    return 0;
}


/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
