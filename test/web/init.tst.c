/*
    init.c.tst - Unit tests for web server init

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void initTest()
{
    webInit();
    webTerm();
}

static void allocHost()
{
    WebHost *host;
    Json    *config;
    cchar   *docs;

    webInit();
    config = jsonParseFile("web.json5", NULL, 0);
    ttrue(config);
    host = webAllocHost(config, 0);
    ttrue(config);

    docs = webGetDocs(host);
    tmatch(docs, "./site");
    webFreeHost(host);
    webTerm();
}

static void fiberMain(void *data)
{
    if (setup(NULL, NULL)) {
        initTest();
        allocHost();
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
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
