/*
    web-upload/sample.c - Process upload requests
 */

/********************************** Includes **********************************/

#include "ioto.h"

/*********************************** Forwards *********************************/

static void upload(Web *web);

/************************************* Code ***********************************/
/*
    Create web server and listen for requests
*/
PUBLIC int ioStart(void)
{
    WebHost *host;

    host = ioto->webHost;
    webAddAction(host, "/api/upload", upload, 0);
    return 0;
}

/*
    The web server has already processed the uploaded file and stored the details in the web->uploads hash
*/
static void upload(Web *web)
{
    WebUpload *upload;
    RName     *np;

    /*
        Iterate over all the uploaded files
    */
    for (ITERATE_NAME_DATA(web->uploads, np, upload)) {
        if (upload->filename) {
            rInfo("app", "Upload %s to %s size %lld", upload->clientFilename, upload->filename,
                  (long long) upload->size);
            // Process
            unlink(upload->filename);
        } else {
            rInfo("app", "Upload form var %s = %s", upload->name, webGetVar(web, upload->name, "null"));
        }
    }
    webWriteResponse(web, 200, "Done");
    //  The web server will remove the uploaded file temporary automatically.
}

PUBLIC void ioStop(void) {}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
