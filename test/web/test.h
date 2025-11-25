/*
    test.h - Unit tests helpers

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "url.h"
#include    "web.h"
#include    "testme.h"

/************************************ Code ************************************/

PUBLIC bool setup(char **HTTP, char **HTTPS)
{
    Json *json;

    //  For debug tracing -- set the LOG_FILTER env to: stdout:raw,error,info,trace,debug:all,!mbedtls

    // Construct path to CA certificate
    rSetSocketDefaultCerts("../certs/ca.crt", 0, 0, 0);
    urlSetDefaultTimeout(60 * TPS);

    if (HTTP || HTTPS) {
        json = jsonParseFile("web.json5", NULL, 0);
        if (!json) {
            tfail("Cannot parse web.json5");
            return 0;
        }
        if (HTTP) {
            *HTTP = jsonGetClone(json, 0, "web.listen[0]", NULL);
            if (!*HTTP) {
                tfail("Cannot get HTTP");
                jsonFree(json);
                return 0;
            }
        }
        if (HTTPS) {
            *HTTPS = jsonGetClone(json, 0, "web.listen[1]", NULL);
            if (!*HTTPS) {
                tfail("Cannot get HTTPS");
                jsonFree(json);
                return 0;
            }
        }
        jsonFree(json);
    }
    return 1;
}
