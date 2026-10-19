/*
    sig-field-names.tst.c - Client field names are matched literally against the signature

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define STRICT  "http://127.0.0.1:4889"
#define LENIENT "http://127.0.0.1:4890"
#define SCHEMA  "tmp/sig-field-names.json5"
#define JSONHDR "Content-Type: application/json\r\n"

/************************************ Code ************************************/

static bool writeSchema(void)
{
    cchar *text;

    text = "{ form: { item: { request: { type: 'object', fields: {"
           " label: { type: 'string' }, count: { type: 'number' } },"
           " query: { type: 'object', fields: { label: { type: 'string' } } } } } } }\n";
    return rWriteFile(SCHEMA, text, slen(text), 0644) >= 0;
}

static int fetch(cchar *method, cchar *url, cchar *body)
{
    Url *up;
    int status;

    up = urlAlloc(0);
    status = urlFetch(up, method, url, body, body ? (size_t) -1 : 0, body ? JSONHDR : NULL);
    urlFree(up);
    return status;
}

static WebHost *startHost(cchar *endpoint, bool strict)
{
    char config[512];

    SFMT(config, "{ web: { documents: './site', listen: ['%s'], "
         "signatures: { enable: true, path: '" SCHEMA "', strict: %s }, "
         "routes: [ { match: '/test/sig/', handler: 'action', validate: true, role: 'public' }, {} ] } }",
         endpoint, strict ? "true" : "false");
    return startTestHost(config);
}

static void testStrictNames(void)
{
    WebHost *host;
    int     status;

    if ((host = startHost(STRICT, 1)) == 0) {
        return;
    }
    status = fetch("POST", STRICT "/test/sig/form/item", "{\"label\":\"bob\",\"count\":1}");
    teqi(status, 200, "Declared fields are accepted");
    status = fetch("POST", STRICT "/test/sig/form/item", "{\"label\":\"bob\",\"label.type\":\"x\"}");
    teqi(status, 400, "A dotted name is not a path into the signature");
    status = fetch("POST", STRICT "/test/sig/form/item", "{\".label\":\"x\"}");
    teqi(status, 400, "A leading-dot name is undeclared");
    status = fetch("POST", STRICT "/test/sig/form/item", "{\" label\":\"x\"}");
    teqi(status, 400, "A leading-space body name is undeclared");
    status = fetch("POST", STRICT "/test/sig/form/item", "{\"label\":\"a\",\"count.type\":\"number\"}");
    teqi(status, 400, "A name naming a signature attribute is undeclared");
    status = fetch("GET", STRICT "/test/sig/form/item?label=x", NULL);
    teqi(status, 200, "A declared query name is accepted");
    status = fetch("GET", STRICT "/test/sig/form/item?%20label=x", NULL);
    teqi(status, 400, "A leading-space query name is not the declared name");
    stopTestHost(host);
}

static void testUndecodedName(void)
{
    WebHost *host;
    int     status;

    if ((host = startHost(LENIENT, 0)) == 0) {
        return;
    }
    status = fetch("GET", LENIENT "/test/sig/form/item?a%2Eb", NULL);
    teqi(status, 400, "A name without a value is decoded before the structural check");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (writeSchema()) {
        testStrictNames();
        testUndecodedName();
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
    This is proprietary software and requires a commercial license from the author.
 */
