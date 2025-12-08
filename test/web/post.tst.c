

/*
    post.c.tst - Unit tests for post requests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void post()
{
    Url    *up;
    Json   *json;
    cchar  *response;
    char   *body, url[128];
    size_t len;
    int    status;

    // up = urlAlloc(URL_SHOW_REQ_HEADERS | URL_SHOW_RESP_BODY | URL_SHOW_RESP_HEADERS);
    up = urlAlloc(0);

    //  Post to a static file
    len = 80 * 1024;
    body = rAlloc(len);
    memset(body, 'a', len);
    status = urlFetch(up, "POST", SFMT(url, "%s/index.html", HTTP), body, len,
                      "Content-Type: text/plain\r\n");
    teqi(status, 200);
    response = urlGetResponse(up);
    tcontains(response, "<title>index.html</title>");

    //  Post to a form
    json = urlJson(up, "POST", SFMT(url, "%s/test/show", HTTP), "hello world", 11,
                   "Content-Type: text/plain\r\n");
    tnotnull(json);
    tcontains(jsonGet(json, 0, "body", 0), "hello world");

    rFree(body);
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        post();
    }
    rFree(HTTP);
    rFree(HTTPS);
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
