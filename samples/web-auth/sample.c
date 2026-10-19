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
PUBLIC int ioStart(void)
{
    WebHost *host;

    host = ioto->webHost;
    webAddAction(host, "/auth/login", login, 0);
    webAddAction(host, "/auth/logout", logout, 0);
    return 0;
}


/*
    Verify the password against the user's stored hash, then log in with the role the server holds
    for that user. The role never comes from the request.
 */
static void login(Web *web)
{
    WebUser *user;
    cchar   *password, *username;

    username = webGetVar(web, "username", 0);
    password = webGetVar(web, "password", 0);

    if (username && password && webVerifyUserPassword(web->host, username, password) &&
        (user = webLookupUser(web->host, username)) != 0 && webLogin(web, username, user->role)) {
        webRedirectLocal(web, 302, "/welcome.html");
    } else {
        rError("app", "Login failed");
        webError(web, 401, "Login failed");
    }
}


static void logout(Web *web)
{
    webLogout(web);
    webRedirectLocal(web, 302, "/login.html");
}


PUBLIC void ioStop(void) {}


/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
