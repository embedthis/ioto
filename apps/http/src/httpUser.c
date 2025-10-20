/*
    httpUser.c - User authentication and management
 */

#include "http.h"

/************************************* Code ***********************************/

static void loginUser(Web *web)
{
    const DbItem *user;
    cchar        *role, *username;

    username = webGetVar(web, "username", 0);

    user = dbFindOne(ioto->db, "User", DB_PROPS("username", username), NULL);
    if (!user) {
        webWriteResponse(web, 400, "Unknown user");
        return;
    }
    if (!cryptCheckPassword(webGetVar(web, "password", 0), dbField(user, "password"))) {
        rTrace("auth", "Password does not match");
        webWriteResponse(web, 401, "Password failed to authenticate");
        //  Security: throttle rate of attempts
        rSleep(500);
        return;

    }
    role = dbField(user, "role");
    if (!webLogin(web, username, role)) {
        webWriteResponse(web, 400, "Unknown user role");
    } else {
        //  The password is removed by the signature
        webWrite(web, "{\"user\":", 1);
        webWriteItem(web, user);
        webWrite(web, "}", 1);
        webFinalize(web);
    }
}

static void logoutUser(Web *web)
{
    webWriteResponse(web, 200, "Logged out");
    webLogout(web);
}


/*
    Return current authentication status if logged in
 */
static void getAuth(Web *web)
{
    webAuthenticate(web);
    if (web->username) {
        //  Logged in and authenticated with a role
        webWriteFmt(web, "{\"username\":\"%s\",\"role\":\"%s\"}", web->username, web->role);
    } else {
        webWriteFmt(web, "{}");
    }
    webFinalize(web);
}

PUBLIC int httpAddUser(WebHost *host)
{
    webAddAction(host, "/api/public/getAuth", getAuth, NULL);
    webAddAction(host, "/api/public/login", loginUser, NULL);
    webAddAction(host, "/api/user/logout", logoutUser, NULL);
    return 0;
}