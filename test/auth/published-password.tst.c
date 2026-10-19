/*
    published-password.tst.c - An account with the published default password "demo" cannot log in and is reported

    The 3.1 http template seeded accounts whose password was "demo", with the hashes in the source
    distribution, made over the bare password. A device upgraded in place keeps them. The hashes here
    are made at run time, in both that form and the password utility's, so no published hash ships.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "auth.h"

/************************************ Code ************************************/

static void testPublishedPassword(void)
{
    char *admin, *legacy, *current, *cookie, *log;

    waitFailurePeriod();
    admin = loginAdmin();
    tnotnull(admin);
    if (!admin) {
        return;
    }
    //  As the 3.1 template made them, and as the password utility makes them
    legacy = cryptMakePassword("demo", CRYPT_BLOWFISH_SALT_LENGTH, CRYPT_BLOWFISH_ROUNDS);
    current = makeHash("demo-current", "demo");
    teqi(changeUser(admin, "demo-legacy", "admin", legacy, 0), 200);
    teqi(changeUser(admin, "demo-current", "admin", current, 0), 200);

    log = readLog();
    tcontains(log, "User \"demo-legacy\" has the published default password");
    tcontains(log, "User \"demo-current\" has the published default password");
    rFree(log);

    cookie = NULL;
    teqi(postLogin("demo-legacy", "demo", NULL, &cookie), 401, "The published password must not log in");
    tnull(cookie);
    teqi(postLogin("demo-current", "demo", NULL, &cookie), 401, "The published password must not log in");
    tnull(cookie);

    log = readLog();
    tcontains(log, "Refused login as \"demo-current\"");
    rFree(log);

    teqi(changeUser(admin, "demo-legacy", NULL, NULL, 1), 200);
    teqi(changeUser(admin, "demo-current", NULL, NULL, 1), 200);
    rFree(legacy);
    rFree(current);
    rFree(admin);
}

static void fiberMain(void *data)
{
    if (scenarioSetup()) {
        testPublishedPassword();
    }
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
