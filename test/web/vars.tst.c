/*
    vars.tst.c - Request variable count cap and literal parameter names

    Query strings, url-encoded bodies and multipart form fields all define request variables, and
    each definition linear-scans the variables already defined, so an uncapped count is quadratic
    work done synchronously before the handler runs. The cap is web.limits.vars and it must hold
    over every producer: capping only the url-encoded parser left multipart, framed against the
    much larger upload limit, unbounded.

    A variable name from the wire is also an opaque key, never a property path. A name containing
    "." or "[" would otherwise create nested objects and could restructure or delete a sibling
    variable, so such a name is refused. An application/json body still nests normally.

    Runs on an in-process host with a cap of 128 so the shared server is untouched, and with no
    web.limits.uploads set so the cap is shown to hold at that limit's disabled default.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define BASE     "http://127.0.0.1:4863"
#define MAX_VARS 128
#define BOUNDARY "----WebKitFormBoundaryVarsTest"

static WebHost *host;

/************************************ Code ************************************/

/*
    Build "a0=1&a1=1&..." with the requested number of pairs.
 */
static char *makeEncoded(int count)
{
    RBuf *buf;
    int  i;

    buf = rAllocBuf(count * 8 + 1);
    for (i = 0; i < count; i++) {
        rPutToBuf(buf, "%sa%d=1", i ? "&" : "", i);
    }
    return rBufToStringAndFree(buf);
}

/*
    Build a multipart body of the requested number of form-field parts. No part carries a
    filename, so every part defines a variable and none creates a file.
 */
static RBuf *makeMultipart(int count)
{
    RBuf *buf;
    int  i;

    buf = rAllocBuf(count * 80 + 64);
    for (i = 0; i < count; i++) {
        rPutToBuf(buf,
                  "--%s\r\n"
                  "Content-Disposition: form-data; name=\"a%d\"\r\n"
                  "\r\n"
                  "1\r\n",
                  BOUNDARY, i);
    }
    rPutToBuf(buf, "--%s--\r\n", BOUNDARY);
    return buf;
}

static int fetchQuery(cchar *query)
{
    Url  *up;
    char url[2048];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/show?%s", BASE, query), NULL, 0, NULL);
    urlFree(up);
    return status;
}

static int postEncoded(cchar *body)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, "POST", SFMT(url, "%s/test/show", BASE), body, slen(body),
                      "Content-Type: application/x-www-form-urlencoded\r\n");
    urlFree(up);
    return status;
}

static int postMultipart(RBuf *body)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", BASE),
                      rGetBufStart(body), rGetBufLength(body),
                      "Content-Type: multipart/form-data; boundary=" BOUNDARY "\r\n");
    urlFree(up);
    return status;
}

/*
    An url-encoded body is accepted at the cap and refused one variable past it.
 */
static void testFormVarCount(void)
{
    char *body;

    body = makeEncoded(MAX_VARS);
    teqi(postEncoded(body), 200, "A form body at the variable cap must be accepted");
    rFree(body);

    body = makeEncoded(MAX_VARS + 1);
    teqi(postEncoded(body), 413, "A form body past the variable cap must be refused");
    rFree(body);
}

/*
    A query string is bounded by the same cap.
 */
static void testQueryVarCount(void)
{
    char *query;

    query = makeEncoded(MAX_VARS);
    teqi(fetchQuery(query), 200, "A query at the variable cap must be accepted");
    rFree(query);

    query = makeEncoded(MAX_VARS + 1);
    teqi(fetchQuery(query), 413, "A query past the variable cap must be refused");
    rFree(query);
}

/*
    Multipart form fields are the producer that was left unbounded. The cap must hold there too,
    and must hold on its own: web.limits.uploads bounds the files a request may create, so it does
    not fire on a body of form fields however many there are.
 */
