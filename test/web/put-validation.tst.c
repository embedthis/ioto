/*
    diff-put-validation.tst.c - A PUT body is validated and bounded like any other request body

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4893"
#define SIGS     "tmp/put-validation.json5"

/************************************ Code ************************************/

static int sendRequest(cchar *method, cchar *body)
{
    cchar *segments[1];
    char  *req, buf[8192];
    int   status;

    req = sfmt("%s /test/put HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\n"
               "Content-Length: %d\r\nConnection: close\r\n\r\n%s", method, (int) slen(body), body);
    segments[0] = req;
    status = rawRequest(ENDPOINT, segments, 1, 0, 1, buf, sizeof(buf));
    rFree(req);
    return status;
}

static void testPutBodyValidated(void)
{
    WebHost *host;
    char    *big;
    int     status;
    cchar   *sigs = "{ put: { request: { type: 'object', fields: { name: { type: 'string' } } } } }";

    rWriteFile(SIGS, sigs, slen(sigs), 0644);
    host = startTestHost("{ web: { documents: './site/', listen: ['" ENDPOINT "'], "
                         "limits: { body: '1k', upload: '10mb' }, "
                         "signatures: { enable: true, path: '" SIGS "', strict: true }, "
                         "routes: [ { match: '/test/', handler: 'action', validate: true, methods: ['POST', 'PUT'] }, {} ] } }");
    if (!host) {
        unlink(SIGS);
        return;
    }
    big = rAlloc(4096);
    memset(big, 'a', 4095);
    big[4095] = '\0';

    status = sendRequest("POST", "{\"name\":\"a\"}");
    teqi(status, 200, "A described POST body is accepted");
    status = sendRequest("POST", "{\"role\":\"admin\"}");
    teqi(status, 400, "POST: an undeclared field is refused in strict mode");
    status = sendRequest("POST", "not json");
    teqi(status, 400, "POST: a malformed body is refused");
    status = sendRequest("POST", big);
    teqi(status, 413, "POST: the body limit applies");

    status = sendRequest("PUT", "{\"name\":\"a\"}");
    teqi(status, 200, "A described PUT body is accepted");
    status = sendRequest("PUT", "{\"role\":\"admin\"}");
    teqi(status, 400, "PUT: an undeclared field must be refused like POST");
    status = sendRequest("PUT", "not json");
    teqi(status, 400, "PUT: a malformed body must be refused like POST");
    status = sendRequest("PUT", big);
    teqi(status, 413, "PUT to a validated action: the body limit must apply like POST");

    rFree(big);
    stopTestHost(host);
    unlink(SIGS);
}

static void fiberMain(void *arg)
{
    testPutBodyValidated();
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
