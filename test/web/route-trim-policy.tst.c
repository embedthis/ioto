/*
    route-trim-policy.tst.c - A trim rewrite into another route's namespace is held to that route's policy

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4897"
#define SCHEMA   "tmp/trim-policy.json5"
#define RODIR    "site/trimro"
#define KEEP     "site/trimro/keep.txt"
#define CREATED  "site/trimro/created.txt"

#define CONFIG                                                                              \
    "{web: {documents: './site', listen: ['http://127.0.0.1:4897'],"                       \
    " signatures: {enable: true, path: '" SCHEMA "', strict: true},"                        \
    " routes: ["                                                                            \
    "  {match: '/pubf/', trim: '/pubf', handler: 'file', methods: ['GET', 'PUT', 'DELETE']}," \
    "  {match: '/pub/', trim: '/pub', handler: 'action', methods: ['GET', 'POST']},"        \
    "  {match: '/test/sig/', handler: 'action', validate: true, methods: ['GET', 'POST']}," \
    "  {match: '/test/xsrf/', handler: 'action', xsrf: true, methods: ['GET', 'POST']},"    \
    "  {match: '/test/', handler: 'action'},"                                               \
    "  {match: '/trimro/', handler: 'file', methods: ['GET']},"                             \
    "  {}"                                                                                  \
    " ]}}"

/************************************ Code ************************************/

static int fetch(cchar *method, cchar *path, cchar *body, cchar *headers, char *response, size_t size)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, method, SFMT(url, "%s%s", ENDPOINT, path), body, body ? (size_t) -1 : 0, headers);
    if (response) {
        scopy(response, size, urlGetResponse(up));
    }
    urlFree(up);
    return status;
}

static bool writeSchema(void)
{
    cchar *text;

    text = "{ api: { item: { request: { type: 'object', hasRequired: true, fields: {"
           " email: { type: 'string', required: true }, role: { type: 'string', role: 'admin' } } } } } }\n";
    return rWriteFile(SCHEMA, text, slen(text), 0644) >= 0;
}

/*
    The rewritten namespace accepts GET only. Its method list must hold through the rewrite.
 */
static void testTargetMethodsApply(void)
{
    int status;

    mkdir(RODIR, 0755);
    ttrue(rWriteFile(KEEP, "keep\n", 5, 0644) > 0, "Cannot create the keep file");

    status = fetch("PUT", "/trimro/created.txt", "new", NULL, NULL, 0);
    teqi(status, 405, "PUT is not a method of /trimro/");
    status = fetch("PUT", "/pubf/trimro/created.txt", "new", NULL, NULL, 0);
    teqi(status, 405,
         "A trim rewrite must not grant PUT in a namespace whose route refuses it");
    ttrue(!rFileExists(CREATED), "The rewritten PUT must not create the file");

    status = fetch("DELETE", "/pubf/trimro/keep.txt", NULL, NULL, NULL, 0);
    teqi(status, 405,
         "A trim rewrite must not grant DELETE in a namespace whose route refuses it");
    ttrue(rFileExists(KEEP), "The rewritten DELETE must not remove the file");

    unlink(CREATED);
    unlink(KEEP);
    rmdir(RODIR);
}

/*
    The rewritten namespace requires an XSRF token. The rewrite must not drop that requirement.
 */
static void testTargetXsrfApplies(void)
{
    int status;

    status = fetch("POST", "/test/xsrf/x", "a=1", "Content-Type: application/x-www-form-urlencoded\r\n", NULL, 0);
    teqi(status, 400, "A POST without a token is refused on the XSRF route");
    status = fetch("POST", "/pub/test/xsrf/x", "a=1", "Content-Type: application/x-www-form-urlencoded\r\n", NULL, 0);
    teqi(status, 400, "A trim rewrite must not skip the XSRF check of the namespace it reaches");
}


static void testTargetSignatureApplies(void)
{
    char response[1024];
    int  status;

    status = fetch("POST", "/test/sig/api/item", "{}", "Content-Type: application/json\r\n", NULL, 0);
    teqi(status, 400,
         "A missing required field is refused on the validated route");
    status = fetch("POST", "/pub/test/sig/api/item", "{}", "Content-Type: application/json\r\n", NULL, 0);
    teqi(status, 400,
         "A trim rewrite must not skip request validation");

    fetch("POST", "/pub/test/sig/api/item", "{\"email\":\"a\",\"role\":\"admin\"}",
          "Content-Type: application/json\r\n", response, sizeof(response));
    tfalse(scontains(response, "admin"), "A role-gated field must be filtered through a trim rewrite");
}

static void fiberMain(void *arg)
{
    WebHost *host;

    if (!writeSchema()) {
        tfail("Cannot write the signature schema");
    } else if ((host = startTestHost(CONFIG)) != 0) {
        testTargetMethodsApply();
        testTargetXsrfApplies();
        testTargetSignatureApplies();
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
