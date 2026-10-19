/*
    logout-method.tst.c - The built-in logout action is a state change and accepts only POST

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "../utils/scenario.h"

/************************************ Code ************************************/

static bool authenticated(cchar *header)
{
    char url[256];

    return request("GET", SFMT(url, "%s/api/user/whoami", HTTP), NULL, 0, header) == 200;
}

static void testLogoutMethod(void)
{
    char *cookie, *header, url[256];

    cookie = login("guest", FIXTURE_PASSWORD);
    ttrue(cookie != NULL);
    if (!cookie) {
        return;
    }
    header = cookieHeader(cookie);
    ttrue(authenticated(header));

    //  GET and HEAD are refused and the session survives
    ttrue(expect("GET", SFMT(url, "%s/api/user/logout", HTTP), NULL, 0, header, 405));
    ttrue(authenticated(header));
    ttrue(expect("HEAD", SFMT(url, "%s/api/user/logout", HTTP), NULL, 0, header, 405));
    ttrue(authenticated(header));

    //  POST ends the session
    ttrue(expect("POST", SFMT(url, "%s/api/user/logout", HTTP), NULL, 0, header, 302));
    tfalse(authenticated(header));

    rFree(header);
    rFree(cookie);
}

static void fiberMain(void *data)
{
    if (scenarioSetup()) {
        testLogoutMethod();
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
