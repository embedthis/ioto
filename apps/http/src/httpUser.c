/*
    httpUser.c - User authentication status

    Login and logout are the agent's webLoginUser and webLogoutUser, installed at web.auth.login and
    web.auth.logout in web.json5.
 */

#include "http.h"

/************************************* Code ***********************************/
/*
    Return the current authentication status as JSON: the username and role if logged in, otherwise {}
 */
static void getAuth(Web *web)
{
    Json *json;

    json = jsonParse("{}", 0);
    webAuthenticate(web);
    if (web->username) {
        jsonSet(json, 0, "username", web->username, JSON_STRING);
        jsonSet(json, 0, "role", web->role, JSON_STRING);
    }
    webAddHeaderStaticString(web, "Content-Type", "application/json");
    webWriteJson(web, json);
    webFinalize(web);
    jsonFree(json);
}

PUBLIC int httpAddUser(WebHost *host)
{
    webAddAction(host, "/api/public/getAuth", getAuth, NULL);
    return 0;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
