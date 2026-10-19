/*
    sig-canon.tst.c - The signature key comes from the same path routing used

    The signature key was sliced off web->url, which parseUrl decodes but does not normalize, while
    routing, the role gate and both handlers use the normalized web->path. A ".." segment therefore
    selected a different signature - or none, which fails open unless web.signatures.strict is set,
    and it is not the default - for an endpoint that ran normally.

    The schema is written here and served from in-process hosts so the shared server's signatures
    are untouched. Ports 4861 and 4862 are used by this test alone.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define SCHEMA  "tmp/sig-canon.json5"
#define STRICT  "http://127.0.0.1:4861"
#define LENIENT "http://127.0.0.1:4862"

//  A body the signature refuses: "email" is required
#define MISSING "{\"email-not\":\"x\",\"name\":\"x\"}"
//  A body the signature accepts
#define VALID   "{\"email\":\"a@b.c\",\"name\":\"x\"}"

/************************************ Code ************************************/

static bool writeSchema(void)
{
    cchar *text;

    text = "{ controller: { test_2: { "
           "request: { type: 'object', hasRequired: true, fields: { "
           "  email: { type: 'string', required: true }, name: { type: 'string' } } }, "
           "response: { type: 'object' } } } }\n";
    return rWriteFile(SCHEMA, text, slen(text), 0644) >= 0;
}

static WebHost *startSigHost(cchar *listen, bool strict)
{
    char config[512];

    SFMT(config, "{ web: { documents: './site', listen: ['%s'], "
         "signatures: { enable: true, path: '" SCHEMA "', strict: %s }, "
         "routes: [ { match: '/test/sig/', handler: 'action', validate: true, role: 'public' }, {} ] } }",
         listen, strict ? "true" : "false");
    return startTestHost(config);
}

/*
    POST body to a raw, un-normalized request target. The url client would normalize the target
    itself, so the request line is built by hand.
 */
static int post(cchar *endpoint, cchar *target, cchar *body)
{
    cchar *segments[1];
    char  *req, buf[8192];
    int   status;

    req = sfmt("POST %s HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\n"
               "Content-Length: %d\r\nConnection: close\r\n\r\n%s", target, (int) slen(body), body);
    segments[0] = req;
    status = rawRequest(endpoint, segments, 1, 0, 1, buf, sizeof(buf));
    rFree(req);
    return status;
}

/*
    The default configuration. A dot-segment spelling must select the same signature as the
    canonical one, so a body the signature refuses is refused either way.
 */
static void testLenientKeysOffTheRoutedPath(void)
{
    WebHost *host;

    if ((host = startSigHost("http://127.0.0.1:4862", 0)) == 0) {
        return;
    }
    teqi(post(LENIENT, "/test/sig/controller/test_2", MISSING), 400,
         "The canonical spelling is validated");
    teqi(post(LENIENT, "/test/sig/controller/test_2/x/..", MISSING), 400,
         "A dot-segment spelling must select the same signature");
    teqi(post(LENIENT, "/test/sig/controller/test_2", VALID), 200,
         "A valid body is accepted");
    teqi(post(LENIENT, "/test/sig/controller/test_2/x/..", VALID), 200,
         "A valid body is accepted through a dot-segment spelling");
    stopTestHost(host);
}

/*
    A trailing ".." leaves web->path at /test/sig/controller, which has no signature of its own.
    The un-normalized key resolved to controller.test_2 and applied that endpoint's rules to a
    different endpoint; the routed path has none, so strict mode must refuse.
 */
static void testTrailingDotDotSelectsNoSignature(void)
{
    WebHost *host;

    if ((host = startSigHost("http://127.0.0.1:4861", 1)) == 0) {
        return;
    }
    teqi(post(STRICT, "/test/sig/controller/test_2", VALID), 200,
         "The canonical spelling is validated and accepted");
    teqi(post(STRICT, "/test/sig/controller/test_2/..", VALID), 400,
         "A trailing '..' must not be validated against controller.test_2");

    //  Empty segments are absorbed by normalization and must keep behaving as the canonical form
    teqi(post(STRICT, "/test/sig/controller/./test_2", VALID), 200,
         "A '.' segment must behave as the canonical form");
    teqi(post(STRICT, "/test/sig/controller//test_2", VALID), 200,
         "A duplicate separator must behave as the canonical form");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (writeSchema()) {
        testLenientKeysOffTheRoutedPath();
        testTrailingDotDotSelectsNoSignature();
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
