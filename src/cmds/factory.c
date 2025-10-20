/*
    factory.c - Factory Serialize service
 */

/********************************** Includes **********************************/

#include "ioto.h"
#include "crypt.h"

/*********************************** Forwards *********************************/

static int serialize(Web *web);

/************************************ Code ************************************/

PUBLIC int ioStart(void)
{
    webAddAction(ioto->webHost, "/ioto/serialize", (WebProc)serialize, NULL);
    return 0;
}

static int serialize(Web *web)
{
    char *id;

#if FUTURE
    cchar *product, *model;
    product = webGetVar(web, "product", 0);
    model = webGetVar(web, "model", 0);
#endif

    id = cryptID(10);

    webAddHeader(web, "Content-Type", "application-json");
    webWriteFmt(web, "{id:\"%s\"}\n", id);
    webWrite(web, NULL, 0);
    rFree(id);
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
