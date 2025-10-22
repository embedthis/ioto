/*
    session.c.tst - Unit tests for sessions and XSRF

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testSession(void)
{
    Url   *up;
    cchar *token, *response;
    char  *cookie, url[128];
    int   status;

    up = urlAlloc(0);

    /*
        Create a token and store it in the session
     */
    status = urlFetch(up, "GET", SFMT(url, "%s/test/session/create", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    token = urlGetResponse(up);
    ttrue(token);
    cookie = urlGetCookie(up, WEB_SESSION_COOKIE);
    ttrue(cookie);

    /*
        Check if the token matches that stored in the session
     */
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/session/check?%s", HTTP, token), NULL, 0,
                      "Cookie: %s=%s\r\n", WEB_SESSION_COOKIE, cookie);
    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(smatch(response, "success"));

    urlFree(up);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, &HTTPS)) {
        testSession();
    }
    rFree(HTTP);
    rFree(HTTPS);
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}
