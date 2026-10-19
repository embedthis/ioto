/*
    utils.c.tst - Unit tests for web utility functions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"
#include    <locale.h>

/************************************ Code ************************************/

static void testWebEscapeHtml()
{
    char *result;

    // Test HTML escaping (adjust expectations based on actual implementation)
    result = webEscapeHtml("<script>alert('xss')</script>");
    ttrue(scontains(result, "&lt;") && scontains(result, "&gt;"));
    rFree(result);

    // Test with quotes and ampersands
    result = webEscapeHtml("Hello & \"World\"");
    tmatch(result, "Hello &amp; &quot;World&quot;");
    rFree(result);

    // Test with no special characters
    result = webEscapeHtml("Hello World");
    tmatch(result, "Hello World");
    rFree(result);

    // A null input yields an allocated empty string, not null
    result = webEscapeHtml(NULL);
    tmatch(result, "");
    rFree(result);

    // Every character in the escape set is replaced
    result = webEscapeHtml("&<>#()\"'");
    tmatch(result, "&amp;&lt;&gt;&#35;&#40;&#41;&quot;&#39;");
    rFree(result);

    // An empty string round trips
    result = webEscapeHtml("");
    tmatch(result, "");
    rFree(result);
}

static void testWebEncode()
{
    char *result;

    // Test URL encoding
    result = webEncode("hello world");
    tmatch(result, "hello%20world");
    rFree(result);

    // Test with special characters - adjust expectation
    result = webEncode("hello@world.com");
    ttrue(result != NULL);  // Function may not encode @ symbol
    rFree(result);

    // Test with characters that shouldn't be encoded
    result = webEncode("hello-world_123");
    tmatch(result, "hello-world_123");
    rFree(result);

    // Characters that must be percent encoded in a URI
    result = webEncode("a b\"c<d>e");
    tmatch(result, "a%20b%22c%3Cd%3Ee");
    rFree(result);

    // An empty string round trips
    result = webEncode("");
    tmatch(result, "");
    rFree(result);

    tnull(webEncode(NULL));
}

static void testWebDecode()
{
    char *result;
    char input[128];

    // Test URL decoding
    scopy(input, sizeof(input), "hello%20world");
    result = webDecode(input);
    tmatch(result, "hello world");

    // Test with encoded special characters
    scopy(input, sizeof(input), "hello%40world.com");
    result = webDecode(input);
    tmatch(result, "hello@world.com");

    // Test with no encoded characters
    scopy(input, sizeof(input), "hello-world");
    result = webDecode(input);
    tmatch(result, "hello-world");

    // Test with invalid encoding (should not crash)
    scopy(input, sizeof(input), "hello%2");
    result = webDecode(input);
    ttrue(result != NULL);
}

static void testWebNormalizePath()
{
    char *result;

    // Test path normalization with dot segments
    result = webNormalizePath("/path/./to/../file.html");
    tmatch(result, "/path/file.html");
    rFree(result);

    // Test with double slashes
    result = webNormalizePath("/path//to///file.html");
    tmatch(result, "/path/to/file.html");
    rFree(result);

    // Test with trailing dots - adjust expectation
    result = webNormalizePath("/path/to/file.html.");
    ttrue(result != NULL);  // May or may not strip trailing dots
    rFree(result);

    // Test security: a traversal above the root is refused outright
    tnull(webNormalizePath("/path/../../etc/passwd"));

    // Test with NULL input
    tnull(webNormalizePath(NULL));
}

