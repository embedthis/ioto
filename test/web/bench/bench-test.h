/*
    bench-test.h - Benchmark test helpers

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "r.h"
#include    "url.h"
#include    "web.h"
#include    "testme.h"

/*********************************** Locals ***********************************/

// Helper to setup HTTP and HTTPS endpoints from web.json5
PUBLIC bool benchSetup(char **HTTP, char **HTTPS)
{
    Json *json;

    rSetSocketDefaultCerts("../../certs/ca.crt", 0, 0, 0);
    urlSetDefaultTimeout(60 * TPS);

    if (HTTP || HTTPS) {
        json = jsonParseFile("web.json5", NULL, 0);
        if (!json) {
            printf("Cannot parse web.json5\n");
            return 0;
        }
        if (HTTP) {
            *HTTP = jsonGetClone(json, 0, "web.listen[0]", NULL);
            if (!*HTTP) {
                printf("Cannot get HTTP endpoint\n");
                jsonFree(json);
                return 0;
            }
        }
        if (HTTPS) {
            *HTTPS = jsonGetClone(json, 0, "web.listen[1]", NULL);
            if (!*HTTPS) {
                printf("Cannot get HTTPS endpoint\n");
                jsonFree(json);
                return 0;
            }
        }
        jsonFree(json);
    }
    return 1;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
