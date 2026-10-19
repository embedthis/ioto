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

/*
    In-process host helpers for tests that need a configuration the shared server does not have:
    a small limit, an endpoint form, client certificate verification. The host is driven by the
    url client from the same process; both run on fibers under the one event loop. Use a port in
    the 4850-4899 range so the shared server and other tests are untouched.
 */

/*
    Allocate and start a host from a JSON5 configuration string. The test actions are registered
    under /test as on the shared server. Returns the host, or NULL after reporting the failure.
 */
PUBLIC WebHost *startTestHost(cchar *config)
{
    WebHost *host;
    Json    *json;

    if ((json = jsonParse(config, 0)) == 0) {
        tfail("Cannot parse test host config");
        return 0;
    }
    if ((host = webAllocHost(json, 0)) == 0) {
        tfail("Cannot allocate test host");
        jsonFree(json);
        return 0;
    }
#if ME_WEB_TEST_ACTIONS
    webTestInit(host, "/test");
#endif
    if (webStartHost(host) < 0) {
        webFreeHost(host);
        jsonFree(json);
        return 0;
    }
    return host;
}

/*
    Stop and free a host started by startTestHost, including its configuration.
 */
PUBLIC void stopTestHost(WebHost *host)
{
    Json *json;

    if (!host) {
        return;
    }
    json = host->config;
    webStopHost(host);
    webFreeHost(host);
    jsonFree(json);
}

/*
    Raw-socket helpers for tests that exercise the wire framing itself, which a client library
    cannot express: malformed chunk sizes, bodies split across reads, pipelined requests.
 */

/*
    Connect to an "http://host:port" endpoint. Returns the socket or NULL.
 */
PUBLIC RSocket *rawConnect(cchar *endpoint)
{
    RSocket *sp;
    char    *cp, *portStr, buf[256];
    int     port;

    scopy(buf, sizeof(buf), endpoint);
    if ((cp = scontains(buf, "://")) != 0) {
        cp += 3;
    } else {
        cp = buf;
    }
    if ((portStr = schr(cp, ':')) == 0) {
        return 0;
    }
    *portStr++ = '\0';
    if ((port = (int) stoi(portStr)) <= 0) {
        return 0;
    }
    if ((sp = rAllocSocket()) == 0) {
        return 0;
    }
    if (rConnectSocket(sp, cp, port, rGetTicks() + 5 * TPS) < 0) {
        rFreeSocket(sp);
        return 0;
    }
    return sp;
}

/*
    Count the complete HTTP responses at the start of buf. A response is complete at its
    Content-Length or at the terminating zero chunk. A response with neither cannot be told
    complete before EOF and is not counted.
 */
PUBLIC int rawCountResponses(cchar *buf, ssize len)
{
    cchar *cp, *end, *headers, *bodyStart, *zero;
    ssize bodyLen;
    int   count;

    count = 0;
    cp = buf;
    end = &buf[len];
    while (cp < end && (headers = scontains(cp, "\r\n\r\n")) != 0) {
        bodyStart = headers + 4;
        if ((zero = sncaselesscontains(cp, "content-length:", (size_t) (headers - cp))) != 0) {
            bodyLen = stoi(&zero[slen("content-length:")]);
            if (&bodyStart[bodyLen] > end) {
                break;
            }
            cp = &bodyStart[bodyLen];
        } else if (sncaselesscontains(cp, "transfer-encoding: chunked", (size_t) (headers - cp)) != 0) {
            if ((zero = scontains(bodyStart, "\r\n0\r\n\r\n")) == 0) {
                //  The whole body is one zero chunk
                if (sncmp(bodyStart, "0\r\n\r\n", 5) != 0) {
                    break;
                }
                zero = bodyStart - 2;
            }
            cp = &zero[7];
        } else {
            break;
        }
        count++;
    }
    return count;
}

/*
    Read from the socket until "want" complete responses have arrived, the socket closes or it
    goes quiet. Returns the number of bytes read. The buffer is null terminated.
 */
PUBLIC ssize rawRead(RSocket *sp, char *buf, size_t size, int want)
{
    ssize nbytes, total;

    total = 0;
    buf[0] = '\0';
    while (total < (ssize) size - 1) {
        nbytes = rReadSocket(sp, &buf[total], size - 1 - (size_t) total, rGetTicks() + 3 * TPS);
        if (nbytes <= 0) {
            break;
        }
        total += nbytes;
        buf[total] = '\0';
        if (rawCountResponses(buf, total) >= want) {
            break;
        }
    }
    return total;
}

/*
    Send a request as a sequence of segments, pausing between them so the server sees each in a
    separate read, then read "want" complete responses into buf. Returns the status of the first
    response, 0 if the server never answered, or -1 if it could not be reached.
 */
PUBLIC int rawRequest(cchar *endpoint, cchar **segments, int count, Ticks pause, int want, char *buf, size_t size)
{
    RSocket *sp;
    ssize   total;
    int     i;

    if ((sp = rawConnect(endpoint)) == 0) {
        return -1;
    }
    for (i = 0; i < count; i++) {
        if (rWriteSocket(sp, segments[i], slen(segments[i]), rGetTicks() + 5 * TPS) < 0) {
            rFreeSocket(sp);
            return -1;
        }
        if (pause && i < count - 1) {
            rSleep(pause);
        }
    }
    total = rawRead(sp, buf, size, want);
    rFreeSocket(sp);
    if (total <= 0) {
        return 0;
    }
    return (total > 12) ? (int) stoi(&buf[9]) : -1;
}
