/*
    sig-refusals.tst.c - API signature validation refusals

    The signature tests cover typed fields, roles and drops. These cover the refusals that guard
    the validator itself: a body nested past the recursion limit, an undeclared query parameter in
    strict mode, names the validator always hides, and the non-strict mode that admits an unknown
    method and silently drops an undeclared field. The schema is generated here and served from
    in-process hosts so the shared server's signatures are untouched.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define SCHEMA  "tmp/sig-refusals.json5"
#define STRICT  "http://localhost:4859"
#define LENIENT "http://localhost:4860"
#define DEPTH   (WEB_MAX_SIG_DEPTH + 2)

/************************************ Code ************************************/

/*
    Write the schema: a request object nested DEPTH levels for the depth bomb, a query-only
    method, and a plain method with two declared fields echoed in the response.
 */
static bool writeSchema(void)
{
    RBuf *buf;
    int  i, rc;

    buf = rAllocBuf(0);
    rPutStringToBuf(buf, "{ controller: { deep: { request: ");
    for (i = 0; i < DEPTH; i++) {
        rPutStringToBuf(buf, "{ type: 'object', fields: { child: ");
    }
    rPutStringToBuf(buf, "{ type: 'string' }");
    for (i = 0; i < DEPTH; i++) {
        rPutStringToBuf(buf, " } }");
    }
    rPutStringToBuf(buf, ", response: { type: 'object' } }, "
                    "q: { request: { query: { type: 'object', fields: { name: { type: 'string' } } } }, "
                    "response: { type: 'object', fields: { name: { type: 'string' } } } }, "
                    "plain: { request: { type: 'object', fields: { name: { type: 'string' }, size: { type: 'number' } } }, "
                    "response: { type: 'object', fields: { name: { type: 'string' }, size: { type: 'number' } } } }, "
                    "resonly: { response: { type: 'object' } } } }\n");
    rc = rWriteFile(SCHEMA, rGetBufStart(buf), rGetBufLength(buf), 0644);
    rFreeBuf(buf);
    return rc >= 0;
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
    POST a JSON body (or GET when body is NULL) to a method and return the status; the response
    body is copied out for the caller.
 */
static int invoke(cchar *base, cchar *method, cchar *body, char *response, size_t size)
{
    Url   *up;
    cchar *text;
    char  url[256];
    int   status;

    up = urlAlloc(0);
    if (body) {
        status = urlFetch(up, "POST", SFMT(url, "%s/test/sig/controller/%s", base, method), body, (size_t) -1,
                          "Content-Type: application/json\r\n");
    } else {
        status = urlFetch(up, "GET", SFMT(url, "%s/test/sig/controller/%s", base, method), NULL, 0, NULL);
    }
    text = urlGetResponse(up);
    scopy(response, size, text ? text : "");
    urlFree(up);
    return status;
}

/*
    Build {child:{child:...}} nested n deep. The leaf is the string the schema declares at the
    bottom, or an empty object, which the schema admits at any level since child is not required.
 */
static char *nested(int n, cchar *leaf)
{
    RBuf *buf;
    char *result;
    int  i;

    buf = rAllocBuf(0);
    for (i = 0; i < n; i++) {
        rPutStringToBuf(buf, "{\"child\":");
    }
    rPutStringToBuf(buf, leaf);
    for (i = 0; i < n; i++) {
        rPutCharToBuf(buf, '}');
    }
    result = sclone(rGetBufStart(buf));
    rFreeBuf(buf);
    return result;
}

static void testStrictRefusals(WebHost *host)
{
    char *body, response[1024];
    int  status;

    //  A body nested past the recursion limit is refused before the validator recurses into it
    body = nested(DEPTH, "\"x\"");
    status = invoke(STRICT, "deep", body, response, sizeof(response));
    teqi(status, 400, "A body nested past the depth limit must be refused");
    rFree(body);

    body = nested(3, "{}");
    status = invoke(STRICT, "deep", body, response, sizeof(response));
    teqi(status, 200, "A body within the depth limit is accepted");
    rFree(body);

    //  Query parameters are validated against the request.query signature
    status = invoke(STRICT, "q?name=x", NULL, response, sizeof(response));
    teqi(status, 200, "A declared query parameter is accepted");
    status = invoke(STRICT, "q?name=x&bogus=1", NULL, response, sizeof(response));
    teqi(status, 400, "An undeclared query parameter is refused in strict mode");

    //  An undeclared body field is refused in strict mode
    status = invoke(STRICT, "plain", "{\"name\":\"x\",\"size\":1,\"extra\":true}", response, sizeof(response));
    teqi(status, 400, "An undeclared body field is refused in strict mode");

    //  Names the validator always hides are dropped from the response without error
    status = invoke(STRICT, "plain", "{\"name\":\"x\",\"size\":1,\"_secret\":\"s\",\"pk\":\"p\",\"sk\":\"k\"}",
                    response, sizeof(response));
    teqi(status, 200, "Hidden names are tolerated");
    tcontains(response, "\"name\":\"x\"");
    ttrue(scontains(response, "_secret") == NULL, "A field starting with _ must never be echoed");
    ttrue(scontains(response, "\"pk\"") == NULL && scontains(response, "\"sk\"") == NULL, "pk and sk must never be echoed");

    //  A method with no signature at all is refused in strict mode
    status = invoke(STRICT, "unknown", "{\"name\":\"x\"}", response, sizeof(response));
    teqi(status, 400, "A method without a signature is refused in strict mode");

    //  A method that declares only a response has no request signature: refused in strict mode
    status = invoke(STRICT, "resonly", "{\"name\":\"x\"}", response, sizeof(response));
    teqi(status, 400, "A method with no request signature is refused in strict mode");
}

static void testLenientMode(WebHost *host)
{
    char response[1024];
    int  status;

    status = invoke(LENIENT, "unknown", "{\"name\":\"x\"}", response, sizeof(response));
    teqi(status, 200, "A method without a signature is admitted in non-strict mode");

    status = invoke(LENIENT, "resonly", "{\"name\":\"x\"}", response, sizeof(response));
    teqi(status, 200, "A method with no request signature is admitted in non-strict mode");

    //  An undeclared field is dropped rather than refused, and the declared ones survive
    status = invoke(LENIENT, "plain", "{\"name\":\"x\",\"size\":1,\"extra\":true}", response, sizeof(response));
    teqi(status, 200, "An undeclared body field is dropped in non-strict mode");
    tcontains(response, "\"name\":\"x\"");
    ttrue(scontains(response, "extra") == NULL, "The undeclared field must not be echoed");

    //  The depth limit holds regardless of mode
    status = invoke(LENIENT, "deep", "{\"child\":{\"child\":{\"child\":{\"child\":{\"child\":{\"child\":{\"child\":{\"child\":"
                    "{\"child\":{\"child\":{\"child\":{\"child\":{\"child\":{\"child\":{\"child\":{\"child\":{\"child\":{\"child\":\"x\"}}}}}}}}}}}}}}}}}}",
                    response, sizeof(response));
    teqi(status, 400, "The depth limit holds in non-strict mode");
}

static void fiberMain(void *arg)
{
    WebHost *strict, *lenient;

    if (setup(NULL, NULL) && writeSchema()) {
        strict = startSigHost("http://:4859", 1);
        lenient = startSigHost("http://:4860", 0);
        if (strict && lenient) {
            testStrictRefusals(strict);
            testLenientMode(lenient);
        }
        stopTestHost(strict);
        stopTestHost(lenient);
    }
    unlink(SCHEMA);
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
