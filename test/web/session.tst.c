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
    teqi(status, 200);
    token = urlGetResponse(up);
    tnotnull(token);
    cookie = urlGetCookie(up, WEB_SESSION_COOKIE);
    tnotnull(cookie);

    /*
        Check if the token matches that stored in the session
     */
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/session/check?%s", HTTP, token), NULL, 0,
                      "Cookie: %s=%s\r\n", WEB_SESSION_COOKIE, cookie);
    teqi(status, 200);
    response = urlGetResponse(up);
    tmatch(response, "success");

    urlFree(up);
}

static void testSessionPersistence()
{
    Url  *up;
    char *token;
    char *cookie, url[128];
    int  status;

    up = urlAlloc(0);

    //  Create a session
    status = urlFetch(up, "GET", SFMT(url, "%s/test/session/create", HTTP), NULL, 0, NULL);
    teqi(status, 200);
    token = sclone(urlGetResponse(up));
    cookie = urlGetCookie(up, WEB_SESSION_COOKIE);
    tnotnull(cookie);

    //  Make another request with the same session cookie
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/session/check?%s", HTTP, token), NULL, 0,
                      "Cookie: %s=%s\r\n", WEB_SESSION_COOKIE, cookie);
    teqi(status, 200);

    //  And one more to verify persistence
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/session/check?%s", HTTP, token), NULL, 0,
                      "Cookie: %s=%s\r\n", WEB_SESSION_COOKIE, cookie);
    teqi(status, 200);

    rFree(token);
    urlFree(up);
}

static void testSessionCookieAttributes()
{
    Url  *up;
    char *cookie, *setCookie, url[128];
    int  status;

    up = urlAlloc(0);

    //  Create a session and check Set-Cookie header
    status = urlFetch(up, "GET", SFMT(url, "%s/test/session/create", HTTP), NULL, 0, NULL);
    teqi(status, 200);
    cookie = urlGetCookie(up, WEB_SESSION_COOKIE);
    tnotnull(cookie);

    //  Check Set-Cookie header for security attributes
    setCookie = (char*) urlGetHeader(up, "Set-Cookie");
    if (setCookie) {
        //  Should have HttpOnly attribute for security
        tcontains(setCookie, "HttpOnly");
        //  Should have SameSite attribute
        tcontains(setCookie, "SameSite");
    }

    urlFree(up);
}

static void testMultipleSessions()
{
    Url  *up1, *up2;
    char *token1, *token2;
    char *cookie1, *cookie2, url[128];
    int  status;

    //  Create two separate sessions
    up1 = urlAlloc(0);
    status = urlFetch(up1, "GET", SFMT(url, "%s/test/session/create", HTTP), NULL, 0, NULL);
    teqi(status, 200);
    token1 = sclone(urlGetResponse(up1));
    cookie1 = sclone(urlGetCookie(up1, WEB_SESSION_COOKIE));
    tnotnull(cookie1);

    up2 = urlAlloc(0);
    status = urlFetch(up2, "GET", SFMT(url, "%s/test/session/create", HTTP), NULL, 0, NULL);
    teqi(status, 200);
    token2 = sclone(urlGetResponse(up2));
    cookie2 = sclone(urlGetCookie(up2, WEB_SESSION_COOKIE));
    tnotnull(cookie2);

    //  Verify sessions are different
    tfalse(smatch(cookie1, cookie2));
    tfalse(smatch(token1, token2));

    //  Verify each session works independently
    urlClose(up1);
    status = urlFetch(up1, "GET", SFMT(url, "%s/test/session/check?%s", HTTP, token1), NULL, 0,
                      "Cookie: %s=%s\r\n", WEB_SESSION_COOKIE, cookie1);
    teqi(status, 200);

    urlClose(up2);
    status = urlFetch(up2, "GET", SFMT(url, "%s/test/session/check?%s", HTTP, token2), NULL, 0,
                      "Cookie: %s=%s\r\n", WEB_SESSION_COOKIE, cookie2);
    teqi(status, 200);

    rFree(token1);
    rFree(token2);
    rFree(cookie1);
    rFree(cookie2);
    urlFree(up1);
    urlFree(up2);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, &HTTPS)) {
        testSession();
        testSessionPersistence();
        testSessionCookieAttributes();
        testMultipleSessions();
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
