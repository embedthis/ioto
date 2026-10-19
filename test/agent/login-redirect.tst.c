/*
    login-redirect.tst.c - The login and logout redirects keep the client on its own origin

    The built-in login and logout actions answer a success with a 302. Built as an absolute URI
    from the backend socket, that Location names the backend's scheme and port, so behind a
    TLS-terminating proxy the user is sent to a plaintext internal address right after
    authenticating. A same-origin redirect is a relative reference (RFC 9110 10.2.2), so the
    client keeps the scheme, host and port it used.

    Each assertion is on the exact Location: anything carrying "://" has named an origin.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "../utils/scenario.h"

/************************************ Code ************************************/
/*
    POST a form to a URL and return the Location of its redirect, or NULL when the status was not
    the one expected. The caller frees the result.
 */
static char *redirectLocation(cchar *url, cchar *body, cchar *cookie, int expected)
{
    Url   *up;
    cchar *location;
    char  *headers, *result;
    int   status;

    up = urlAlloc(0);
    headers = sfmt("Content-Type: application/x-www-form-urlencoded\r\n%s", cookie ? cookie : "");
    status = urlFetch(up, "POST", url, body, slen(body), "%s", headers);
    urlGetResponse(up);
    result = NULL;
    if (status != expected) {
        tinfo("  POST %s answered %d, expected %d", url, status, expected);
    } else if ((location = urlGetHeader(up, "location")) != 0) {
        result = sclone(location);
    }
    rFree(headers);
    urlFree(up);
    return result;
}

/*
    A successful login redirects to "/" and names no origin
 */
static void testLoginRedirect(cchar *endpoint)
{
    char url[256], *location;

    location = redirectLocation(SFMT(url, "%s/api/public/login", endpoint), "username=admin&password=" FIXTURE_PASSWORD, NULL, 302);
    ttrue(location != NULL);
    if (location) {
        tmatch(location, "/", "Login redirects to a relative \"/\"");
        rFree(location);
    }
}

/*
    Logout is behind the user role, so it redirects a logged-in session to "/"
 */
static void testLogoutRedirect(void)
{
    char url[256], *cookie, *headers, *location;

    cookie = login("admin", FIXTURE_PASSWORD);
    ttrue(cookie != NULL);
    if (!cookie) {
        return;
    }
    headers = cookieHeader(cookie);
    location = redirectLocation(SFMT(url, "%s/api/user/logout", HTTP), "", headers, 302);
    ttrue(location != NULL);
    if (location) {
        tmatch(location, "/", "Logout redirects to a relative \"/\"");
        rFree(location);
    }
    rFree(headers);
    rFree(cookie);
}

static void fiberMain(void *data)
{
    if (scenarioSetup()) {
        testLoginRedirect(HTTP);
        //  The TLS listener is the same code on a secure socket: its Location must not name https either
        testLoginRedirect(HTTPS);
        testLogoutRedirect();
    }
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
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
