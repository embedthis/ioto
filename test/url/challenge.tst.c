/*
    challenge.tst.c - Unit tests for parsing WWW-Authenticate challenges

    These tests drive urlParseAuthChallenge with challenges a hostile or non-conforming server
    could return. The test web server only ever issues well formed challenges, so the rejection
    paths are unreachable through a live request.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

#if URL_AUTH

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

/*
    Install a synthetic WWW-Authenticate response header.
    Rx headers are held tokenized as "key\0 value\0\0" -- the form left behind by parsing
    "key: value\r\n" in-situ. setChallenge asserts the layout via urlGetHeader so a change to the
    representation fails here rather than in the challenge tests below.
 */
static void setChallenge(Url *up, cchar *challenge)
{
    rFreeBuf(up->rxHeaders);
    up->rxHeaders = rAllocBuf(slen(challenge) + 32);
    rPutBlockToBuf(up->rxHeaders, "www-authenticate", slen("www-authenticate") + 1);
    rPutBlockToBuf(up->rxHeaders, " ", 1);
    rPutBlockToBuf(up->rxHeaders, challenge, slen(challenge) + 1);
    rPutBlockToBuf(up->rxHeaders, "", 1);
    tmatch(urlGetHeader(up, "WWW-Authenticate"), challenge);
}

static Url *allocChallenged(cchar *challenge)
{
    Url *up;

    up = urlAlloc(0);
    urlSetAuth(up, "bob", "password", NULL);
    setChallenge(up, challenge);
    return up;
}

/*
    A Basic challenge needs no parameters. Without a configured type it is refused on a plain
    connection, since answering it would send the password in the clear; a Basic client accepts it.
 */
static void testBasicChallenge()
{
    Url *up;

    up = allocChallenged("Basic realm=\"Test Realm\"");
    ttrue(!urlParseAuthChallenge(up));
    tcontains(urlGetError(up), "insecure");
    ttrue(up->offeredBasic == 0);
    ttrue(up->authType == NULL);
    urlFree(up);

    up = urlAlloc(0);
    urlSetAuth(up, "bob", "password", "basic");
    setChallenge(up, "Basic realm=\"Test Realm\"");
    ttrue(urlParseAuthChallenge(up));
    ttrue(up->offeredBasic == 1);
    tmatch(up->authType, "basic");
    urlFree(up);
}

/*
    A full digest challenge populates every parameter
 */
static void testDigestChallenge()
{
    Url *up;

    up = allocChallenged("Digest realm=\"Test Realm\", nonce=\"abc123\", qop=\"auth\", "
                         "opaque=\"opaque-value\", algorithm=SHA-256");
    ttrue(urlParseAuthChallenge(up));
    //  The challenge supplies the parameters; the configured type stays as the caller set it
    ttrue(up->authType == NULL);
    tmatch(up->realm, "Test Realm");
    tmatch(up->nonce, "abc123");
    tmatch(up->qop, "auth");
    tmatch(up->opaque, "opaque-value");
    tmatch(up->algorithm, "SHA-256");
    urlFree(up);
}

/*
    A challenge with no algorithm defaults to MD5, and unquoted values are accepted
 */
static void testDigestUnquoted()
{
    Url *up;

    up = allocChallenged("Digest realm=Realm, nonce=xyz, qop=auth");
    ttrue(urlParseAuthChallenge(up));
    tmatch(up->realm, "Realm");
    tmatch(up->nonce, "xyz");
    tmatch(up->qop, "auth");
    urlFree(up);
}


static void testDigestStale()
{
    Url *up;

    up = allocChallenged("Digest realm=\"Test Realm\", nonce=\"fresh\", qop=\"auth\", stale=true");
    ttrue(urlParseAuthChallenge(up));
    tmatch(up->nonce, "fresh");
    urlFree(up);
}

static void testDigestNotStale()
{
    Url *up;

    up = allocChallenged("Digest realm=\"Test Realm\", nonce=\"fresh\", qop=\"auth\", stale=false");
    ttrue(urlParseAuthChallenge(up));
    tmatch(up->nonce, "fresh");
    urlFree(up);
}

/*
    Unknown parameters are ignored rather than failing the challenge
 */
static void testDigestUnknownParam()
{
    Url *up;

    up = allocChallenged("Digest realm=\"Test Realm\", nonce=\"abc\", qop=\"auth\", domain=\"/api\"");
    ttrue(urlParseAuthChallenge(up));
    tmatch(up->nonce, "abc");
    urlFree(up);
}

/*
    Only MD5 and SHA-256 are supported
 */
static void testDigestBadAlgorithm()
{
    Url *up;

    up = allocChallenged("Digest realm=\"Test Realm\", nonce=\"abc\", qop=\"auth\", algorithm=SHA-512");
    ttrue(!urlParseAuthChallenge(up));
    urlFree(up);
}

/*
    Only the "auth" qop is supported. "auth-int" requires hashing the body and is refused.
 */
