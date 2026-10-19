/*
    upload-part-state.tst.c - Each multipart part is parsed on its own

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

#define BOUNDARY "----PartStateBoundary"

/************************************ Code ************************************/


static int post(cchar *contentType, cchar *body, Json **json)
{
    Url  *up;
    char url[128], headers[256];
    int  status;

    up = urlAlloc(0);
    SFMT(headers, "Content-Type: %s\r\n", contentType);
    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", HTTP), body, slen(body), headers);
    *json = status == 200 ? jsonParse(urlGetResponse(up), 0) : NULL;
    urlFree(up);
    return status;
}

static void testNamelessPartDoesNotInherit(void)
{
    Json *json;
    int  status;

    status = post("multipart/form-data; boundary=" BOUNDARY,
                  "--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"first\"\r\n\r\none\r\n"
                  "--" BOUNDARY "\r\nContent-Disposition: form-data\r\n\r\ntwo\r\n"
                  "--" BOUNDARY "--\r\n", &json);
    teqi(status, 400, "A part without a name must not inherit the previous part's name");
    jsonFree(json);
}

static void testContentTypeStaysWithItsPart(void)
{
    Json *json;
    int  status;

    status = post("multipart/form-data; boundary=" BOUNDARY,
                  "--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"part-state.txt\"\r\n"
                  "Content-Type: text/plain\r\n\r\nfile data\r\n"
                  "--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"note\"\r\n"
                  "Content-Type: application/x-evil\r\n\r\nfield data\r\n"
                  "--" BOUNDARY "--\r\n", &json);
    teqi(status, 200);
    if (json) {
        tmatch(jsonGet(json, 0, "uploads[0].contentType", 0), "text/plain");
        teqi(jsonGetInt(json, 0, "uploads[0].size", 0), 9, "The field's data must not be written to the file");
        tmatch(jsonGet(json, 0, "form.note", 0), "field data");
    }
    jsonFree(json);
    unlink("tmp/part-state.txt");
}

static void testBareLineBreakRefused(void)
{
    Json *json;
    int  status;

    status = post("multipart/form-data; boundary=" BOUNDARY,
                  "--" BOUNDARY "\r\nX-Note: a\nContent-Disposition: form-data; name=\"file\"; filename=\"b.cgi\"\r\n"
                  "\r\ndata\r\n--" BOUNDARY "--\r\n", &json);
    teqi(status, 400, "A bare LF must not start a new part header");
    jsonFree(json);
}

static void testBoundaryParameter(void)
{
    Json *json;
    int  status;

    //  Quoted, and followed by another parameter
    status = post("multipart/form-data; boundary=\"" BOUNDARY "\"; charset=utf-8",
                  "--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"x\"\r\n\r\nquoted\r\n"
                  "--" BOUNDARY "--\r\n", &json);
    teqi(status, 200, "A quoted boundary followed by another parameter is accepted");
    if (json) {
        tmatch(jsonGet(json, 0, "form.x", 0), "quoted");
    }
    jsonFree(json);

    //  The quotes are not part of the delimiter
    status = post("multipart/form-data; boundary=\"B0\"",
                  "--\"B0\"\r\nContent-Disposition: form-data; name=\"x\"\r\n\r\nliteral\r\n--\"B0\"--\r\n", &json);
    tneqi(status, 200, "A delimiter carrying the boundary quotes is not a delimiter");
    jsonFree(json);
}

static void testEmptyFieldAccepted(void)
{
    Json *json;
    int  status;

    status = post("multipart/form-data; boundary=" BOUNDARY,
                  "--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"empty\"\r\n\r\n\r\n"
                  "--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"after\"\r\n\r\nvalue\r\n"
                  "--" BOUNDARY "--\r\n", &json);
    teqi(status, 200, "An empty form field is accepted");
    if (json) {
        tmatch(jsonGet(json, 0, "form.after", 0), "value");
    }
    jsonFree(json);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testNamelessPartDoesNotInherit();
        testContentTypeStaysWithItsPart();
        testBareLineBreakRefused();
        testBoundaryParameter();
        testEmptyFieldAccepted();
    }
    rFree(HTTP);
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
