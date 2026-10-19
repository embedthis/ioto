/*
    sig-enable.tst.c - web.signatures.enable alone validates requests

    Enabling signatures only loaded the file. Validation additionally required an undocumented
    per-route "validate: true", defaulting to off, so a server configured exactly as the hardening
    checklist prescribes loaded the signature file and checked nothing on the request path - no type
    checks, no required fields, no unexpected-field rejection - with no warning that it was off.
    Response validation was unaffected, so a smoke test of a well-formed response looked correct.

    A route with no "validate" key is now validated wherever the request resolves to a signature;
    "validate: false" exempts it, and "validate: true" additionally makes strict mode refuse a
    request that resolves to no signature at all.

    Ports 4867-4869 are used by this test alone.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define SCHEMA "tmp/sig-enable.json5"

#define DEFAULTED "http://127.0.0.1:4867"
#define EXEMPTED  "http://127.0.0.1:4868"
#define STRICT    "http://127.0.0.1:4869"

//  Missing the required "name" and carrying a field the signature does not declare
#define BAD_BODY  "{\"bogus\":1}"
#define GOOD_BODY "{\"name\":\"bob\"}"

/************************************ Code ************************************/

static bool writeSchema(void)
{
    cchar *text;

    text = "{ controller: { described: { "
           "request: { type: 'object', hasRequired: true, fields: { name: { type: 'string', required: true } } }, "
           "response: { type: 'object', hasWild: true } } } }\n";
    return rWriteFile(SCHEMA, text, slen(text), 0644) >= 0;
}

/*
    A host with signatures enabled the documented way. "validate" is only written when the caller
    asks for it, so the default path is exercised as an operator would meet it.
 */
static WebHost *startHost(cchar *listen, cchar *validate, bool strict)
{
    char config[768], key[64];

    key[0] = '\0';
    if (validate) {
        SFMT(key, "validate: %s, ", validate);
    }
    SFMT(config, "{ web: { documents: './site', listen: ['%s'], "
         "signatures: { enable: true, path: '" SCHEMA "', strict: %s }, "
         "routes: [ { match: '/test/sig/', %shandler: 'action', role: 'public' }, "
         "{ match: '/test/', handler: 'action' }, {} ] } }",
         listen, strict ? "true" : "false", key);
    return startTestHost(config);
}

static int post(cchar *endpoint, cchar *method, cchar *body)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, "POST", SFMT(url, "%s/test/sig/controller/%s", endpoint, method), body, (size_t) -1,
                      "Content-Type: application/json\r\n");
    urlFree(up);
    return status;
}

/*
    The documented configuration, with no "validate" key anywhere.
 */
static void testEnableAloneValidates(void)
{
    WebHost *host;

    if ((host = startHost(DEFAULTED, NULL, 0)) == 0) {
        return;
    }
    teqi(post(DEFAULTED, "described", BAD_BODY), 400,
         "Enabling signatures must reject a body the signature refuses");
    teqi(post(DEFAULTED, "described", GOOD_BODY), 200, "A valid body must still be accepted");

    //  An endpoint the signature file does not describe is not an error outside strict mode
    teqi(post(DEFAULTED, "undescribed", GOOD_BODY), 200,
         "An endpoint with no signature must not be refused by default");
    stopTestHost(host);
}

/*
    The escape hatch must still exempt a route.
 */
static void testValidateFalseExempts(void)
{
    WebHost *host;

    if ((host = startHost(EXEMPTED, "false", 0)) == 0) {
        return;
    }
    teqi(post(EXEMPTED, "described", BAD_BODY), 200, "validate:false must exempt the route");
    stopTestHost(host);
}

/*
    Strict mode refuses a request that resolves to no signature, but only for a route that asked to
    be validated: a route validated by default has nothing to check when the file describes nothing.
 */
static void testStrictAppliesToOptedInRoutes(void)
{
    WebHost *host;

    if ((host = startHost(STRICT, "true", 1)) == 0) {
        return;
    }
    teqi(post(STRICT, "described", GOOD_BODY), 200, "A described endpoint must still be served");
    teqi(post(STRICT, "undescribed", GOOD_BODY), 400,
         "validate:true with strict must refuse an endpoint the file does not describe");
    stopTestHost(host);

    if ((host = startHost(STRICT, NULL, 1)) == 0) {
        return;
    }
    teqi(post(STRICT, "undescribed", GOOD_BODY), 200,
         "Strict must not refuse an undescribed endpoint on a route validated by default");
    teqi(post(STRICT, "described", BAD_BODY), 400, "Strict must still validate a described endpoint");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (writeSchema()) {
        testEnableAloneValidates();
        testValidateFalseExempts();
        testStrictAppliesToOptedInRoutes();
    } else {
        tfail("Cannot write the signature schema");
    }
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