static void testMultipartVarCount(void)
{
    RBuf *body;

    tgti(host->maxUploads, 0, "The upload file count limit must be enabled by default");

    body = makeMultipart(MAX_VARS);
    teqi(postMultipart(body), 200, "A multipart body at the variable cap must be accepted");
    rFreeBuf(body);

    body = makeMultipart(MAX_VARS + 1);
    teqi(postMultipart(body), 413, "A multipart body past the variable cap must be refused");
    rFreeBuf(body);
}

/*
    The cap is a per-request budget over the query and the body together, so neither producer can
    be used to pay for the other.
 */
static void testCombinedBudget(void)
{
    Url  *up;
    char *body, *query, url[2048];
    int  status;

    query = makeEncoded(MAX_VARS / 2);
    body = makeEncoded(MAX_VARS);
    up = urlAlloc(0);
    status = urlFetch(up, "POST", SFMT(url, "%s/test/show?%s", BASE, query), body, slen(body),
                      "Content-Type: application/x-www-form-urlencoded\r\n");
    teqi(status, 413, "A query and a body must share one variable budget");
    urlFree(up);
    rFree(body);
    rFree(query);
}

/*
    A name from the wire is a key, not a path: the structural characters are refused rather than
    honored, so no request can nest, retype or delete a variable.
 */
static void testLiteralNames(void)
{
    teqi(fetchQuery("user_role=admin"), 200, "An ordinary parameter name must be accepted");
    teqi(fetchQuery("user.role=admin"), 400, "A dotted query parameter name must be refused");
    teqi(fetchQuery("a%5B0%5D=1"), 400, "A subscripted query parameter name must be refused");
    teqi(postEncoded("user.role=admin"), 400, "A dotted form parameter name must be refused");
}

/*
    An application/json body is parsed by the JSON parser, not by the name/value parser, so it
    still nests and is not subject to the parameter name rule.
 */
static void testJsonBodyStillNests(void)
{
    Json *json;
    Url  *up;
    char url[128];

    up = urlAlloc(0);
    json = urlJson(up, "POST", SFMT(url, "%s/test/show", BASE), "{\"user\":{\"role\":\"admin\"}}",
                   (size_t) -1, "Content-Type: application/json\r\n");
    tnotnull(json);
    if (json) {
        tmatch(jsonGet(json, 0, "form.user.role", 0), "admin", "A JSON body must still nest");
        jsonFree(json);
    }
    urlFree(up);
}

/*
    The cap exists because the work is quadratic in the variable count. A request at the cap must
    still complete promptly; the uncapped multipart path took minutes of synchronous CPU, which on
    a single-threaded runtime is the whole process.
 */
static void testAtCapIsPrompt(void)
{
    RBuf  *body;
    Ticks elapsed, start;
    char  *encoded;

    encoded = makeEncoded(MAX_VARS);
    start = rGetTicks();
    teqi(postEncoded(encoded), 200);
    elapsed = rGetTicks() - start;
    ttrue(elapsed < 2000, "A form body at the cap took %lld msec", (long long) elapsed);
    rFree(encoded);

    body = makeMultipart(MAX_VARS);
    start = rGetTicks();
    teqi(postMultipart(body), 200);
    elapsed = rGetTicks() - start;
    ttrue(elapsed < 2000, "A multipart body at the cap took %lld msec", (long long) elapsed);
    rFreeBuf(body);
}

static void fiberMain(void *data)
{
    host = startTestHost("{ web: { documents: './site', listen: ['http://127.0.0.1:4863'], "
                         "limits: { vars: 128 }, upload: { dir: './tmp' }, "
                         "routes: [ { match: '/test/', handler: 'action' }, {} ] } }");
    if (host) {
        teqi(host->maxVars, MAX_VARS, "web.limits.vars must configure the cap");
        testFormVarCount();
        testQueryVarCount();
        testMultipartVarCount();
        testCombinedBudget();
        testLiteralNames();
        testJsonBodyStillNests();
        testAtCapIsPrompt();
        stopTestHost(host);
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
