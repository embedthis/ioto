/*
    host.c.tst - Unit tests for WebHost functionality

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void testHostAllocation()
{
    WebHost *host;
    Json    *config;

    webInit();
    config = jsonParseFile("web.json5", NULL, 0);
    ttrue(config);

    host = webAllocHost(config, 0);
    ttrue(host);
    ttrue(host->config == config);
    ttrue(host->listeners);
    ttrue(host->webs);
    ttrue(host->sessions);
    ttrue(host->methods);
    ttrue(host->mimeTypes);
    ttrue(host->actions);
    ttrue(host->routes);
    // Note: users and redirects may be NULL if not configured in web.json5

    webFreeHost(host);
    webTerm();
}

static void testHostConfiguration()
{
    WebHost *host;
    Json    *config;
    cchar   *docs;

    webInit();
    config = jsonParseFile("web.json5", NULL, 0);
    ttrue(config);

    host = webAllocHost(config, 0);
    ttrue(host);

    docs = webGetDocs(host);
    ttrue(docs);
    tmatch(docs, "./site");

    // Test timeout configuration
    ttrue(host->inactivityTimeout > 0);
    ttrue(host->parseTimeout > 0);
    ttrue(host->requestTimeout > 0);
    ttrue(host->sessionTimeout > 0);

    // Test limits configuration
    ttrue(host->maxBuffer > 0);
    ttrue(host->maxHeader > 0);
    ttrue(host->maxConnections > 0);
    ttrue(host->maxBody > 0);
    ttrue(host->maxSessions > 0);

    webFreeHost(host);
    webTerm();
}

static void testHostDefaultIP()
{
    WebHost *host;
    Json    *config;

    webInit();
    config = jsonParseFile("web.json5", NULL, 0);
    ttrue(config);

    host = webAllocHost(config, 0);
    ttrue(host);

    webSetHostDefaultIP(host, "192.168.1.100");
    tmatch(host->ip, "192.168.1.100");

    webFreeHost(host);
    webTerm();
}

static void testHostActions()
{
    WebHost *host;
    Json    *config;

    webInit();
    config = jsonParseFile("web.json5", NULL, 0);
    ttrue(config);

    host = webAllocHost(config, 0);
    ttrue(host);

    // Test adding an action
    webAddAction(host, "/test", NULL, "user");
    ttrue(rGetListLength(host->actions) > 0);

    webFreeHost(host);
    webTerm();
}

static void testHostFlags()
{
    WebHost *host;
    Json    *config;

    webInit();
    config = jsonParseFile("web.json5", NULL, 0);
    ttrue(config);

    // Test with show flags
    host = webAllocHost(config, WEB_SHOW_REQ_HEADERS | WEB_SHOW_RESP_HEADERS);
    ttrue(host);
    ttrue(host->flags & WEB_SHOW_REQ_HEADERS);
    ttrue(host->flags & WEB_SHOW_RESP_HEADERS);

    webFreeHost(host);
    webTerm();
}

static void fiberMain(void *arg)
{
    testHostAllocation();
    testHostConfiguration();
    testHostDefaultIP();
    testHostActions();
    testHostFlags();
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
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */