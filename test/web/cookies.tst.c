/*
    cookies.tst.c - Unit tests for request cookie parsing and response cookie generation

    webGetCookie and webParseCookie read the request Cookie header; webSetCookie emits a
    Set-Cookie response header and enforces the RFC 6265 name and value character sets.
    These run against a WebHost without an HTTP round trip.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static Web *allocTestWeb(WebHost *host, cchar *cookieHeader)
{
    Web *web;

    web = rAllocType(Web);
    web->host = host;
    web->txHeaders = rAllocHash(16, R_DYNAMIC_VALUE);
    web->signature = -1;
    web->status = 200;
    web->cookie = cookieHeader ? sclone(cookieHeader) : 0;
    return web;
}

static void freeTestWeb(Web *web)
{
    rFreeHash(web->txHeaders);
    rFree(web->cookie);
    rFree(web);
}

/*
    Return the first Set-Cookie header emitted on this request, or NULL.
 */
static cchar *getSetCookie(Web *web)
{
    return rLookupName(web->txHeaders, "Set-Cookie");
}

static void testGetCookie(WebHost *host)
{
    Web  *web;
    char *value, *big;

    web = allocTestWeb(host, "name=value");
    value = webGetCookie(web, "name");
    tmatch(value, "value");
    rFree(value);

    //  A missing cookie is not an error
    tnull(webGetCookie(web, "other"));

    //  A name must be supplied
    tnull(webGetCookie(web, NULL));
    tnull(webGetCookie(web, ""));
    freeTestWeb(web);

    //  No Cookie header at all
    web = allocTestWeb(host, NULL);
    tnull(webGetCookie(web, "name"));
    freeTestWeb(web);

    //  Multiple cookies are separated by ";" and may carry surrounding spaces
    web = allocTestWeb(host, "first=one; second=two;third=three");
    value = webGetCookie(web, "first");
    tmatch(value, "one");
    rFree(value);
    value = webGetCookie(web, "second");
    tmatch(value, "two");
    rFree(value);
    value = webGetCookie(web, "third");
    tmatch(value, "three");
    rFree(value);
    freeTestWeb(web);

    //  Double quotes are stripped, but spaces inside them are preserved
    web = allocTestWeb(host, "quoted=\"a b\"");
    value = webGetCookie(web, "quoted");
    tmatch(value, "a b");
    rFree(value);
    freeTestWeb(web);

    //  An empty value is returned as an empty string, not as a missing cookie
    web = allocTestWeb(host, "empty=");
    value = webGetCookie(web, "empty");
    tmatch(value, "");
    rFree(value);
    freeTestWeb(web);

    /*
        A cookie header beyond WEB_MAX_COOKIE_SIZE is refused outright rather than parsed, so an
        oversized header cannot be used to burn CPU in the parser.
     */
    big = rAlloc(WEB_MAX_COOKIE_SIZE + 32);
    memcpy(big, "name=", 5);
    memset(&big[5], 'v', WEB_MAX_COOKIE_SIZE + 26);
    big[WEB_MAX_COOKIE_SIZE + 31] = '\0';
    web = allocTestWeb(host, big);
    tnull(webGetCookie(web, "name"));
    tnull(webParseCookie(web, "name"));
    freeTestWeb(web);
    rFree(big);
}

/*
    webParseCookie is the session-lookup variant. It trims only leading spaces from the name.
 */
static void testParseCookie(WebHost *host)
{
    Web  *web;
    char *value;

    web = allocTestWeb(host, "-web-session-=abc123; other=x");
    value = webParseCookie(web, "-web-session-");
    tmatch(value, "abc123");
    rFree(value);

    tnull(webParseCookie(web, "missing"));
    tnull(webParseCookie(web, NULL));
    tnull(webParseCookie(web, ""));
    freeTestWeb(web);

    web = allocTestWeb(host, NULL);
    tnull(webParseCookie(web, "-web-session-"));
    freeTestWeb(web);
}

static void testSetCookie(WebHost *host)
{
    Web   *web;
    cchar *header;

    web = allocTestWeb(host, NULL);
    teqi(webSetCookie(web, "session", "abc123", "/", 0, 0), 0);
    header = getSetCookie(web);
    tnotnull(header);
    tcontains(header, "session=abc123");
    tcontains(header, "path=/");
    tcontains(header, "Max-Age=");
    //  httpOnly defaults to true in web.json5 and there is no TLS socket, so no Secure flag
    tcontains(header, "HttpOnly");
    tfalse(scontains(header, "Secure"));
    tcontains(header, "SameSite=");
    freeTestWeb(web);

    //  A null value clears the cookie and still emits a well-formed header
    web = allocTestWeb(host, NULL);
    teqi(webSetCookie(web, "session", NULL, "/", 0, 0), 0);
    header = getSetCookie(web);
    tcontains(header, "session=;");
    freeTestWeb(web);

    //  A null path defaults to "/"
    web = allocTestWeb(host, NULL);
    teqi(webSetCookie(web, "session", "v", NULL, 0, 0), 0);
    tcontains(getSetCookie(web), "path=/");
    freeTestWeb(web);

    //  An explicit lifespan is emitted as Max-Age in seconds
    web = allocTestWeb(host, NULL);
    teqi(webSetCookie(web, "session", "v", "/app", 120 * TPS, 0), 0);
    header = getSetCookie(web);
    tcontains(header, "Max-Age=120");
    tcontains(header, "path=/app");
    freeTestWeb(web);
}

/*
    WEB_COOKIE_OVERRIDE selects the flags from the caller rather than from the host config.
 */
static void testSetCookieOverride(WebHost *host)
{
    Web   *web;
    cchar *header;

    //  Override with no flags clears HttpOnly and Secure and forces SameSite=Lax
    web = allocTestWeb(host, NULL);
    teqi(webSetCookie(web, "c", "v", "/", 0, WEB_COOKIE_OVERRIDE), 0);
    header = getSetCookie(web);
    tfalse(scontains(header, "HttpOnly"));
    tfalse(scontains(header, "Secure"));
    tcontains(header, "SameSite=Lax");
    freeTestWeb(web);

    //  Override requesting every flag
    web = allocTestWeb(host, NULL);
    teqi(webSetCookie(web, "c", "v", "/", 0,
                      WEB_COOKIE_OVERRIDE | WEB_COOKIE_HTTP_ONLY | WEB_COOKIE_SECURE | WEB_COOKIE_SAME_SITE), 0);
    header = getSetCookie(web);
    tcontains(header, "HttpOnly");
    tcontains(header, "Secure");
    tcontains(header, "SameSite=");
    freeTestWeb(web);
}

/*
    Names and values are restricted to the RFC 6265 token sets so a caller cannot inject
    attributes or terminate the header.
 */
static void testSetCookieValidation(WebHost *host)
{
    Web  *web;
    char *big;

    web = allocTestWeb(host, NULL);

    //  Separators and control characters are rejected in the name
    tneqi(webSetCookie(web, "bad name", "v", "/", 0, 0), 0);
    tneqi(webSetCookie(web, "bad;name", "v", "/", 0, 0), 0);
    tneqi(webSetCookie(web, "bad=name", "v", "/", 0, 0), 0);
    tneqi(webSetCookie(web, "bad,name", "v", "/", 0, 0), 0);
    tneqi(webSetCookie(web, "bad\"name", "v", "/", 0, 0), 0);
    tneqi(webSetCookie(web, "bad\\name", "v", "/", 0, 0), 0);
    tneqi(webSetCookie(web, "bad\r\nSet-Cookie: x=y", "v", "/", 0, 0), 0);

    //  And in the value
    tneqi(webSetCookie(web, "c", "bad value", "/", 0, 0), 0);
    tneqi(webSetCookie(web, "c", "bad;value", "/", 0, 0), 0);
    tneqi(webSetCookie(web, "c", "bad,value", "/", 0, 0), 0);
    tneqi(webSetCookie(web, "c", "bad\"value", "/", 0, 0), 0);
    tneqi(webSetCookie(web, "c", "bad\\value", "/", 0, 0), 0);
    tneqi(webSetCookie(web, "c", "bad\r\nX-Injected: 1", "/", 0, 0), 0);

    //  Nothing above should have emitted a header
    tnull(getSetCookie(web));

    //  The permitted punctuation is accepted
    teqi(webSetCookie(web, "a!#$%&'*+-.^_`|~9", "v", "/", 0, 0), 0);
    teqi(webSetCookie(web, "c", "!#$%&'()*+-./:=?@[]^_`{|}~", "/", 0, 0), 0);
    freeTestWeb(web);

    //  Names and values are capped at 4096 characters
    web = allocTestWeb(host, NULL);
    big = rAlloc(4098 + 1);
    memset(big, 'a', 4098);
    big[4098] = '\0';
    tneqi(webSetCookie(web, big, "v", "/", 0, 0), 0);
    tneqi(webSetCookie(web, "c", big, "/", 0, 0), 0);
    tnull(getSetCookie(web));
    rFree(big);
    freeTestWeb(web);
}

static void fiberMain(void *arg)
{
    WebHost *host;
    Json    *config;

    webInit();
    config = jsonParseFile("web.json5", NULL, 0);
    tnotnull(config);
    if (config && (host = webAllocHost(config, 0)) != 0) {
        testGetCookie(host);
        testParseCookie(host);
        testSetCookie(host);
        testSetCookieOverride(host);
        testSetCookieValidation(host);
        webFreeHost(host);
    }
    webTerm();
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
