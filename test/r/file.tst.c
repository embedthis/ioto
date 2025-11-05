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
    size_t len, size;
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
    tgtei(len, 29);
    tcontains(data, "Tue Feb 21 11:27:27 PST 2012");

    // Use unique filenames to avoid conflicts during concurrent testing
    char  tempFile[256], backupFile[256];
    pid_t pid = getpid();
    snprintf(tempFile, sizeof(tempFile), "data/temp-%d.tmp", pid);
    snprintf(backupFile, sizeof(backupFile), "data/temp-%d-0.tmp", pid);

    size = rWriteFile(tempFile, data, slen(data), 0);
    unlink(tempFile);
    teqz(size, len);

    size = rCopyFile("data/file.dat", tempFile, 0);
    teqz(size, len);

    status = rBackupFile(tempFile, 0);
    teqi(status, 0);
    unlink(backupFile);
    unlink(tempFile);
}


int main(void)
{
    rInit(0, 0);

    joinFile();
    globFile();
    globDirs();
    regressFile();
    matchFile();
    miscFile();
    rTerm();

    return 0;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
