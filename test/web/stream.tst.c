

/*
    stream.c.tst - Unit tests for streaming

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/
/*
    Stream the request by writing progressively in the client
 */
static void streamRequest()
{
    Url   *up;
    Json  *json;
    char  buf[ME_BUFSIZE], url[128];
    ssize nbytes, total;
    int   i, rc;

    up = urlAlloc(0);   // URL_SHOW_REQ_HEADERS | URL_SHOW_RESP_BODY | URL_SHOW_RESP_HEADERS);

    //  This request is streamed here, but buffered in the server
    rc = urlStart(up, "POST", SFMT(url, "%s/test/show", HTTP));
    ttrue(rc == 0);

    rc = urlWriteHeaders(up, NULL);
    ttrue(rc == 0);

    memset(buf, 'a', sizeof(buf));

    for (i = 0, total = 0; i < 10; i++) {
        nbytes = urlWrite(up, buf, sizeof(buf));
        ttrue(nbytes > 0);
        if (nbytes < 0) {
            break;
        }
        total += nbytes;
    }
    urlFinalize(up);
    ttrue(urlGetStatus(up) == 200);

    json = urlGetJsonResponse(up);
    ttrue(jsonGetInt(json, 0, "bodyLength", 0) == total);
    jsonFree(json);

    urlFree(up);
}

/*
    Stream the request by reading progressively in the server
 */
static void streamAtServer()
{
    Url   *up;
    Json  *json;
    char  buf[ME_BUFSIZE], url[128];
    ssize nbytes, total;
    int   i, rc;

    up = urlAlloc(0);

    //  The server will stream the receipt
    rc = urlStart(up, "POST", SFMT(url, "%s/stream/test/stream", HTTP));
    ttrue(rc == 0);

    rc = urlWriteHeaders(up, NULL);
    ttrue(rc == 0);

    memset(buf, 'a', sizeof(buf));
    for (i = 0, total = 0; i < 10; i++) {
        nbytes = urlWrite(up, buf, sizeof(buf));
        ttrue(nbytes > 0);
        if (nbytes < 0) {
            break;
        }
        total += nbytes;
    }
    urlFinalize(up);
    ttrue(urlGetStatus(up) == 200);

    json = urlGetJsonResponse(up);
    ttrue(jsonGetInt(json, 0, "length", 0) == total);
    jsonFree(json);

    urlFree(up);
}

/*
    Stream the response by reading progressively in the client
 */
static void streamResponse()
{
    Url   *up;
    char  buf[ME_BUFSIZE], url[128];
    ssize count, nbytes;
    int   status;

    up = urlAlloc(0);

    //  Stream response
    status = urlFetch(up, "POST", SFMT(url, "%s/test/bulk?count=1000", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    count = 0;
    do {
        nbytes = urlRead(up, buf, sizeof(buf));
        count += nbytes;
    } while (nbytes > 0);
    ttrue(count == 2300);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        streamRequest();
        streamAtServer();
        streamResponse();
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
