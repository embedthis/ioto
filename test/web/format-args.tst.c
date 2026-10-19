/*
    format-args.tst.c - Regression tests for variadic format argument handling

    A format conversion that does not match the width of its argument consumes the wrong number
    of varargs slots. When pointer arguments follow the mismatched conversion, every one of them
    is read from the wrong slot and then dereferenced as a string - disclosing unrelated memory
    into a response header, or faulting.

    These tests pin the rendered output of every response header and body that is produced by a
    variadic wrapper carrying a 64-bit argument ahead of a string argument, and confirm that
    caller-supplied data is never interpreted as a format string.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

/*
    web.json5 sets web.timeouts.session to '5 mins', so every Max-Age must render as 300.
 */
#define SESSION_MAX_AGE "300"

/*
    web.json5 sets web.timeouts.request to '10 mins'. Keep-Alive reports what is left of it, so
    this is the ceiling a correct rendering can reach, not the value it must equal.
 */
#define REQUEST_TIMEOUT 600

/************************************ Code ************************************/

/*
    Every byte of a response header must be printable ASCII. A shifted varargs slot dereferenced
    as a string is the way unrelated memory reaches the client, and it is almost never printable.
 */
static bool isCleanHeader(cchar *value)
{
    cchar *cp;

    if (value == 0) {
        return 0;
    }
    for (cp = value; *cp; cp++) {
        if (*cp < 0x20 || *cp > 0x7e) {
            return 0;
        }
    }
    return 1;
}

/*
    webSetCookie formats a 64-bit Max-Age ahead of four string arguments. Pin the whole header.
 */
static void testSetCookieRendering(void)
{
    Url   *up;
    cchar *cookie;
    char  url[256];
    int   status;

    up = urlAlloc(0);

    status = urlFetch(up, "GET", SFMT(url, "%s/test/cookie?name=tcookie&value=tvalue&path=/tpath",
                                      HTTP), NULL, 0, NULL);
    teqi(status, 200);

    cookie = urlGetHeader(up, "Set-Cookie");
    tnotnull(cookie);
    ttrue(isCleanHeader(cookie), "Set-Cookie must contain only printable ASCII");

    /*
        Exact rendering. Anything else means a conversion consumed the wrong number of slots.
     */
    tmatch(cookie, "tcookie=tvalue; Max-Age=" SESSION_MAX_AGE "; path=/tpath; HttpOnly; SameSite=Lax");

    urlFree(up);
}

/*
    Max-Age above 2^31 seconds. This is the case a %d conversion cannot render on any ABI:
    it truncates to 32 bits and wraps negative. On 32-bit ABIs the same defect additionally
    shifts the four string arguments that follow.
 */
static void testLargeMaxAgeRendering(void)
{
    Url   *up;
    cchar *cookie;
    char  url[256];
    int   status;

    up = urlAlloc(0);

    //  3000000000 seconds does not fit in a signed 32-bit integer
    status = urlFetch(up, "GET",
                      SFMT(url, "%s/test/cookie?name=big&value=v&path=/bp&lifespan=3000000000", HTTP),
                      NULL, 0, NULL);
    teqi(status, 200);

    cookie = urlGetHeader(up, "Set-Cookie");
    tnotnull(cookie);
    ttrue(isCleanHeader(cookie), "Large Max-Age Set-Cookie must contain only printable ASCII");
    tmatch(cookie, "big=v; Max-Age=3000000000; path=/bp; HttpOnly; SameSite=Lax");

    urlFree(up);
}

/*
    The session cookie travels the same code path on the authenticated hot path.
 */
static void testSessionCookieRendering(void)
{
    Url   *up;
    cchar *cookie;
    char  url[256];
    int   status;

    up = urlAlloc(0);

    status = urlFetch(up, "GET", SFMT(url, "%s/test/session/create", HTTP), NULL, 0, NULL);
    teqi(status, 200);

    cookie = urlGetHeader(up, "Set-Cookie");
    tnotnull(cookie);
    ttrue(isCleanHeader(cookie), "Session Set-Cookie must contain only printable ASCII");

    ttrue(scontains(cookie, "Max-Age=" SESSION_MAX_AGE ";") != 0, "Max-Age must render the configured lifetime");
    ttrue(scontains(cookie, "; path=/;") != 0, "path must render after Max-Age");
    ttrue(scontains(cookie, "HttpOnly;") != 0, "HttpOnly must render");
    ttrue(scontains(cookie, "SameSite=Lax") != 0, "SameSite must render");
    ttrue(scontains(cookie, WEB_SESSION_COOKIE) != 0, "Cookie name must render");

    urlFree(up);
}

/*
    The pre-auth path. webCheckSecurityToken calls webAddSecurityToken on failure, which creates
    a session and sets a cookie - reachable with no credentials on any xsrf route.
 */
static void testAnonymousXsrfCookieRendering(void)
{
    Url   *up;
    cchar *cookie, *token;
    char  url[256];
    int   i, status;

    /*
        Repeat so a shifted slot that happens to hold a printable byte on one request is caught.
        Each iteration creates a session, so stay well inside web.limits.sessions.
     */
    for (i = 0; i < 50; i++) {
        up = urlAlloc(0);
        status = urlFetch(up, "GET", SFMT(url, "%s/test/xsrf/", HTTP), NULL, 0, NULL);
        teqi(status, 200);

        cookie = urlGetHeader(up, "Set-Cookie");
        tnotnull(cookie);
        ttrue(isCleanHeader(cookie), "Anonymous XSRF Set-Cookie must contain only printable ASCII");
        ttrue(scontains(cookie, "Max-Age=" SESSION_MAX_AGE ";") != 0, "Anonymous Max-Age must render correctly");
        ttrue(scontains(cookie, "SameSite=Lax") != 0, "Anonymous SameSite must render");

        token = urlGetHeader(up, WEB_XSRF_HEADER);
        if (token) {
            ttrue(isCleanHeader(token), "XSRF token header must contain only printable ASCII");
        }
        urlFree(up);
    }
}

/*
    Content-Length is formatted from an ssize. Verify it agrees with the body actually delivered.
 */
static void testContentLengthRendering(void)
{
    Url   *up;
    cchar *header, *body;
    char  url[256];
    int   status;

    up = urlAlloc(0);

    status = urlFetch(up, "GET", SFMT(url, "%s/test/bulk?count=500", HTTP), NULL, 0, NULL);
    teqi(status, 200);

    body = urlGetResponse(up);
    tnotnull(body);

    header = urlGetHeader(up, "Content-Length");
    if (header) {
        ttrue(isCleanHeader(header), "Content-Length must contain only printable ASCII");
        teqz((ssize) stoi(header), slen(body));
    }
    urlFree(up);
}

/*
    Keep-Alive carries a 64-bit timeout followed by a 64-bit request count.
 */
static void testKeepAliveRendering(void)
{
    Url   *up;
    cchar *header, *cp;
    char  url[256];
    int64 timeout;
    int   status;

    up = urlAlloc(0);

    status = urlFetch(up, "GET", SFMT(url, "%s/test/success", HTTP), NULL, 0, NULL);
    teqi(status, 200);

    header = urlGetHeader(up, "Keep-Alive");
    if (header) {
        ttrue(isCleanHeader(header), "Keep-Alive must contain only printable ASCII");
        /*
            The timeout is what remains of the request budget, not the budget itself, and the
            division to seconds truncates -- so it reads one less as soon as a single millisecond
            has passed on the connection. Pinning REQUEST_TIMEOUT exactly asserted that the host
            answered within the same millisecond it accepted, which held on a fast loopback and
            failed on a slower one. Assert the range instead: a shifted or truncated 64-bit
            conversion lands well outside it, which is what this file exists to catch, and a slow
            host does not. The header is reported so a failure says which of the two it was.
         */
        cp = scontains(header, "timeout=");
        ttrue(cp != 0, "Keep-Alive must carry a timeout: %s", header);
        if (cp) {
            timeout = stoi(&cp[sizeof("timeout=") - 1]);
            ttrue(0 < timeout && timeout <= REQUEST_TIMEOUT,
                  "Keep-Alive timeout must render a plausible value: %s", header);
        }
        ttrue(scontains(header, "max=") != 0, "Keep-Alive max must render: %s", header);
    }
    urlFree(up);
}

/*
    webWriteEvent formats a 64-bit event id ahead of two string arguments. An id above 2^32
    is what separates a correct conversion from a shifted one.
 */
typedef struct EventCheck {
    int64 expectId;
    int   count;
    int   badId;
    int   badEvent;
    int   badData;
} EventCheck;

static void eventIdCallback(Url *up, ssize id, cchar *event, cchar *data, void *arg)
{
    EventCheck *check;

    check = (EventCheck*) arg;
    if (!event || !data) {
        return;
    }
    if ((int64) id != check->expectId + check->count) {
        check->badId++;
    }
    if (!smatch(event, "test")) {
        check->badEvent++;
    }
    if (!sstarts(data, "Event ")) {
        check->badData++;
    }
    check->count++;
}

static void testSseEventIdRendering(void)
{
    EventCheck check;
    char       url[256];
    int        rc;

    memset(&check, 0, sizeof(check));

    /*
        An id that does not fit in 32 bits. A narrow conversion here truncates the id and
        shifts the "test" and data pointers that follow it.
     */
    check.expectId = 4294967296LL + 12345;

    rc = urlGetEvents(SFMT(url, "%s/test/event?id=%lld&count=10", HTTP, (long long) check.expectId),
                      eventIdCallback, &check, NULL);
    teqi(rc, 0);

    teqi(check.count, 10);
    teqi(check.badId, 0);
    teqi(check.badEvent, 0);
    teqi(check.badData, 0);
}

/*
    Caller-supplied strings must never be used as a format string. webSetSessionVar and webLogin
    both take a printf format; passing user data there lets a client insert conversions that read
    varargs slots that were never pushed.
 */
static void testFormatStringInjection(void)
{
    Url   *up;
    cchar *response;
    char  url[256];
    int   status;

    /*
        A session variable value holding conversions must round-trip byte for byte.
     */
    up = urlAlloc(0);
    status = urlFetch(up, "GET",
                      SFMT(url, "%s/test/session/setvar?name=probe&value=%%25s%%25s%%25s%%25n", HTTP),
                      NULL, 0, NULL);
    teqi(status, 200);
    response = urlGetResponse(up);
    tmatch(response, "%s%s%s%n");
    urlFree(up);

    /*
        A username holding conversions must be stored and echoed literally.
     */
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/session/login?user=%%25s%%25n%%25p&role=user", HTTP),
                      NULL, 0, NULL);
    teqi(status, 200);
    response = urlGetResponse(up);
    tmatch(response, "%s%n%p");
    urlFree(up);
}

/*
    The session timeout is stored as Ticks. A 32-bit field would clamp any lifetime beyond
    about 24.8 days, so a configured 30 days could never render.
 */
static void testLongSessionTimeout(void)
{
    Json    *config;
    WebHost *host;

    config = jsonParse("{ web: { documents: './site', listen: [], timeouts: { session: '30 days' } } }", 0);
    tnotnull(config);

    host = webAllocHost(config, 0);
    tnotnull(host);

    //  30 days in seconds is 2592000; sessionTimeout is held in ticks (milliseconds)
    teqi((int) (host->sessionTimeout / TPS), 2592000);

    webFreeHost(host);
    jsonFree(config);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testSetCookieRendering();
        testLargeMaxAgeRendering();
        testSessionCookieRendering();
        testAnonymousXsrfCookieRendering();
        testContentLengthRendering();
        testKeepAliveRendering();
        testSseEventIdRendering();
        testFormatStringInjection();
        testLongSessionTimeout();
    }
    rFree(HTTP);
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
