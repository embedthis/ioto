/*
    unix-socket-type.tst.c - Refuse Unix sockets as files or directories.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

#if ME_UNIX_LIKE
    #include <sys/socket.h>
    #include <sys/un.h>
#endif

/*********************************** Locals ***********************************/

static char *HTTPS;

#if ME_UNIX_LIKE
static cchar *SOCK_PATH = "site/type-test.sock";
#endif

/************************************ Code ************************************/

#if ME_UNIX_LIKE
static bool createSocket(cchar *path)
{
    struct sockaddr_un addr;
    int                fd;

    unlink(path);
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        return false;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    scopy(addr.sun_path, sizeof(addr.sun_path), path);
    if (bind(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        close(fd);
        unlink(path);
        return false;
    }
    close(fd);
    return true;
}
#endif

/*
    A socket must return 404, not a directory redirect.
 */
static void testUnixSocketNotServedAsDirectory(void)
{
#if ME_UNIX_LIKE
    Url   *up;
    char  url[256];
    int   status;

    if (!createSocket(SOCK_PATH)) {
        tfail("Cannot create unix socket fixture");
        return;
    }
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/type-test.sock", HTTPS), NULL, 0, NULL);
    ttrue(status != 301, "A socket must not be redirected to as a directory");
    teqi(status, 404, "A socket must be refused, not served as a file or directory");
    urlFree(up);

    unlink(SOCK_PATH);
#else
    tinfo("Skipped: Unix-domain sockets are not supported on this platform");
#endif
}

static void fiberMain(void *arg)
{
    if (setup(NULL, &HTTPS)) {
        testUnixSocketNotServedAsDirectory();
    }
    rFree(HTTPS);
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