static void testDigestAuthIntQop()
{
    Url *up;

    up = allocChallenged("Digest realm=\"Test Realm\", nonce=\"abc\", qop=\"auth-int\"");
    ttrue(!urlParseAuthChallenge(up));
    urlFree(up);
}

/*
    RFC 7616 section 3.4 makes qop a list of alternatives from which the client must choose one.
    A list offering "auth" alongside "auth-int" -- what Apache sends when AuthDigestQop names both --
    must select "auth", and the chosen value alone goes into the Authorization header.
 */
static void testDigestQopList()
{
    Url *up;

    up = allocChallenged("Digest realm=\"Test Realm\", nonce=\"abc\", qop=\"auth\"");
    ttrue(urlParseAuthChallenge(up));
    tmatch(up->qop, "auth");
    urlFree(up);

    up = allocChallenged("Digest realm=\"Test Realm\", nonce=\"abc\", qop=\"auth,auth-int\"");
    ttrue(urlParseAuthChallenge(up));
    tmatch(up->qop, "auth");
    urlFree(up);

    //  Order and surrounding white space must not matter
    up = allocChallenged("Digest realm=\"Test Realm\", nonce=\"abc\", qop=\"auth-int, auth\"");
    ttrue(urlParseAuthChallenge(up));
    tmatch(up->qop, "auth");
    urlFree(up);
}

/*
    An oversized parameter is a denial of service vector and must be rejected
 */
static void testDigestOversizedParam()
{
    Url  *up;
    char *challenge, *nonce;

    //  MAX_DIGEST_PARAM_LEN is 8192
    nonce = rAlloc(9001);
    memset(nonce, 'A', 9000);
    nonce[9000] = '\0';
    challenge = sfmt("Digest realm=\"Test Realm\", nonce=\"%s\", qop=\"auth\"", nonce);

    up = allocChallenged(challenge);
    ttrue(!urlParseAuthChallenge(up));
    urlFree(up);

    rFree(challenge);
    rFree(nonce);
}

/*
    A truncated parameter list must be rejected, not read past the end
 */
static void testDigestMalformed()
{
    Url *up;

    up = allocChallenged("Digest realm");
    ttrue(!urlParseAuthChallenge(up));
    ttrue(up->realm == NULL);
    urlFree(up);

    up = allocChallenged("Digest realm=\"unterminated");
    ttrue(!urlParseAuthChallenge(up));
    urlFree(up);
}

/*
    An unrecognised scheme is not a challenge we can answer
 */
static void testUnknownScheme()
{
    Url *up;

    up = allocChallenged("Negotiate");
    ttrue(!urlParseAuthChallenge(up));
    urlFree(up);
}

/*
    No challenge header at all
 */
static void testNoChallenge()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(!urlParseAuthChallenge(up));
    urlFree(up);

    ttrue(!urlParseAuthChallenge(NULL));
}


static void testDigestNoQop()
{
    Url *up;

    up = allocChallenged("Digest realm=\"Test Realm\", nonce=\"deadbeef\", algorithm=MD5");
    ttrue(!urlParseAuthChallenge(up));
    urlFree(up);

    up = allocChallenged("Digest realm=\"Test Realm\", nonce=\"deadbeef\", algorithm=SHA-256");
    ttrue(!urlParseAuthChallenge(up));
    urlFree(up);
}

/*
    A digest request carrying a query string. RFC 7616 section 3.4 makes the digest uri the
    Request-URI, so the client hashes "METHOD:/path?query" and sends uri="/path?query".
 */
static void testDigestWithQuery()
{
    Url  *up;
    char url[128];

    //  alice's password is stored as a SHA-256 hash, as required by the SHA-256 /digest/ route
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", "digest");
    ttrue(urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP), NULL, 0, NULL) == 200);
    tcontains(urlGetResponse(up), "Digest Authentication Success");
    urlFree(up);

    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", "digest");
    ttrue(urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html?a=1&b=2", HTTP), NULL, 0, NULL) == 200);
    tcontains(urlGetResponse(up), "Digest Authentication Success");
    urlFree(up);
}

/*
    Values containing escaped quotes must survive the round trip into the Authorization header
 */
static void testDigestEscapedValues()
{
    Url *up;

    up = allocChallenged("Digest realm=\"Say \\\"Hello\\\"\", nonce=\"a\\\\b\", qop=\"auth\"");
    ttrue(urlParseAuthChallenge(up));
    tmatch(up->realm, "Say \"Hello\"");
    tmatch(up->nonce, "a\\b");
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testBasicChallenge();
        testDigestChallenge();
        testDigestUnquoted();
        testDigestStale();
        testDigestNotStale();
        testDigestUnknownParam();
        testDigestBadAlgorithm();
        testDigestAuthIntQop();
        testDigestQopList();
        testDigestOversizedParam();
        testDigestMalformed();
        testUnknownScheme();
        testNoChallenge();
        testDigestEscapedValues();
        testDigestNoQop();
        testDigestWithQuery();
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

#else

int main(void)
{
    tskip("URL_AUTH is not enabled");
    return 0;
}

#endif /* URL_AUTH */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
