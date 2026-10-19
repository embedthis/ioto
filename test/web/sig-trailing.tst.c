/*
    sig-trailing.tst.c - Trailing path segments do not deselect the endpoint's signature

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define SCHEMA   "tmp/sig-trailing.json5"
#define ENDPOINT "http://127.0.0.1:4896"
#define JSONHDR  "Content-Type: application/json\r\n"

/************************************ Code ************************************/

static bool writeSchema(void)
{
    cchar *text;

    text = "{ api: { item: { request: { type: 'object', hasRequired: true, fields: {"
           " email: { type: 'string', required: true }, role: { type: 'string', role: 'admin' } } } } } }\n";
    return rWriteFile(SCHEMA, text, slen(text), 0644) >= 0;
}

static int post(cchar *path, cchar *body, char *response, size_t size)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, "POST", SFMT(url, "%s%s", ENDPOINT, path), body, (size_t) -1, JSONHDR);
    if (response) {
        scopy(response, size, urlGetResponse(up));
    }
    urlFree(up);
    return status;
}

static void fiberMain(void *arg)
{
    WebHost *host;
    char    response[1024];
    int     status;

    if (!writeSchema()) {
        tfail("Cannot write the signature schema");

    } else if ((host = startTestHost("{ web: { documents: './site', listen: ['" ENDPOINT "'], "
                                     "signatures: { enable: true, path: '" SCHEMA "' }, "
                                     "routes: [ { match: '/test/sig/', handler: 'action', validate: true }, {} ] } }"))
               != 0) {
        status = post("/test/sig/api/item", "{}", NULL, 0);
        teqi(status, 400, "A missing required field is refused");
        status = post("/test/sig/api/item/x", "{}", NULL, 0);
        teqi(status, 400, "A trailing segment must not deselect the signature of the endpoint that runs");
        post("/test/sig/api/item/x", "{\"email\":\"a\",\"role\":\"admin\"}", response, sizeof(response));
        tfalse(scontains(response, "admin"), "A role-gated field must be filtered under a trailing segment");
        stopTestHost(host);
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
