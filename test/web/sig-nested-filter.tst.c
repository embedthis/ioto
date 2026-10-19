/*
    sig-nested-filter.tst.c - Fields after a filtered nested object or array item are still validated

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4883"
#define URL      ENDPOINT "/test/sig/form/obj"
#define SCHEMA   "tmp/sig-nested-filter.json5"
#define JSONHDR  "Content-Type: application/json\r\n"

/************************************ Code ************************************/

static bool writeSchema(void)
{
    cchar *text;

    text = "{ form: { obj: { request: { type: 'object', fields: {"
           " name: { type: 'string' }, secret: { type: 'string', role: 'admin' },"
           " address: { type: 'object', fields: { street: { type: 'string' }, level: { type: 'string', role: 'admin' } } },"
           " items: { type: 'array', of: { type: 'object', fields: { name: { type: 'string' } } } } } } } } }\n";
    return rWriteFile(SCHEMA, text, slen(text), 0644) >= 0;
}

static int post(cchar *body, char *response, size_t size)
{
    Url *up;
    int status;

    up = urlAlloc(0);
    status = urlFetch(up, "POST", URL, body, (size_t) -1, JSONHDR);
    if (response) {
        scopy(response, size, urlGetResponse(up));
    }
    urlFree(up);
    return status;
}

static void testNestedFilter(void)
{
    WebHost *host;
    char    response[1024];
    int     status;

    host = startTestHost("{ web: { documents: './site', listen: ['" ENDPOINT "'], "
                         "signatures: { enable: true, strict: true, path: '" SCHEMA "' }, "
                         "routes: [ { match: '/test/sig/', handler: 'action', validate: true, role: 'public' }, {} ] } }");
    if (!host) {
        return;
    }
    //  Baselines without a nested filter
    status = post("{\"name\":\"a\",\"secret\":\"top\"}", response, sizeof(response));
    teqi(status, 200, "A body with a role-gated field is accepted");
    tfalse(scontains(response, "secret"), "A role-gated field is removed");
    status = post("{\"name\":\"a\",\"bogus\":1}", NULL, 0);
    teqi(status, 400, "Strict mode refuses an undeclared field");

    //  The same checks after a nested object that had a field removed
    status = post("{\"address\":{\"street\":\"a\",\"level\":\"x\"},\"secret\":\"top\"}", response, sizeof(response));
    teqi(status, 200, "A body with a filtered nested object is accepted");
    tfalse(scontains(response, "level"), "A role-gated nested field is removed");
    tfalse(scontains(response, "secret"), "A role-gated field after a filtered nested object is removed");
    status = post("{\"address\":{\"street\":\"a\",\"_x\":1},\"bogus\":1}", NULL, 0);
    teqi(status, 400, "Strict mode refuses an undeclared field after a filtered nested object");

    //  Later array items after an item that had a field removed
    status = post("{\"items\":[{\"name\":\"a\",\"_x\":1},{\"bogus\":1}]}", NULL, 0);
    teqi(status, 400, "Strict mode refuses an undeclared field in a later array item");

    stopTestHost(host);
}

/*
    An array or scalar where the signature declares an object is refused, not dereferenced
 */
static void testNonObject(void)
{
    WebHost *host;
    cchar   *bodies[] = { "{\"address\":\"x\"}", "\"x\"", "{\"address\":[1]}", "{\"items\":[[1]]}", "[1]" };
    int     i, status;

    host = startTestHost("{ web: { documents: './site', listen: ['" ENDPOINT "'], "
                         "signatures: { enable: true, strict: true, path: '" SCHEMA "' }, "
                         "routes: [ { match: '/test/sig/', handler: 'action', validate: true, role: 'public' }, {} ] } }");
    if (!host) {
        return;
    }
    for (i = 0; i < (int) (sizeof(bodies) / sizeof(bodies[0])); i++) {
        status = post(bodies[i], NULL, 0);
        teqi(status, 400, "A non-object where an object is declared is refused: %s", bodies[i]);
    }
    status = post("{\"name\":\"a\"}", NULL, 0);
    teqi(status, 200, "The server keeps serving");
    stopTestHost(host);
}


static void testRepeatedName(void)
{
    WebHost *host;
    char    response[1024];
    int     status;

    host = startTestHost("{ web: { documents: './site', listen: ['" ENDPOINT "'], "
                         "signatures: { enable: true, strict: true, path: '" SCHEMA "' }, "
                         "routes: [ { match: '/test/sig/', handler: 'action', validate: true, role: 'public' }, {} ] } }");
    if (!host) {
        return;
    }
    status = post("{\"address\":{\"street\":\"a\"},\"address\":{\"bogus\":1}}", NULL, 0);
    teqi(status, 400, "Strict mode refuses an undeclared field in a repeated object");
    status = post("{\"items\":[{\"name\":\"a\"}],\"items\":[{\"bogus\":1}]}", NULL, 0);
    teqi(status, 400, "Strict mode refuses an undeclared field in a repeated array");
    status = post("{\"address\":{\"street\":\"a\"},\"address\":{\"street\":\"b\",\"level\":\"top\"}}",
                  response, sizeof(response));
    teqi(status, 200, "A repeated object is accepted");
    tfalse(scontains(response, "level"), "A role-gated field in a repeated object is removed");
    stopTestHost(host);
}


static void testLinearFilter(void)
{
    WebHost *host;
    RBuf    *buf;
    Ticks   elapsed, start;
    int     i, status;

    host = startTestHost("{ web: { documents: './site', listen: ['" ENDPOINT "'], "
                         "signatures: { enable: true, strict: true, path: '" SCHEMA "' }, "
                         "routes: [ { match: '/test/sig/', handler: 'action', validate: true, role: 'public' }, {} ] } }");
    if (!host) {
        return;
    }
    //  Each pair is 17 bytes, so 5900 pairs fit the 100K body limit
    buf = rAllocBuf(5900 * 17 + 16);
    rPutCharToBuf(buf, '{');
    for (i = 0; i < 5900; i++) {
        rPutToBuf(buf, "%s\"name\":\"a\",\"_\":1", i ? "," : "");
    }
    rPutCharToBuf(buf, '}');
    start = rGetTicks();
    status = post(rBufToString(buf), NULL, 0);
    elapsed = rGetTicks() - start;
    teqi(status, 200, "A filtered body is accepted");
    ttrue(elapsed < 2000, "Validating a 100K body took %lld msec", (long long) elapsed);
    rFreeBuf(buf);
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (writeSchema()) {
        testNestedFilter();
        testLinearFilter();
        testNonObject();
        testRepeatedName();
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
