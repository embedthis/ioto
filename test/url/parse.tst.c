/*
    parse.c.tst - Unit tests for Parsing

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

static void parseBasic()
{
    Url *up;
    int rc;

    up = urlAlloc(0);

    // NULL url should fail
    rc = urlParse(up, NULL);
    ttrue(rc < 0);

    // Empty url uses defaults
    rc = urlParse(up, "");
    ttrue(rc == 0);
    tmatch(up->scheme, "http");
    tmatch(up->host, "localhost");
    teqi(up->port, 80);
    tmatch(up->path, "");
    ttrue(up->query == 0);
    ttrue(up->hash == 0);

    // Full URL with all components
    rc = urlParse(up, "http://www.example.com:1234/index.html?query=true#frag");
    ttrue(rc == 0);
    tmatch(up->scheme, "http");
    tmatch(up->host, "www.example.com");
    teqi(up->port, 1234);
    tmatch(up->path, "index.html");
    tmatch(up->query, "query=true");
    tmatch(up->hash, "frag");

    // URL without port
    rc = urlParse(up, "http://example.com/path");
    ttrue(rc == 0);
    tmatch(up->host, "example.com");
    teqi(up->port, 80);
    tmatch(up->path, "path");

    // URL without path
    rc = urlParse(up, "http://example.com");
    ttrue(rc == 0);
    tmatch(up->host, "example.com");
    tmatch(up->path, "");

    // Minimal scheme
    rc = urlParse(up, "http://");
    ttrue(rc == 0);
    tmatch(up->scheme, "http");
    tmatch(up->host, "localhost");
    teqi(up->port, 80);

    urlFree(up);
}

static void parseSchemes()
{
    Url *up;
    int rc;

    up = urlAlloc(0);

    // HTTPS defaults to port 443
    rc = urlParse(up, "https://secure.example.com/api");
    ttrue(rc == 0);
    tmatch(up->scheme, "https");
    tmatch(up->host, "secure.example.com");
    teqi(up->port, 443);
    tmatch(up->path, "api");

    // HTTPS with explicit port
    rc = urlParse(up, "https://secure.example.com:8443/api");
    ttrue(rc == 0);
    teqi(up->port, 8443);

    // WSS (WebSocket Secure) defaults to port 443
    rc = urlParse(up, "wss://ws.example.com/socket");
    ttrue(rc == 0);
    tmatch(up->scheme, "wss");
    teqi(up->port, 443);
    tmatch(up->path, "socket");

    // WS (WebSocket) defaults to port 80
    rc = urlParse(up, "ws://ws.example.com/socket");
    ttrue(rc == 0);
    tmatch(up->scheme, "ws");
    teqi(up->port, 80);

    urlFree(up);
}

static void parseIPv6()
{
    Url *up;
    int rc;

    up = urlAlloc(0);

    // IPv6 loopback with port and path
    rc = urlParse(up, "http://[::1]:8080/path");
    ttrue(rc == 0);
    tmatch(up->host, "::1");
    teqi(up->port, 8080);
    tmatch(up->path, "path");

    // IPv6 without port
    rc = urlParse(up, "http://[::1]/path");
    ttrue(rc == 0);
    tmatch(up->host, "::1");
    teqi(up->port, 80);

    // Full IPv6 address
    rc = urlParse(up, "http://[2001:db8:85a3::8a2e:370:7334]:9000/api");
    ttrue(rc == 0);
    tmatch(up->host, "2001:db8:85a3::8a2e:370:7334");
    teqi(up->port, 9000);

    // IPv6 with query and hash
    rc = urlParse(up, "https://[::1]:443/path?key=value#section");
    ttrue(rc == 0);
    tmatch(up->host, "::1");
    teqi(up->port, 443);
    tmatch(up->query, "key=value");
    tmatch(up->hash, "section");

    urlFree(up);
}

static void parsePorts()
{
    Url *up;
    int rc;

    up = urlAlloc(0);

    // Port without explicit host (uses localhost)
    rc = urlParse(up, ":8080/path");
    ttrue(rc == 0);
    tmatch(up->host, "localhost");
    teqi(up->port, 8080);
    tmatch(up->path, "path");

    // Minimum valid port
    rc = urlParse(up, "http://example.com:1/path");
    ttrue(rc == 0);
    teqi(up->port, 1);

    // Maximum valid port
    rc = urlParse(up, "http://example.com:65535/path");
    ttrue(rc == 0);
    teqi(up->port, 65535);

    // Invalid port: zero
    rc = urlParse(up, "http://example.com:0/path");
    ttrue(rc < 0);

    // Invalid port: too large
    rc = urlParse(up, "http://example.com:65536/path");
    ttrue(rc < 0);

    // Invalid port: negative (strtol interprets as negative)
    rc = urlParse(up, "http://example.com:-1/path");
    ttrue(rc < 0);

    // Invalid port: non-numeric
    rc = urlParse(up, "http://example.com:abc/path");
    ttrue(rc < 0);

    // Invalid port: mixed
    rc = urlParse(up, "http://example.com:80abc/path");
    ttrue(rc < 0);

    urlFree(up);
}

static void parseQueryHash()
{
    Url *up;
    int rc;

    up = urlAlloc(0);

    // Query only
    rc = urlParse(up, "http://example.com/path?query=value");
    ttrue(rc == 0);
    tmatch(up->query, "query=value");
    ttrue(up->hash == 0);

    // Hash only
    rc = urlParse(up, "http://example.com/path#section");
    ttrue(rc == 0);
    ttrue(up->query == 0);
    tmatch(up->hash, "section");

    // Empty query
    rc = urlParse(up, "http://example.com/path?");
    ttrue(rc == 0);
    tmatch(up->query, "");

    // Empty hash
    rc = urlParse(up, "http://example.com/path#");
    ttrue(rc == 0);
    tmatch(up->hash, "");

    // Query with multiple params
    rc = urlParse(up, "http://example.com/api?a=1&b=2&c=3");
    ttrue(rc == 0);
    tmatch(up->query, "a=1&b=2&c=3");

    // Hash in query value (hash takes precedence)
    rc = urlParse(up, "http://example.com/path?q=test#frag");
    ttrue(rc == 0);
    tmatch(up->query, "q=test");
    tmatch(up->hash, "frag");

    urlFree(up);
}

static void parseInvalid()
{
    Url *up;
    int rc;
    char longHost[300];

    up = urlAlloc(0);

    // Hostname too long (> 255 chars)
    memset(longHost, 'a', 256);
    longHost[256] = '\0';
    char *longUrl = sfmt("http://%s/path", longHost);
    rc = urlParse(up, longUrl);
    ttrue(rc < 0);
    rFree(longUrl);

    // Hostname at limit (255 chars) should succeed
    memset(longHost, 'a', 255);
    longHost[255] = '\0';
    longUrl = sfmt("http://%s/path", longHost);
    rc = urlParse(up, longUrl);
    ttrue(rc == 0);
    rFree(longUrl);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        parseBasic();
        parseSchemes();
        parseIPv6();
        parsePorts();
        parseQueryHash();
        parseInvalid();
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
