/*
    password.c -- Create passwords

    pass [--cipher sha256|bcrypt] [--password password] user

    This file provides facilities for creating passwords. It supports the MD5 or Blowfish ciphers.
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
    rFprintf(stderr, "usage: pass [--cipher cipher] [--password password] user\n"
             "Options:\n"
             "    --cipher sha256|bcrypt Select the encryption cipher. Defaults to bcrypt\n"
             "    --file filename        Modify the password file\n"
             "    --password password    Use the specified password\n"
             "\n\n");
    exit(1);
}


PUBLIC int main(int argc, char *argv[])
{
    RBuf *buf;
    char *password, *username, *encodedPassword, *cp, *cipher;
    int  i, errflg, nextArg;

    username = 0;
    errflg = 0;
    password = 0;
    cipher = "bcrypt";

    for (i = 1; i < argc && !errflg; i++) {
        if (argv[i][0] != '-') {
            break;
        }
        for (cp = &argv[i][1]; *cp && !errflg; cp++) {
            if (*cp == '-') {
                cp++;
            }
            if (smatch(cp, "cipher")) {
                if (++i == argc) {
                    errflg++;
                } else {
                    cipher = argv[i];
                    if (!smatch(cipher, "sha256") && !smatch(cipher, "bcrypt")) {
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
    if (smatch(cipher, "sha256")) {
        encodedPassword = cryptGetSha256((uchar*) sfmt("%s:%s", username, password), 0);
    } else {
        // This uses the more secure bcrypt cipher
        encodedPassword = cryptMakePassword(sfmt("%s:%s", username, password), 0, 0);
    }
    rPrintf("%s\n", encodedPassword);
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
