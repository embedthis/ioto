/*
    sig-drop.tst.c - The signature's field filter applies to requests, not only to responses

    validateObject implemented "drop this field" as omission from the output buffer. On the response
    path that buffer is the response, so the omission is a real filter. On the request path there is
    no buffer - the handler reads web->vars and web->qvars directly - so every drop was a no-op and a
    field the signature gated behind a role reached the handler exactly as the client sent it. That
    is the documented mass-assignment control: role, isAdmin, owner and the like.

    The schema is written here and served from an in-process host on port 4865, which this test uses
    alone. The client is anonymous for the drop cases, so webCan("admin") is false.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define SCHEMA   "tmp/sig-drop.json5"
#define ENDPOINT "http://127.0.0.1:4865"

/************************************ Code ************************************/

static bool writeSchema(void)
{
    cchar *text;

    text = "{ controller: {"
           "  byRole: { request: { type: 'object', fields: {"
           "      name: { type: 'string' }, role: { type: 'string', role: 'admin' } } },"
           "    response: { type: 'object', hasWild: true } },"
           "  byDrop: { request: { type: 'object', fields: {"
           "      name: { type: 'string' }, secret: { type: 'string', drop: true } } },"
           "    response: { type: 'object', hasWild: true } },"
           "  byDropRole: { request: { type: 'object', fields: {"
           "      name: { type: 'string' }, owner: { type: 'string', drop: 'admin' } } },"
           "    response: { type: 'object', hasWild: true } },"
           "  byDropTag: { request: { type: 'object', fields: {"
           "      name: { type: 'string' }, tagged: { type: 'string', drop: { request: 'admin' } } } },"
           "    response: { type: 'object', hasWild: true } },"
           "  nested: { request: { type: 'object', fields: {"
           "      inner: { type: 'object', fields: {"
           "        name: { type: 'string' }, role: { type: 'string', role: 'admin' } } } } },"
           "    response: { type: 'object', hasWild: true } },"
           "  listed: { request: { type: 'object', fields: {"
           "      items: { type: 'array', of: { type: 'object', fields: {"
           "        name: { type: 'string' }, role: { type: 'string', role: 'admin' } } } } } },"
           "    response: { type: 'object', hasWild: true } },"
           "  queried: { request: { query: { type: 'object', fields: {"
           "      name: { type: 'string' }, role: { type: 'string', role: 'admin' } } } },"
           "    response: { type: 'object', hasWild: true } }"
           "} }\n";
    return rWriteFile(SCHEMA, text, slen(text), 0644) >= 0;
}

static WebHost *startSigHost(void)
{
    return startTestHost("{ web: { documents: './site', listen: ['" ENDPOINT "'], "
                         "auth: { roles: { public: [], user: ['view'], admin: ['user'] } }, "
                         "signatures: { enable: true, path: '" SCHEMA "', strict: false }, "
                         "routes: [ { match: '/test/sig/', handler: 'action', validate: true, role: 'public' }, "
                         "{ match: '/test/', handler: 'action' }, {} ] } }");
}

/*
    POST a body to a signature method and copy the echoed response out. The echo is re-serialized
    from web->vars, so it shows exactly what the handler was given.
 */
static int post(cchar *method, cchar *body, cchar *cookie, char *response, size_t size)
{
    Url   *up;
    cchar *text;
    char  url[256];
    int   status;

    up = urlAlloc(0);
    SFMT(url, "%s/test/sig/controller/%s", ENDPOINT, method);
    if (cookie) {
        status = urlFetch(up, "POST", url, body, (size_t) -1,
                          "Content-Type: application/json\r\nCookie: %s\r\n", cookie);
    } else {
        status = urlFetch(up, "POST", url, body, (size_t) -1, "Content-Type: application/json\r\n");
    }
    text = urlGetResponse(up);
    scopy(response, size, text ? text : "");
    urlFree(up);
    return status;
}

/*
    Every way the signature can say "the caller may not supply this field" must remove it from the
    request the handler sees.
 */
