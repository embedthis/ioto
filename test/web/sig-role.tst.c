/*
    sig-role.tst.c - A signature's method role and its controller's _meta.role gate the request

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4895"
#define SCHEMA   "tmp/sig-role.json5"
#define JSONHDR  "Content-Type: application/json\r\n"

/************************************ Code ************************************/

static int post(cchar *path, cchar *body)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, "POST", SFMT(url, "%s%s", ENDPOINT, path), body, (size_t) -1, JSONHDR);
    urlFree(up);
    return status;
}

static void testSignatureRoleIsEnforced(void)
{
    WebHost *host;
    cchar   *text;
    int     status;

    text = "{ api: { _meta: { role: 'admin' }, item: { request: { type: 'object', fields: {"
           " email: { type: 'string' } } } },"
           " plan: { role: 'admin', request: { type: 'object', fields: { email: { type: 'string' } } } } },"
           " open: { item: { role: 'public', request: { type: 'object' } } } }\n";
    if (rWriteFile(SCHEMA, text, slen(text), 0644) < 0) {
        tfail("Cannot write the signature schema");
        return;
    }
    host = startTestHost("{web: {documents: './site', listen: ['" ENDPOINT "'],"
                         " auth: {roles: {public: [], user: ['view'], admin: ['user']}},"
                         " signatures: {enable: true, path: '" SCHEMA "'},"
                         " routes: [ {match: '/test/sig/', handler: 'action', validate: true}, {} ]}}");
    if (!host) {
        return;
    }
    status = post("/test/sig/api/plan", "{\"email\":\"a\"}");
    teqi(status, 401, "A method-level signature role gates the request");
    status = post("/test/sig/api/item", "{\"email\":\"a\"}");
    teqi(status, 401, "A controller _meta.role gates its methods");
    status = post("/test/sig/open/item", "{}");
    teqi(status, 200, "A public signature is open");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    testSignatureRoleIsEnforced();
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
