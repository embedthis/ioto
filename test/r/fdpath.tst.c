/*
    fdpath.tst.c - Tests for rGetFdPath and rGetRealPath

    rGetFdPath returns the name the file system actually resolved a descriptor to, which is not
    necessarily the name the caller passed to open(). Callers use it to reject a request that
    reached a file by a spelling other than its own - the basis of path containment and of
    case-variant authorization checks on case-folding file systems.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "testme.h"
#include "r.h"

/*********************************** Locals ***********************************/

#define TEST_DIR  ".fdpath"
#define TEST_FILE TEST_DIR "/MixedCase.txt"

static bool caseFolding;

/************************************ Code ************************************/

#if ME_WIN_LIKE
    #define makeDir(p)   _mkdir(p)
    #define removeDir(p) _rmdir(p)
    #define removeFile(p) _unlink(p)
#else
    #define makeDir(p)   mkdir(p, 0755)
    #define removeDir(p) rmdir(p)
    #define removeFile(p) unlink(p)
#endif

static void setupFiles(void)
{
    int fd;

    removeFile(TEST_FILE);
    removeDir(TEST_DIR);
    makeDir(TEST_DIR);

    if ((fd = open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644)) >= 0) {
        write(fd, "hello", 5);
        close(fd);
    }
    /*
        Detect whether this file system folds case. The assertions below differ between a
        case-folding file system (macOS APFS, Windows NTFS) and a case-sensitive one (ext4).
     */
    caseFolding = 0;
    if ((fd = open(TEST_DIR "/mixedcase.txt", O_RDONLY, 0)) >= 0) {
        caseFolding = 1;
        close(fd);
    }
}

static void cleanupFiles(void)
{
    removeFile(TEST_FILE);
    removeDir(TEST_DIR);
}

/*
    The resolved path of a descriptor opened by its exact name must end in that exact name.
 */
static void testExactSpelling(void)
{
    char  actual[ME_MAX_FNAME];
    ssize len;
    int   fd;

    fd = open(TEST_FILE, O_RDONLY, 0);
    ttrue(fd >= 0, "Test file must open");
    if (fd < 0) {
        return;
    }
    len = rGetFdPath(fd, actual, sizeof(actual));
    close(fd);

    if (len == R_ERR_NOT_READY) {
        tinfo("Skipped: rGetFdPath is not supported on this platform");
        return;
    }
    ttrue(len > 0, "rGetFdPath must resolve an open descriptor");
    teqz((size_t) len, slen(actual));
    ttrue(sends(actual, "MixedCase.txt"), "Resolved path must carry the on-disk spelling");
    ttrue(actual[0] == '/' || (actual[1] == ':'), "Resolved path must be absolute");
}

/*
    This is the property the authorization check depends on: opening a file by a differently
    cased name on a case-folding file system must still resolve to the on-disk spelling, so the
    caller can tell the two apart and refuse.
 */
static void testCaseVariantResolvesToOnDiskSpelling(void)
{
    char  actual[ME_MAX_FNAME];
    ssize len;
    int   fd;

    if (!caseFolding) {
        tinfo("Skipped: File system is case sensitive - case-variant open cannot succeed");
        return;
    }
    fd = open(TEST_DIR "/mixedcase.txt", O_RDONLY, 0);
    ttrue(fd >= 0, "Case-variant open must succeed on a case-folding file system");
    if (fd < 0) {
        return;
    }
    len = rGetFdPath(fd, actual, sizeof(actual));
    close(fd);

    if (len == R_ERR_NOT_READY) {
        tinfo("Skipped: rGetFdPath is not supported on this platform");
        return;
    }
    ttrue(len > 0, "rGetFdPath must resolve a case-variant descriptor");

    //  The requested spelling was "mixedcase.txt"; the resolved name must be the real one
    ttrue(sends(actual, "MixedCase.txt"), "Resolved path must be the on-disk spelling, not the requested one");
    tfalse(sends(actual, "mixedcase.txt"), "Resolved path must not echo the requested spelling");
}

/*
    Failure must be reported, never a stale or partial buffer.
 */
static void testInvalidDescriptor(void)
{
    char  actual[ME_MAX_FNAME];
    ssize len;

    len = rGetFdPath(-1, actual, sizeof(actual));
    ttrue(len < 0, "An invalid descriptor must fail");
    teqz(slen(actual), 0);

    len = rGetFdPath(0, NULL, 128);
    ttrue(len < 0, "A NULL buffer must fail");
}

/*
    A buffer too small to hold the resolved name must fail rather than return a truncated name.
    A truncated name compared against a requested name is a false match waiting to happen.
 */
