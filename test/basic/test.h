/*
    test.h - Unit tests helpers

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "json.h"
#include    "url.h"
#include    "web.h"
#include    "testme.h"

/************************************ Code ************************************/

PUBLIC bool setup(char **http, char **https)
{
    Json    *json;
    Url     *up;
    char    url[128];
    int     status;

    rSetSocketDefaultCerts("../../certs/ca.crt", NULL, NULL, NULL);
    urlSetDefaultTimeout(60 * TPS);

    if (http || https) {
        json = jsonParseFile("state/config/web.json5", NULL, 0);
        if (http) {
            *http = sclone(jsonGet(json, 0, "listen[0]", "http://localhost:9090"));
        }
        if (https) {
            *https = sclone(jsonGet(json, 0, "listen[1]", "https://localhost:9091"));
        }
        jsonFree(json);
    }
    /*
        Get index.html to test that the server is running
     */
    up = urlAlloc(0);
    if ((status = urlFetch(up, "GET", SFMT(url, "%s/index.html", *https), NULL, 0, NULL)) != 200) {
        teq(200, status);
        urlFree(up);
        return 0;
    }
    urlFree(up);
    ttrue(1);
    return 1;
}
