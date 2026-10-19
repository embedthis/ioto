/*
    auth.h - Helpers shared by the test/auth group

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#include "../utils/scenario.h"

/*********************************** Locals ***********************************/

//  web.auth.failurePeriod in test/utils/agent/web.json5, plus a margin
#define FAILURE_PERIOD (2100)

//  web.auth.maxFailures in test/utils/agent/web.json5
#define MAX_FAILURES   5

/************************************ Code ************************************/
/*
    Form-encode a value: everything but letters and digits is percent-encoded, so "+" and "/" in a hash survive
 */
static char *formEncode(cchar *value)
{
    RBuf  *buf;
    cchar *cp;

    buf = rAllocBuf(0);
    for (cp = value; cp && *cp; cp++) {
        if (isalnum((uchar) *cp)) {
            rPutCharToBuf(buf, *cp);
        } else {
            rPutToBuf(buf, "%%%02X", (uchar) *cp);
        }
    }
    return rBufToStringAndFree(buf);
}

/*
    POST a login. Returns the status and, if wanted, the response body and the session cookie.
 */
static int postLogin(cchar *username, cchar *password, char **body, char **cookie)
{
    Url  *up;
    char url[256], *data, *user, *pass;
    int  status;

    up = urlAlloc(0);
    user = formEncode(username);
    pass = formEncode(password);
    data = sfmt("username=%s&password=%s", user, pass);
    rFree(user);
    rFree(pass);
    status = urlFetch(up, "POST", SFMT(url, "%s/api/public/login", HTTP), data, slen(data),
                      "Content-Type: application/x-www-form-urlencoded\r\n");
    if (body) {
        *body = sclone(urlGetResponse(up));
    } else {
        urlGetResponse(up);
    }
    if (cookie) {
        *cookie = status == 302 ? urlGetCookie(up, SESSION_COOKIE) : NULL;
    }
    rFree(data);
    urlFree(up);
    return status;
}

/*
    The role the agent reports for a session, or NULL if the session does not authenticate
 */
static char *whoami(cchar *cookie)
{
    Url  *up;
    Json *json;
    char url[256], *header, *role;
    int  status;

    up = urlAlloc(0);
    header = cookieHeader(cookie);
    status = urlFetch(up, "GET", SFMT(url, "%s/api/user/whoami", HTTP), NULL, 0, "%s", header);
    role = NULL;
    if (status == 200 && (json = jsonParse(urlGetResponse(up), 0)) != 0) {
        role = sclone(jsonGet(json, 0, "role", 0));
        jsonFree(json);
    } else {
        urlGetResponse(up);
    }
    rFree(header);
    urlFree(up);
    return role;
}

/*
    The stored hash the password utility makes: bcrypt over username:realm:password, realm "web"
 */
static char *makeHash(cchar *username, cchar *password)
{
    char *input, *hash;

    input = sfmt("%s:web:%s", username, password);
    hash = cryptMakePassword(input, CRYPT_BLOWFISH_SALT_LENGTH, CRYPT_BLOWFISH_ROUNDS);
    rFree(input);
    return hash;
}

/*
    Change the User table through the unit app's admin-only fixture action. Returns the status.
 */
static int changeUser(cchar *adminCookie, cchar *username, cchar *role, cchar *hash, bool remove)
{
    Url  *up;
    RBuf *buf;
    char url[256], *header, *encoded;
    int  status;

    buf = rAllocBuf(0);
    rPutToBuf(buf, "username=%s", username);
    if (role) {
        rPutToBuf(buf, "&role=%s", role);
    }
    if (hash) {
        encoded = formEncode(hash);
        rPutToBuf(buf, "&password=%s", encoded);
        rFree(encoded);
    }
    if (remove) {
        rPutStringToBuf(buf, "&remove=1");
    }
    header = sfmt("Cookie: %s=%s\r\nContent-Type: application/x-www-form-urlencoded\r\n", SESSION_COOKIE,
                  adminCookie);
    up = urlAlloc(0);
    status = urlFetch(up, "POST", SFMT(url, "%s/api/admin/user", HTTP), rGetBufStart(buf), rGetBufLength(buf),
                      "%s", header);
    urlGetResponse(up);
    urlFree(up);
    rFree(header);
    rFreeBuf(buf);
    return status;
}

/*
    Log in as the fixture administrator
 */
static char *loginAdmin(void)
{
    char *cookie;

    cookie = NULL;
    postLogin("admin", FIXTURE_PASSWORD, NULL, &cookie);
    return cookie;
}

/*
    The agent's log, which agent-setup.sh starts in the group directory
 */
static char *readLog(void)
{
    char *log;

    log = rReadFile("ioto.log", NULL);
    return log ? log : sclone("");
}

/*
    Let failures recorded by an earlier test expire, so each test starts below the failure limit
 */
static void waitFailurePeriod(void)
{
    rSleep(FAILURE_PERIOD);
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