static void testTruncationFailsClosed(void)
{
    char  tiny[8];
    ssize len;
    int   fd;

    fd = open(TEST_FILE, O_RDONLY, 0);
    if (fd < 0) {
        return;
    }
    len = rGetFdPath(fd, tiny, sizeof(tiny));
    close(fd);

    if (len == R_ERR_NOT_READY) {
        return;
    }
    ttrue(len < 0, "A buffer too small must fail rather than truncate");
    teqz(slen(tiny), 0);
}

/*
    rGetRealPath underpins the document root containment check.
 */
static void testRealPath(void)
{
    char *docs, *nested, *missing;

    docs = rGetRealPath(TEST_DIR);
    tnotnull(docs);
    if (docs) {
        ttrue(docs[0] == '/' || docs[1] == ':', "Canonical path must be absolute");
        tfalse(scontains(docs, "/./") != 0, "Canonical path must not contain ./");
    }
    //  Relative segments must collapse to the same canonical path
    nested = rGetRealPath(TEST_DIR "/../" TEST_DIR);
    tnotnull(nested);
    if (docs && nested) {
        tmatch(nested, docs);
    }
    //  A path that does not exist must fail, not be synthesized
    missing = rGetRealPath(TEST_DIR "/no-such-file");
    tnull(missing);

    rFree(docs);
    rFree(nested);
    rFree(missing);
}

/* Normalize Windows extended-length handle names to the same plain DOS form as rGetRealPath. */
static void testExtendedPrefix(void)
{
    char path[ME_MAX_FNAME];

    //  A local volume
    scopy(path, sizeof(path), "\\\\?\\C:\\dir\\file.txt");
    teqz((size_t) rStripExtendedPrefix(path), 15);
    tmatch(path, "C:\\dir\\file.txt");

    //  A network share keeps the two leading separators
    scopy(path, sizeof(path), "\\\\?\\UNC\\server\\share\\file.txt");
    teqz((size_t) rStripExtendedPrefix(path), 23);
    tmatch(path, "\\\\server\\share\\file.txt");

    //  A path without the prefix is unchanged
    scopy(path, sizeof(path), "C:\\dir\\file.txt");
    teqz((size_t) rStripExtendedPrefix(path), 15);
    tmatch(path, "C:\\dir\\file.txt");
    scopy(path, sizeof(path), "/usr/local/file.txt");
    teqz((size_t) rStripExtendedPrefix(path), 19);
    tmatch(path, "/usr/local/file.txt");

    //  A device path is not an extended-length path, and the prefix alone leaves nothing
    scopy(path, sizeof(path), "\\\\.\\C:\\dir");
    tmatch(path, "\\\\.\\C:\\dir");
    scopy(path, sizeof(path), "\\\\?\\");
    teqz((size_t) rStripExtendedPrefix(path), 0);
    tmatch(path, "");

    scopy(path, sizeof(path), "");
    teqz((size_t) rStripExtendedPrefix(path), 0);
    ttrue(rStripExtendedPrefix(NULL) < 0);
}

#if ME_WIN_LIKE
/* rGetRealPath and rGetFdPath must produce matching canonical names for containment checks. */
static void testRealPathAgreesWithFdPath(void)
{
    cchar *spellings[] = { TEST_DIR "/mixedcase.txt", TEST_DIR "/MixedCase.txt", ".\\.fdpath\\MIXEDCASE.TXT" };
    char  actual[ME_MAX_FNAME], *real;
    ssize len;
    int   fd, i;

    //  The same file spelled in different case and with different separators
    for (i = 0; i < (int) (sizeof(spellings) / sizeof(*spellings)); i++) {
        real = rGetRealPath(spellings[i]);
        tnotnull(real, "rGetRealPath must resolve the spelling");
        fd = open(spellings[i], O_RDONLY | O_BINARY, 0);
        ttrue(fd >= 0);
        if (fd >= 0) {
            len = rGetFdPath(fd, actual, sizeof(actual));
            close(fd);
            ttrue(len > 0);
            if (real && len > 0) {
                tmatch(real, actual, "Both must report the same canonical path for %s", spellings[i]);
                ttrue(sends(real, "MixedCase.txt"), "The on-disk case must be reported");
            }
        }
        rFree(real);
    }

    //  A directory resolves too, without a trailing separator
    real = rGetRealPath(TEST_DIR "/");
    tnotnull(real);
    if (real) {
        tfalse(sends(real, "\\") || sends(real, "/"), "No trailing separator");
        ttrue(sends(real, ".fdpath"));
    }
    rFree(real);
}
#endif

static void fiberMain(void *arg)
{
    setupFiles();

    testExactSpelling();
    testCaseVariantResolvesToOnDiskSpelling();
    testInvalidDescriptor();
    testTruncationFailsClosed();
    testRealPath();
    testExtendedPrefix();
#if ME_WIN_LIKE
    testRealPathAgreesWithFdPath();
#else
    tinfo("Skipped: Windows resolves paths from a handle, which is not used on this platform");
#endif

    cleanupFiles();
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