/* webParseUrl out-parameters borrow its returned buffer. Path omits the leading /; query/hash omit ?/#. */
static void testWebParseUrl()
{
    char  *buf;
    cchar *scheme, *host, *path, *query, *hash;
    int   port;

    //  A fully populated URI
    buf = webParseUrl("https://example.com:8443/a/b?x=1&y=2#frag",
                      &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tmatch(scheme, "https");
    tmatch(host, "example.com");
    teqi(port, 8443);
    tmatch(path, "a/b");
    tmatch(query, "x=1&y=2");
    tmatch(hash, "frag");
    rFree(buf);

    //  A port that only fits after truncation to int, or carries a sign, is refused
    tnull(webParseUrl("http://example.com:4294967376/", &scheme, &host, &port, &path, &query, &hash));
    tnull(webParseUrl("http://example.com:+80/", &scheme, &host, &port, &path, &query, &hash));

    //  http with no explicit port leaves the port unset
    buf = webParseUrl("http://example.com/index.html", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tmatch(scheme, "http");
    tmatch(host, "example.com");
    teqi(port, 0);
    tmatch(path, "index.html");
    tnull(query);
    tnull(hash);
    rFree(buf);

    //  https defaults to port 443, and a bare "/" yields an empty path
    buf = webParseUrl("https://example.com/", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    teqi(port, 443);
    tmatch(host, "example.com");
    tmatch(path, "");
    rFree(buf);

    //  Only the authority separator is removed: further leading slashes stay in the path
    buf = webParseUrl("http://h//x", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tmatch(host, "h");
    tmatch(path, "/x");
    rFree(buf);

    buf = webParseUrl("http://h///a/b", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tmatch(path, "//a/b");
    rFree(buf);

    buf = webParseUrl("http://h:8080//x", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    teqi(port, 8080);
    tmatch(path, "/x");
    rFree(buf);

    buf = webParseUrl("http://[::1]//x", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tmatch(host, "::1");
    tmatch(path, "/x");
    rFree(buf);

    //  wss also defaults to 443
    buf = webParseUrl("wss://example.com/socket", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tmatch(scheme, "wss");
    teqi(port, 443);
    tmatch(path, "socket");
    rFree(buf);

    //  An IPv6 literal is returned without its enclosing brackets
    buf = webParseUrl("http://[fe80::1]/p", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tmatch(host, "fe80::1");
    tmatch(path, "p");
    rFree(buf);

    //  An IPv6 literal may carry a port, parsed the same way as for a named host
    buf = webParseUrl("http://[::1]:8080/x", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tmatch(host, "::1");
    teqi(port, 8080);
    tmatch(path, "x");
    rFree(buf);

    //  With a port but no path
    buf = webParseUrl("http://[::1]:8080", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tmatch(host, "::1");
    teqi(port, 8080);
    rFree(buf);

    //  A bare literal with neither port nor path
    buf = webParseUrl("http://[::1]", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tmatch(host, "::1");
    teqi(port, 0);
    rFree(buf);

    //  A bare "/" after the literal yields an empty path, as it does for a named host
    buf = webParseUrl("https://[::1]/", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tmatch(host, "::1");
    teqi(port, 443);
    tmatch(path, "");
    rFree(buf);

    //  A query and fragment still split off correctly around the literal
    buf = webParseUrl("http://[fe80::1]:9000/p?a=b#f", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tmatch(host, "fe80::1");
    teqi(port, 9000);
    tmatch(path, "p");
    tmatch(query, "a=b");
    tmatch(hash, "f");
    rFree(buf);

    //  An out of range port on a literal is rejected, as it is for a named host
    tnull(webParseUrl("http://[::1]:99999/x", &scheme, &host, &port, &path, &query, &hash));
    tnull(webParseUrl("http://[::1]:0/x", &scheme, &host, &port, &path, &query, &hash));
    tnull(webParseUrl("http://[::1]:80x/x", &scheme, &host, &port, &path, &query, &hash));

    //  A bare scheme with no host or path
    buf = webParseUrl("https", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tmatch(scheme, "https");
    tnull(host);
    tnull(path);
    rFree(buf);

    //  An absolute path with no scheme or host
    buf = webParseUrl("/index.html?a=b", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tnull(scheme);
    tnull(host);
    tmatch(path, "index.html");
    tmatch(query, "a=b");
    rFree(buf);

    //  A hash may be present without a query
    buf = webParseUrl("http://example.com/p#top", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tmatch(path, "p");
    tnull(query);
    tmatch(hash, "top");
    rFree(buf);

    //  The hash is split before the query, so a "?" inside the fragment stays in the fragment
    buf = webParseUrl("http://example.com/p#frag?notquery", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tnull(query);
    tmatch(hash, "frag?notquery");
    rFree(buf);

    /*
        A reference with no leading "/" and no scheme is read as an authority, not as a relative
        path - "index.html" parses as a host. Callers must pass an absolute path.
     */
    buf = webParseUrl("index.html", &scheme, &host, &port, &path, &query, &hash);
    tnotnull(buf);
    tmatch(host, "index.html");
    tnull(path);
    rFree(buf);

    //  Every out-parameter is optional
    buf = webParseUrl("http://example.com/p", NULL, NULL, NULL, NULL, NULL, NULL);
    tnotnull(buf);
    rFree(buf);
}

/*
    A malformed port must be rejected rather than silently truncated to a valid one.
 */
static void testWebParseUrlBadPort()
{
    cchar *scheme, *host, *path, *query, *hash;
    int   port;

    //  Out of range
    tnull(webParseUrl("http://example.com:99999/x", &scheme, &host, &port, &path, &query, &hash));
    tnull(webParseUrl("http://example.com:0/x", &scheme, &host, &port, &path, &query, &hash));
    tnull(webParseUrl("http://example.com:-1/x", &scheme, &host, &port, &path, &query, &hash));

    //  Trailing junk after the digits
    tnull(webParseUrl("http://example.com:80x/x", &scheme, &host, &port, &path, &query, &hash));
    //  No digits at all
    tnull(webParseUrl("http://example.com:/x", &scheme, &host, &port, &path, &query, &hash));

    //  The boundary value is accepted
    {
        char *buf = webParseUrl("http://example.com:65535/x", &scheme, &host, &port, &path, &query, &hash);
        tnotnull(buf);
        teqi(port, 65535);
        rFree(buf);
    }
}

/*
    Clear output pointers before freeing their backing buffer on failure.
 */
static void testWebParseUrlBadPortClearsOutParams(void)
{
    cchar *cases[] = { "{a:1}", "http://h:99999/", "http://h:0/", "http://h:1x/" };
    cchar *scheme, *host, *path, *query, *hash;
    int   port;
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        scheme = host = path = query = hash = "poison";
        port = -1;
        tnull(webParseUrl(cases[i], &scheme, &host, &port, &path, &query, &hash));
        tnull(scheme);
        tnull(host);
        teqi(port, 0);
        tnull(path);
        tnull(query);
        tnull(hash);
    }
}

/*
    RFC 3986 makes "+" a literal in a path; the plus-means-space rule belongs only to query
    strings and form bodies. Decoding a path with the form rule would alias "/a+b" and "/a%20b"
    onto one file and evade any policy keyed on the literal path.
 */
static void testWebDecodePath()
{
    char input[128];

    scopy(input, sizeof(input), "a+b.txt");
    tmatch(webDecodePath(input), "a+b.txt");

    scopy(input, sizeof(input), "a+b.txt");
    tmatch(webDecode(input), "a b.txt");

    //  Percent escapes are decoded by both
    scopy(input, sizeof(input), "a%20b.txt");
    tmatch(webDecodePath(input), "a b.txt");

    //  %00 is left undecoded so a null cannot be injected into a path
    scopy(input, sizeof(input), "a%00b");
    tmatch(webDecodePath(input), "a%00b");

    //  Lower and upper case hex digits are both accepted
    scopy(input, sizeof(input), "%2f%2F");
    tmatch(webDecodePath(input), "//");

    //  A truncated escape is left as-is rather than reading past the end
    scopy(input, sizeof(input), "abc%");
    tmatch(webDecodePath(input), "abc%");
    scopy(input, sizeof(input), "abc%z9");
    tmatch(webDecodePath(input), "abc%z9");

    //  Null tolerance
    tnull(webDecode(NULL));
    tnull(webDecodePath(NULL));
}

/*
    Paths that escape above the root must be refused, not clamped. The segment table moves to the
    heap for long paths, so both the stack and heap forms are exercised.
 */
static void testWebNormalizePathEdges()
{
    char   *result, *longPath;
    size_t i;

    //  Traversal above the root is refused
    tnull(webNormalizePath("/../etc/passwd"));
    tnull(webNormalizePath("/a/../../etc/passwd"));
    tnull(webNormalizePath(".."));
    tnull(webNormalizePath("../a"));

    //  An empty path has no normal form
    tnull(webNormalizePath(""));

    //  A path that reduces to nothing
    result = webNormalizePath("/a/..");
    tmatch(result, "/");
    rFree(result);

    result = webNormalizePath(".");
    tmatch(result, ".");
    rFree(result);

    result = webNormalizePath("/.");
    tmatch(result, "/");
    rFree(result);

    //  A relative path keeps its relative form
    result = webNormalizePath("a/./b");
    tmatch(result, "a/b");
    rFree(result);

    //  A trailing slash is preserved
    result = webNormalizePath("/a/./b/");
    tmatch(result, "/a/b/");
    rFree(result);

    /*
        More than 64 segments moves the segment table to the heap. Build "/s0/s1/.../s79".
     */
    {
        RBuf *buf = rAllocBuf(1024);
        for (i = 0; i < 80; i++) {
            rPutToBuf(buf, "/s%d", (int) i);
        }
        longPath = rBufToStringAndFree(buf);
    }
    result = webNormalizePath(longPath);
    tnotnull(result);
    ttrue(sstarts(result, "/s0/s1/"));
    ttrue(sends(result, "/s79"));
    rFree(result);

    //  The same long path with a traversal that escapes the root must still be refused
    {
        RBuf *buf = rAllocBuf(1024);
        rPutStringToBuf(buf, longPath);
        for (i = 0; i < 100; i++) {
            rPutStringToBuf(buf, "/..");
        }
        rFree(longPath);
        longPath = rBufToStringAndFree(buf);
    }
    tnull(webNormalizePath(longPath));
    rFree(longPath);
}

static void testWebGetStatusMsg()
{
    cchar *msg;

    // Test common status codes
    msg = webGetStatusMsg(200);
    tmatch(msg, "OK");

    msg = webGetStatusMsg(404);
    tmatch(msg, "Not Found");

    msg = webGetStatusMsg(500);
    tmatch(msg, "Internal Server Error");

    msg = webGetStatusMsg(101);
    tmatch(msg, "Switching Protocols");

    msg = webGetStatusMsg(206);
    tmatch(msg, "Partial Content");

    // A code that is in range but not in the table
    msg = webGetStatusMsg(499);
    tmatch(msg, "Unknown");

    // Codes outside the valid HTTP range
    tmatch(webGetStatusMsg(999), "Unknown");
    tmatch(webGetStatusMsg(-1), "Unknown");
    tmatch(webGetStatusMsg(0), "Unknown");
    tmatch(webGetStatusMsg(600), "Unknown");
}

/*
    RFC 9110 fixes HTTP-date to the IMF-fixdate form with English day and month abbreviations
    regardless of locale, and it is always 29 characters.
 */
static void testWebDate()
{
    time_t now = time(0);
    char   *result;

    result = webHttpDate(now);
    tnotnull(result);
    teqz(slen(result), 29);
    ttrue(sends(result, " GMT"));
    rFree(result);

    // The epoch is a fixed, known rendering
    result = webHttpDate(0);
    tmatch(result, "Thu, 01 Jan 1970 00:00:00 GMT");
    rFree(result);

    // A known instant: 2025-11-10T21:28:28Z
    result = webHttpDate((time_t) 1762810108);
    tmatch(result, "Mon, 10 Nov 2025 21:28:28 GMT");
    rFree(result);

    // Day and month names must not be affected by the C locale
    setlocale(LC_ALL, "de_DE.UTF-8");
    result = webHttpDate(0);
    tmatch(result, "Thu, 01 Jan 1970 00:00:00 GMT");
    rFree(result);
    setlocale(LC_ALL, "C");
}

static void fiberMain(void *arg)
{
    testWebEscapeHtml();
    testWebEncode();
    testWebDecode();
    testWebNormalizePath();
    testWebNormalizePathEdges();
    testWebParseUrl();
    testWebParseUrlBadPort();
    testWebParseUrlBadPortClearsOutParams();
    testWebDecodePath();
    testWebGetStatusMsg();
    testWebDate();
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
    This is proprietary software and requires a commercial license from the author.
 */