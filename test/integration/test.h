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

PUBLIC bool setup(char **HTTP, char **HTTPS)
{
    Json    *json;

    rSetSocketDefaultCerts("../../certs/ca.crt", NULL, NULL, NULL);
    urlSetDefaultTimeout(60 * TPS);

    if (HTTP || HTTPS) {
        json = jsonParseFile("state/config/web.json5", NULL, 0);
        if (HTTP) {
            *HTTP = jsonGetClone(json, 0, "listen[0]", NULL);
            if (!*HTTP) {
                tfail("Cannot get HTTP URL");
                return 0;
            }
        }
        if (HTTPS) {
            *HTTPS = jsonGetClone(json, 0, "listen[1]", NULL);
            if (!*HTTPS) {
                tfail("Cannot get HTTPS URL");
                return 0;
            }
        }
        jsonFree(json);
    }
    ttrue(1);
    return 1;
}
