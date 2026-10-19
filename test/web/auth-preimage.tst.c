/*
    auth-preimage.tst.c - Hash preimages must not silently truncate

    Every preimage in the authentication path was assembled with SFMT into a fixed stack buffer.
    SFMT truncates rather than overflowing, so there was no memory-safety bug, but the truncation
    was silent and was applied to inputs the client controls. Two inputs differing only past the
    cut produce an identical preimage and therefore an identical hash, which in a credential
    comparator is a collision the attacker chooses.

    webHashPassword is the function an application calls to hash a password for storage, so a long
    username silently produced a weak stored credential. It is exercised here directly rather than
    over the wire: an over-long username on the Basic path is now refused at the parse boundary and
    an unknown user is challenged anyway, so the collision is not observable from outside.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

/*
    A host is needed only for its realm and algorithm. Port 4872 is inside the 4850-4899 range
    reserved for in-process hosts, so the shared server is untouched.
 */
static cchar *CONFIG =
    "{"
    "    web: {"
    "        listen: [ 'http://127.0.0.1:4872' ],"
    "        documents: './site',"
    "        auth: { realm: 'Preimage Realm', algorithm: 'SHA-256' },"
    "    }"
    "}";

/************************************ Code ************************************/

/*
    H(username:realm:password) in a 512-byte buffer: a 600-character username truncated the realm
    and the whole password out of the preimage, so every password hashed to the same value for
    that username.
 */
static void testLongUsernameKeepsPasswordInPreimage(void)
{
    WebHost *host;
    char    username[600], *one, *two;

    if ((host = startTestHost(CONFIG)) == 0) {
        return;
    }
    memset(username, 'A', sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';

    one = webHashPassword(host, username, "password-one");
    two = webHashPassword(host, username, "password-two");
    tnotnull(one);
    tnotnull(two);
    ttrue(!smatch(one, two), "Two passwords under a long username must not hash alike");

    rFree(one);
    rFree(two);
    stopTestHost(host);
}

/*
    The same property at ordinary lengths, so the comparison above is not passing for some reason
    unrelated to the preimage, and a repeat hash of the same inputs is still stable.
 */
static void testShortCredentialsHashDistinctlyAndStably(void)
{
    WebHost *host;
    char    *one, *two, *again;

    if ((host = startTestHost(CONFIG)) == 0) {
        return;
    }
    one = webHashPassword(host, "alice", "password-one");
    two = webHashPassword(host, "alice", "password-two");
    again = webHashPassword(host, "alice", "password-one");

    ttrue(!smatch(one, two), "Different passwords must hash differently");
    tmatch(one, again);

    rFree(one);
    rFree(two);
    rFree(again);
    stopTestHost(host);
}

/*
    A username at the documented bound must still produce a password-dependent hash. This is the
    length webAddUser accepts for a stored credential, so it has to remain usable.
 */
static void testMaxLengthUsernameStillHashes(void)
{
    WebHost *host;
    char    username[ME_WEB_MAX_AUTH + 1], *one, *two;

    if ((host = startTestHost(CONFIG)) == 0) {
        return;
    }
    memset(username, 'U', sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';

    one = webHashPassword(host, username, "password-one");
    two = webHashPassword(host, username, "password-two");
    ttrue(!smatch(one, two), "A maximum-length username must not collapse the preimage");

    rFree(one);
    rFree(two);
    stopTestHost(host);
}

static void fiberMain(void *data)
{
    testLongUsernameKeepsPasswordInPreimage();
    testShortCredentialsHashDistinctlyAndStably();
    testMaxLengthUsernameStillHashes();
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
