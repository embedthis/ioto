/*
    post.c.tst - Unit tests for post requests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/*********************************** Locals ***********************************/

#define SITE        "https://www.embedthis.com"
#define POST_SITE   "https://api.admin.embedthis.com/api/test"

/************************************ Code ************************************/

static void postUrl()
{
    Json    *json;
    char    body[80], url[128];
    char    *response;

    sfmtbuf(body, sizeof(body), "Hello World");
    response = urlPost(SFMT(url, "%s/test/show", HTTP), body, 0, NULL);
    json = jsonParse(response, 0);
    tmatch(jsonGet(json, 0, "body", 0), "Hello World");
    jsonFree(json);
    rFree(response);
}

static void postJsonUrl()
{
    char    body[80], url[128];
    Json    *json;

    sfmtbuf(body, sizeof(body), "{\"message\":\"Hello Json\"}");
    json = urlPostJson(SFMT(url, "%s/test/show", HTTP), body, 0, "Content-Type: application/json\r\n");
    tmatch(jsonGet(json, 0, "form.message", 0), "Hello Json");
    jsonFree(json);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        postUrl();
        postJsonUrl();
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
