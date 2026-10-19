/*
    sig-query-body.tst.c - A query string does not replace validation of the request body

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define SCHEMA  "tmp/sig-query-body.json5"
#define JSONHDR "Content-Type: application/json\r\n"

/************************************ Code ************************************/

static bool writeSchema(void)
{
    cchar *text;

    text = "{ api: { item: { request: { type: 'object', hasRequired: true, fields: {"
           " email: { type: 'string', required: true }, role: { type: 'string', role: 'admin' } },"
           " query: { type: 'object', fields: { limit: { type: 'string' } } } } } } }\n";
    return rWriteFile(SCHEMA, text, slen(text), 0644) >= 0;
}

static int post(cchar *endpoint, cchar *path, cchar *body, char *response, size_t size)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, "POST", SFMT(url, "%s%s", endpoint, path), body, (size_t) -1, JSONHDR);
    if (response) {
        scopy(response, size, urlGetResponse(up));
    }
    urlFree(up);
    return status;
}

static void checkHost(cchar *endpoint, bool strict)
{
    WebHost *host;
    char    config[512], response[1024];
    int     status;

    SFMT(config, "{ web: { documents: './site', listen: ['%s'], "
         "signatures: { enable: true, path: '" SCHEMA "', strict: %s }, "
         "routes: [ { match: '/test/sig/', handler: 'action', validate: true }, {} ] } }",
         endpoint, strict ? "true" : "false");
    if ((host = startTestHost(config)) == 0) {
        return;
    }
    status = post(endpoint, "/test/sig/api/item", "{}", NULL, 0);
    teqi(status, 400, "A missing required field is refused");
    status = post(endpoint, "/test/sig/api/item?limit=1", "{}", NULL, 0);
    teqi(status, 400, "A valid query must not exempt the body from validation");
    status = post(endpoint, "/test/sig/api/item?limit=1", NULL, NULL, 0);
    teqi(status, 400, "A valid query must not exempt a missing body from validation");

    post(endpoint, "/test/sig/api/item", "{\"email\":\"a\",\"role\":\"admin\"}", response, sizeof(response));
    tfalse(scontains(response, "admin"), "A role-gated field is filtered");
    post(endpoint, "/test/sig/api/item?limit=1", "{\"email\":\"a\",\"role\":\"admin\"}", response, sizeof(response));
    tfalse(scontains(response, "admin"), "A role-gated field must be filtered when a query is present");

    status = post(endpoint, "/test/sig/api/item?limit=1", "{\"email\":\"a\"}", NULL, 0);
    teqi(status, 200, "A valid body with a valid query is accepted");
    status = post(endpoint, "/test/sig/api/item?limit=1&role=x", "{\"email\":\"a\"}", NULL, 0);
    teqi(status, strict ? 400 : 200, "An undeclared query parameter follows the strict setting");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (writeSchema()) {
        checkHost("http://127.0.0.1:4881", 0);
        checkHost("http://127.0.0.1:4882", 1);
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
