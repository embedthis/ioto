/*
    password.c -- Create passwords

    password [--algorithm md5|sha256|sha512|bcrypt] [--password password] [--realm realm] user

    This file provides facilities for creating passwords. It supports MD5, SHA-256, SHA-512-256, and Bcrypt algorithms.
 */

/********************************* Includes ***********************************/

#define ME_COM_R    1
#define ME_COM_JSON 1

#include    "r.h"
#include    "crypt.h"
#include    "json.h"

/********************************* Forwards ***********************************/

static char *getPassword(void);

#if ME_WIN_LIKE || VXWORKS
static char *getpass(char *prompt);
#endif

/*********************************** Code *************************************/

static void usage(void)
{
    rFprintf(stderr, "usage: password [--algorithm algorithm] [--password password] [--realm realm] user\n"
             "Options:\n"
             "    --algorithm md5|sha256|sha512|bcrypt  Select the hash algorithm. Defaults to sha256\n"
             "    --password password                   Use the specified password\n"
             "    --realm realm                         Authentication realm (default: 'example.com')\n"
             "\n"
             "Algorithms:\n"
             "    md5      - Legacy MD5 (insecure, for compatibility only)\n"
             "    sha256   - SHA-256 (recommended for digest auth)\n"
             "    sha512   - SHA-512 (strongest for digest auth)\n"
             "    bcrypt   - Blowfish-based bcrypt (most secure, for session auth)\n"
             "\n"
             "Output format:\n"
             "    Passwords include algorithm prefix for self-identification:\n"
             "    MD5:hash, SHA256:hash, SHA512:hash, or BF1:rounds:salt:hash\n"
             "\n"
             "Examples:\n"
             "    password alice\n"
             "    password --algorithm md5 --realm 'Test Realm' bob\n"
             "    password --algorithm bcrypt --password secret123 alice\n"
             "\n");
    exit(1);
}


PUBLIC int main(int argc, char *argv[])
{
    RBuf *buf;
    char *password, *username, *encodedPassword, *cp, *algorithm, *realm, *hashInput;
    int  i, errflg, nextArg;

    username = 0;
    errflg = 0;
    password = 0;
    algorithm = "sha256";
    realm = "example.com";

    for (i = 1; i < argc && !errflg; i++) {
        if (argv[i][0] != '-') {
            break;
        }
        for (cp = &argv[i][1]; *cp && !errflg; cp++) {
            if (*cp == '-') {
                cp++;
            }
            if (smatch(cp, "algorithm") || smatch(cp, "cipher")) {
                if (++i == argc) {
                    errflg++;
                } else {
                    algorithm = argv[i];
                    if (!scaselessmatch(algorithm, "md5") &&
                        !scaselessmatch(algorithm, "sha256") &&
                        !scaselessmatch(algorithm, "sha512") &&
                        !scaselessmatch(algorithm, "sha512-256") &&
                        !scaselessmatch(algorithm, "bcrypt") &&
                        !scaselessmatch(algorithm, "blowfish")) {
                        errflg++;
                    }
                    break;
                }

            } else if (smatch(cp, "password") || smatch(cp, "p")) {
                if (++i == argc) {
                    errflg++;
                } else {
                    password = argv[i];
                    break;
                }

            } else if (smatch(cp, "realm") || smatch(cp, "r")) {
                if (++i == argc) {
                    errflg++;
                } else {
                    realm = argv[i];
                    break;
                }

            } else {
                errflg++;
            }
        }
    }
    nextArg = i;

    if ((nextArg + 1) > argc) {
        errflg++;
    }
    if (errflg) {
        usage();
        exit(2);
    }
    if (rInit(0, 0) < 0) {
        rFprintf(stderr, "pass: Cannot initialize runtime\n");
        exit(1);
    }
    username = argv[nextArg++];

    buf = rAllocBuf(0);
    for (i = nextArg; i < argc; ) {
        rPutStringToBuf(buf, argv[i]);
        if (++i < argc) {
            rPutCharToBuf(buf, ' ');
        }
    }
    if (!password && (password = getPassword()) == 0) {
        exit(7);
    }

    // Hash format: H(username:realm:password) for HTTP Digest authentication
    hashInput = sfmt("%s:%s:%s", username, realm, password);

    if (scaselessmatch(algorithm, "md5")) {
        encodedPassword = sfmt("MD5:%s", cryptGetMd5((uchar*) hashInput, slen(hashInput)));
        rPrintf("# Generated password hash for user '%s' with realm '%s' using %s\n", username, realm, algorithm);

    } else if (scaselessmatch(algorithm, "sha256")) {
        encodedPassword = sfmt("SHA256:%s", cryptGetSha256((uchar*) hashInput, slen(hashInput)));
        rPrintf("# Generated password hash for user '%s' with realm '%s' using %s\n", username, realm, algorithm);

    } else if (scaselessmatch(algorithm, "sha512") || scaselessmatch(algorithm, "sha512-256")) {
        encodedPassword = sfmt("SHA512:%s", cryptGetSha512((uchar*) hashInput, slen(hashInput)));
        rPrintf("# Generated password hash for user '%s' with realm '%s' using %s\n", username, realm, "sha512");

    } else if (scaselessmatch(algorithm, "bcrypt") || scaselessmatch(algorithm, "blowfish")) {
        // Bcrypt uses 16 bytes salt and 128 rounds by default (same as appweb)
        encodedPassword = cryptMakePassword(hashInput, 16, 128);
        rPrintf("# Generated password hash for user '%s' with realm '%s' using bcrypt\n", username, realm);
        rPrintf("# Note: Bcrypt is for session-based auth, not HTTP digest auth\n");

    } else {
        rFprintf(stderr, "Unknown algorithm: %s\n", algorithm);
        exit(3);
    }

    rPrintf("# Add this to your web.json5 users section:\n");
    rPrintf("%s: {\n", username);
    rPrintf("    password: '%s',\n", encodedPassword);
    rPrintf("    role: 'user'  # Change as needed\n");
    rPrintf("}\n");

    rTerm();
    return 0;
}


static char *getPassword(void)
{
    char *password, *confirm;

    password = cryptGetPassword("New password: ");
    confirm = cryptGetPassword("Confirm password: ");
    if (smatch(password, confirm)) {
        return password;
    }
    rFprintf(stderr, "Password not verified\n");
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
