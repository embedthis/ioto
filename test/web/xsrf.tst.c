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

static void testXsrf(void)
{
    Url  *up, *cold, *upNoCookie;
    char *cookie, *anotherCookie, *anotherToken, *securityToken, *formBody, url[128];
    int  status;

    up = urlAlloc(0);

    /*
        Get an XSRF token to use in a form.
     */
    status = urlFetch(up, "GET", SFMT(url, "%s/test/xsrf/form.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    securityToken = sclone(urlGetHeader(up, "X-XSRF-TOKEN"));
    ttrue(securityToken);
    cookie = urlGetCookie(up, WEB_SESSION_COOKIE);
    ttrue(cookie);

    /*
        Now post back the form with the XSRF token.
        The server action will check and respond with "success"
     */
    urlClose(up);
    status = urlFetch(up, "POST", SFMT(url, "%s/test/xsrf/form.html", HTTP), NULL, 0,
                      "Cookie: %s=%s\r\nX-XSRF-TOKEN: %s\r\n", WEB_SESSION_COOKIE, cookie, securityToken);
    ttrue(status == 200);
    ttrue(smatch(urlGetResponse(up), "success"));

    /*
        POST using the form parameter (-xsrf-) instead of header must succeed.
     */
    urlClose(up);
    formBody = sfmt("-xsrf-=%s&name=John", securityToken);
    status = urlFetch(up, "POST", SFMT(url, "%s/test/xsrf/form.html", HTTP), formBody, (size_t) -1,
                      "Cookie: %s=%s\r\nContent-Type: application/x-www-form-urlencoded\r\n",
                      WEB_SESSION_COOKIE, cookie);
    ttrue(status == 200);
    ttrue(smatch(urlGetResponse(up), "success"));
    rFree(formBody);

    /*
        Post back with the wrong XSRF token. This must fail.
     */
    urlClose(up);
    status = urlFetch(up, "POST", SFMT(url, "%s/test/xsrf/form.html", HTTP), NULL, 0,
                      "Cookie: %s=%s\r\nX-XSRF-TOKEN: %s-bad\r\n", WEB_SESSION_COOKIE, cookie, securityToken);
    ttrue(status == 400);

    /*
        POST without prior GET (no session, no token) must fail.
     */
    cold = urlAlloc(0);
    status = urlFetch(cold, "POST", SFMT(url, "%s/test/xsrf/form.html", HTTP), NULL, 0, NULL);
    ttrue(status == 400);
    urlFree(cold);

    /*
        POST with a valid token header but without the session cookie must fail.
     */
    urlClose(up);
    upNoCookie = urlAlloc(0);
    status = urlFetch(upNoCookie, "POST", SFMT(url, "%s/test/xsrf/form.html", HTTP), NULL, 0,
                      "X-XSRF-TOKEN: %s\r\n", securityToken);
    ttrue(status == 400);
    urlFree(upNoCookie);

    /*
        Post back with no XSRF token. This must fail.
     */
    urlClose(up);
    status = urlFetch(up, "POST", SFMT(url, "%s/test/xsrf/form.html", HTTP), NULL, 0,
                      "Cookie: %s=%s\r\n", WEB_SESSION_COOKIE, cookie);
    ttrue(status == 400);

    /*
        Another request without the cookie must get a new XSRF token.
     */
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/xsrf/form.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    anotherToken = sclone(urlGetHeader(up, "X-XSRF-TOKEN"));
    ttrue(anotherToken);
    ttrue(!smatch(anotherToken, securityToken));
    anotherCookie = urlGetCookie(up, WEB_SESSION_COOKIE);
    ttrue(anotherCookie);
    ttrue(!smatch(anotherCookie, cookie));

    rFree(securityToken);
    rFree(anotherToken);
    urlFree(up);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, &HTTPS)) {
        testXsrf();
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
