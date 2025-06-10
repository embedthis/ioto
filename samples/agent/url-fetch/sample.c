/*
    url-fetch/sample.c - Fetch a url.

    This sample provides its own main() so it can parse the command line.
    This sample does not initialize Ioto services (does not call ioInit()).
 */

/********************************** Includes **********************************/

#include "url.h"

/************************************* Locals *********************************/

static cchar *url;

/*********************************** Forwards *********************************/

static void start(void *arg);

/************************************* Code ***********************************/

int main(int argc, char **argv)
{
    rSetLog("stdout:all:all", 0, 1);

    if (argc > 1) {
        url = argv[1];
    }

    rInit(start, 0, 0);

    //  Block until instructed to exit
    rServiceEvents();

    rTerm();
    return 0;
}


/*
    Fetch a URI
*/
static void start(void *arg)
{
    Url     *up;
    char    buf[1024];
    ssize   nbytes;
    int     status;

    up = urlAlloc(0);

    /*
        Set roots so we can verify the peer certificate.
        Alternatively, turn off verifyIssuer via: urlSetVerify(up, 0, 0);
    */
    urlSetCerts(up, NULL, NULL, "url.ca", NULL);

    if (!url) {
        url = "http://www.example.com/index.html";
    }
    status = urlFetch(up, url, "GET", NULL, -1, NULL);
    if (status != 200) {
        printf("Error fetching, status %d, response:\n%s\n", status, urlGetResponse(up));
    } else {
        do {
            if ((nbytes = urlRead(up, buf, sizeof(buf))) == 0) {
                //  End of input
                break;
            } else if (nbytes < 0) {
                //  Error
            }
            //  Write to stdout
            write(1, buf, nbytes);
        } while (nbytes > 0);
        
        fflush(stdout);
    }
    rStop();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
