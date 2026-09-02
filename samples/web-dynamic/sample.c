/*
    web-dynamic/sample.c - Serve dynamic web content

    This sample provides its own main() and creates its own web server.
    This sample does not initialize Ioto services (does not call ioInit()).
 */

/********************************** Includes **********************************/

#include "ioto.h"

/*********************************** Forwards *********************************/

static int start(void);
static void testAction(Web *web);

/************************************* Code ***********************************/
/*
    If you don't provide your own main, the default Ioto library main() will be used
    which will create the Ioto web server if enabled in config/ioto.json5 if present.
*/
int main(int argc, char **argv)
{
    rInit((RFiberProc) start, 0);

    //  Block until instructed to exit
    rServiceEvents();
    rTerm();
    return 0;
}


/*
    Create web server and listen for requests
*/
static int start(void)
{
    WebHost *host;
    Json    *config;
    char    *error;

    if ((config = jsonParseFile("web.json5", &error, 0)) == 0) {
        rError("config", "%s", error);
        rFree(error);
        return R_ERR_CANT_READ;
    }
    host = webAllocHost(config, 0);
    webAddAction(host, "/test", testAction, 0);
    webStartHost(host);
    return 0;
}


//  Invoked for /test request
static void testAction(Web *web)
{
    char  buf[ME_BUFSIZE];
    ssize nbytes;

    //  Read POST body data
    while ((nbytes = webRead(web, buf, sizeof(buf))) > 0) {
        printf("Got body data %.*s\n", (int) nbytes, buf);
    }
    //  Respond
    webWriteFmt(web, "Hello World, now is: %s\n", rFormatLocalTime(0, 0));
    webFinalize(web);
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
