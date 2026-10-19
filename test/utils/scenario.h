/*
    scenario.h - Helpers shared by the groups that drive the assembled agent (test/soak, test/stress)

    TestMe compiles each .tst.c on its own, so anything two of them share lives here as static
    functions. The endpoints come from TM_HTTP and TM_HTTPS, which the group's testme.json5 sets
    and agent-prep.sh wrote into the agent's configuration, so a test and its server cannot
    disagree about the port.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "url.h"
#include "web.h"
#include "websock.h"
#include "testme.h"

#include <stdio.h>
#include <sys/stat.h>

/*********************************** Locals ***********************************/

static cchar *HTTP;
static cchar *HTTPS;

//  The session cookie the agent sets on login. web.json5 does not override the default.
#define SESSION_COOKIE WEB_SESSION_COOKIE

//  The password of the accounts in test/utils/agent/db.json5
#define FIXTURE_PASSWORD "scenario-fixture-pw"

//  What the unit app's /api/public/stats fixture reports about its own process
typedef struct Stats {
    int64 rssKb;        /* resident set, KB; 0 where the platform cannot report it */
    int   fds;          /* open descriptors; 0 where the platform cannot report it */
    int   sessions;     /* live sessions */
} Stats;

/************************************ Code ************************************/
/*
    Resolve the endpoints and trust the test CA. Returns false, having failed the test, when the
    group's environment is not set.
 */
static bool scenarioSetup(void)
{
    HTTP = tget("TM_HTTP", NULL);
    HTTPS = tget("TM_HTTPS", NULL);
    if (!HTTP || !HTTPS) {
        tfail("TM_HTTP and TM_HTTPS must be set by the group's testme.json5");
        return 0;
    }
    rSetSocketDefaultCerts("../certs/ca.crt", 0, 0, 0);
    urlSetDefaultTimeout(60 * TPS);
    return 1;
}

/*
    Issue one request and return its status, draining the body so the request is completed
    rather than abandoned. A headers string is a fully formed header block ending in CRLF.
 */
static int request(cchar *method, cchar *url, cvoid *data, size_t size, cchar *headers)
{
    Url *up;
    int status;

    up = urlAlloc(0);
    status = urlFetch(up, method, url, data, size, headers ? "%s" : NULL, headers);
    urlGetResponse(up);
    urlFree(up);
    return status;
}

/*
    Issue one request and return true when it answered with the expected status
 */
static bool expect(cchar *method, cchar *url, cvoid *data, size_t size, cchar *headers, int expected)
{
    int status;

    status = request(method, url, data, size, headers);
    if (status != expected) {
        tinfo("  %s %s answered %d, expected %d", method, url, status, expected);
        return 0;
    }
    return 1;
}

/*
    GET a document and count its body without buffering it, so a response larger than the url
    client's buffer limit is still measured in full. Returns the byte count, or -1 on a failed
    request, and the status through *status.
 */
static int64 fetchBytes(cchar *url, int *status)
{
    Url   *up;
    char  buf[16 * 1024];
    ssize n;
    int64 total;

    up = urlAlloc(0);
    total = -1;
    *status = 0;
    if (urlStart(up, "GET", url) == 0 && urlFinalize(up) == 0) {
        total = 0;
        while ((n = urlRead(up, buf, sizeof(buf))) > 0) {
            total += n;
        }
        if (n < 0) {
            total = -1;
        }
        *status = urlGetStatus(up);
    }
    urlFree(up);
    return total;
}

/*
    Log in as a seeded user and return the session cookie value, or NULL. The built-in login
    action answers a success with a redirect to "/", carrying the Set-Cookie.
 */
static char *login(cchar *username, cchar *password)
{
    Url  *up;
    char url[256], body[256], *cookie;
    int  status;

    up = urlAlloc(0);
    SFMT(body, "username=%s&password=%s", username, password);
    status = urlFetch(up, "POST", SFMT(url, "%s/api/public/login", HTTP), body, slen(body),
                      "Content-Type: application/x-www-form-urlencoded\r\n");
    urlGetResponse(up);
    cookie = (status == 302) ? urlGetCookie(up, SESSION_COOKIE) : NULL;
    urlFree(up);
    return cookie;
}

/*
    The Cookie header that presents a session
 */
static char *cookieHeader(cchar *cookie)
{
    return sfmt("Cookie: %s=%s\r\n", SESSION_COOKIE, cookie);
}

/*
    The integer that follows "key": in a flat JSON body, or -1
 */
static int64 jsonNumber(cchar *body, cchar *key)
{
    char  pattern[64];
    cchar *cp;

    SFMT(pattern, "\"%s\":", key);
    if (!body || (cp = scontains(body, pattern)) == 0) {
        return -1;
    }
    return stoi(cp + slen(pattern));
}

/*
    Sample the agent's own view of its process. Returns false when the fixture did not answer.
 */
static bool getStats(Stats *stats)
{
    Url   *up;
    cchar *body;
    char  url[256];
    bool  ok;

    up = urlAlloc(0);
    ok = 0;
    if (urlFetch(up, "GET", SFMT(url, "%s/api/public/stats", HTTP), NULL, 0, NULL) == 200) {
        body = urlGetResponse(up);
        stats->rssKb = jsonNumber(body, "rss");
        stats->fds = (int) jsonNumber(body, "fds");
        stats->sessions = (int) jsonNumber(body, "sessions");
        ok = stats->rssKb >= 0 && stats->fds >= 0 && stats->sessions >= 0;
    }
    urlFree(up);
    return ok;
}

/*
    Size of a file in bytes, or 0 when it does not exist
 */
static int64 fileSize(cchar *path)
{
    struct stat info;

    if (stat(path, &info) < 0) {
        return 0;
    }
    return (int64) info.st_size;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