static void testRequestFieldsAreDropped(void)
{
    WebHost *host;
    char    response[2048];
    int     status;

    if ((host = startSigHost()) == 0) {
        return;
    }
    status = post("byRole", "{\"name\":\"bob\",\"role\":\"admin\"}", NULL, response, sizeof(response));
    teqi(status, 200, "A role-gated field must not be an error");
    tcontains(response, "bob");
    ttrue(!scontains(response, "role"), "A role-gated request field must not reach the handler: %s", response);

    status = post("byDrop", "{\"name\":\"bob\",\"secret\":\"s\"}", NULL, response, sizeof(response));
    teqi(status, 200, "drop:true must not be an error");
    ttrue(!scontains(response, "secret"), "A drop:true request field must not reach the handler: %s", response);

    status = post("byDropRole", "{\"name\":\"bob\",\"owner\":\"root\"}", NULL, response, sizeof(response));
    teqi(status, 200, "drop:'admin' must not be an error");
    ttrue(!scontains(response, "owner"), "A drop:'admin' request field must not reach the handler: %s", response);

    status = post("byDropTag", "{\"name\":\"bob\",\"tagged\":\"t\"}", NULL, response, sizeof(response));
    teqi(status, 200, "drop:{request:...} must not be an error");
    ttrue(!scontains(response, "tagged"), "A drop:{request} field must not reach the handler: %s", response);
    stopTestHost(host);
}

/*
    validateObject and validateArray recurse with the same buffer, so the filter has to hold at
    every depth, not only at the top level.
 */
static void testNestedFieldsAreDropped(void)
{
    WebHost *host;
    char    response[2048];
    int     status;

    if ((host = startSigHost()) == 0) {
        return;
    }
    status = post("nested", "{\"inner\":{\"name\":\"bob\",\"role\":\"admin\"}}", NULL, response, sizeof(response));
    teqi(status, 200, "A nested role-gated field must not be an error");
    tcontains(response, "bob");
    ttrue(!scontains(response, "role"), "A role-gated field inside an object must be dropped: %s", response);

    status = post("listed", "{\"items\":[{\"name\":\"bob\",\"role\":\"admin\"}]}", NULL, response, sizeof(response));
    teqi(status, 200, "A role-gated field in an array must not be an error");
    tcontains(response, "bob");
    ttrue(!scontains(response, "role"), "A role-gated field inside an array must be dropped: %s", response);
    stopTestHost(host);
}

/*
    The query direction runs through the same validateObject call with no buffer.
 */
static void testQueryFieldsAreDropped(void)
{
    WebHost *host;
    Url     *up;
    cchar   *text;
    char    url[256], response[2048];
    int     status;

    if ((host = startSigHost()) == 0) {
        return;
    }
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/sig/controller/queried?name=bob&role=admin", ENDPOINT),
                      NULL, 0, NULL);
    teqi(status, 200, "A role-gated query parameter must not be an error");
    text = urlGetResponse(up);
    scopy(response, sizeof(response), text ? text : "");
    tcontains(response, "bob");
    ttrue(!scontains(response, "role"), "A role-gated query parameter must be dropped: %s", response);
    urlFree(up);
    stopTestHost(host);
}

/*
    The filter is authorization, not censorship: a caller who holds the role keeps the field.
 */
static void testAdminKeepsTheField(void)
{
    WebHost *host;
    Url     *up;
    cchar   *header;
    char    url[256], cookie[512], response[2048], *end;
    int     status;

    if ((host = startSigHost()) == 0) {
        return;
    }
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/session/login?user=alice&role=admin", ENDPOINT), NULL, 0, NULL);
    teqi(status, 200, "The login must succeed");
    if ((header = urlGetHeader(up, "Set-Cookie")) == 0) {
        tfail("The login must set a session cookie");
        urlFree(up);
        stopTestHost(host);
        return;
    }
    scopy(cookie, sizeof(cookie), header);
    if ((end = schr(cookie, ';')) != 0) {
        *end = '\0';
    }
    urlFree(up);

    status = post("byRole", "{\"name\":\"bob\",\"role\":\"admin\"}", cookie, response, sizeof(response));
    teqi(status, 200, "An admin request must succeed");
    tcontains(response, "role");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (writeSchema()) {
        testRequestFieldsAreDropped();
        testNestedFieldsAreDropped();
        testQueryFieldsAreDropped();
        testAdminKeepsTheField();
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
