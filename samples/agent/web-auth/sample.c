/*
    web-auth/sample.c - Authenticate web requests
 */

/********************************** Includes **********************************/

#include "ioto.h"

/*********************************** Forwards *********************************/

static void logout(Web *web);
static void login(Web *web);

/************************************* Code ***********************************/
/*
    Entry point
*/
PUBLIC int ioStart()
{
    WebHost *host;

    host = ioto->webHost;
    webAddAction(host, "/auth/login", login, 0);
    webAddAction(host, "/auth/logout", logout, 0);
    return 0;
}


static void login(Web *web)
{
    cchar *password, *username;

    username = webGetVar(web, "username", 0);
    password = webGetVar(web, "password", 0);

    if (smatch(password, "demo")) {
        webLogin(web, username, password);
        webRedirect(web, 302, "/welcome.html");
    } else {
        rError("app", "Login failed");
        webRedirect(web, 401, "/");
    }
}


static void logout(Web *web)
{
    webLogout(web);
    webRedirect(web, 302, "/");
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
