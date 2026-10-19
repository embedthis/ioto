/*
    sendfile-truncate.tst.c - A response that fails after its headers must close the connection

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4875"

#define CONFIG                                                              \
    "{web: {documents: './site', listen: ['" ENDPOINT "'],"                 \
    " routes: [{match: '/test/', handler: 'action'}, {}]}}"

/************************************ Code ************************************/


static void shortFileAction(Web *web)
{
    struct stat info;
    char        *path;
    int         fd;

    path = sfmt("%s/index.html", webGetDocs(web->host));
    if ((fd = open(path, O_RDONLY | O_BINARY)) >= 0) {
        fstat(fd, &info);
        webSetStatus(web, 200);
        if (webSendFile(web, fd, 0, (ssize) info.st_size + 1000) >= 0) {
            webFinalize(web);
        }
        close(fd);
    } else {
        webError(web, 500, "Cannot open fixture");
    }
    rFree(path);
}

/*
    Fail after the headers are committed
 */
static void lateErrorAction(Web *web)
{
    webSetStatus(web, 200);
    webWriteHeaders(web);
    webWrite(web, "partial", 7);
    webError(web, 500, "Failed after the headers were written");
}


static int countStatusLines(cchar *uri)
{
    cchar *segments[1], *cp;
    char  *req, buf[16384];
    int   count;

    req = sfmt("GET %s HTTP/1.1\r\nHost: x\r\n\r\n"
               "GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", uri);
    segments[0] = req;
    rawRequest(ENDPOINT, segments, 1, 0, 2, buf, sizeof(buf));
    rFree(req);
    for (count = 0, cp = buf; (cp = scontains(cp, "HTTP/1.1 ")) != 0; cp++) {
        count++;
    }
    return count;
}

static void testFailureAfterHeadersCloses(void)
{
    WebHost *host;

    if ((host = startTestHost(CONFIG)) == 0) {
        return;
    }
    webAddAction(host, "/test/short", shortFileAction, NULL);
    webAddAction(host, "/test/late", lateErrorAction, NULL);

    teqi(countStatusLines("/test/short"), 1, "A file that ends early must close the connection");
    teqi(countStatusLines("/test/late"), 1, "webError after the headers must close the connection");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (setup(NULL, NULL)) {
        testFailureAfterHeadersCloses();
    }
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
