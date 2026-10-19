/*
    user-revocation.tst.c - A change to the User table takes effect at once

    A demoted or removed user's sessions stop authenticating, the next login carries the role the
    database now holds, a later user of a removed name does not inherit its sessions, and a database
    user cannot log in under the name of a credentialed web.json5 user.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "auth.h"

/*********************************** Locals ***********************************/

#define CAROL_PASSWORD "carol-fixture-pw"

/************************************ Code ************************************/

static char *loginCarol(void)
{
    char *cookie;

    cookie = NULL;
    postLogin("carol", CAROL_PASSWORD, NULL, &cookie);
    return cookie;
}

static void testRevocation(void)
{
    char *admin, *first, *second, *third, *role, *hash;

    waitFailurePeriod();
    admin = loginAdmin();
    tnotnull(admin);
    if (!admin) {
        return;
    }
    hash = makeHash("carol", CAROL_PASSWORD);
    teqi(changeUser(admin, "carol", "admin", hash, 0), 200);

    first = loginCarol();
    tnotnull(first);
    role = whoami(first);
    tmatch(role, "admin");
    rFree(role);

    //  Demote: the open session stops authenticating and the next login carries the new role
    teqi(changeUser(admin, "carol", "user", NULL, 0), 200);
    role = whoami(first);
    tnull(role, "A demoted user's session must stop authenticating");
    rFree(role);
    second = loginCarol();
    tnotnull(second);
    role = whoami(second);
    tmatch(role, "user", "The next login must carry the role the database holds");
    rFree(role);

    //  Promote: works without a restart
    teqi(changeUser(admin, "carol", "admin", NULL, 0), 200);
    third = loginCarol();
    tnotnull(third, "A promoted user must be able to log in");
    role = whoami(third);
    tmatch(role, "admin");
    rFree(role);

    //  Remove: the session stops authenticating
    teqi(changeUser(admin, "carol", NULL, NULL, 1), 200);
    role = whoami(third);
    tnull(role, "A removed user's session must stop authenticating");
    rFree(role);

    //  Re-create the name with the same role: the old session must not come back
    teqi(changeUser(admin, "carol", "admin", hash, 0), 200);
    rFree(loginCarol());
    role = whoami(third);
    tnull(role, "A new user of a removed name must not inherit its sessions");
    rFree(role);

    teqi(changeUser(admin, "carol", NULL, NULL, 1), 200);
    rFree(first);
    rFree(second);
    rFree(third);
    rFree(hash);
    rFree(admin);
}

/*
    "ops" is a credentialed user in web.json5. A database user of the same name cannot log in as it.
 */
static void testCollision(void)
{
    char *admin, *hash, *cookie;

    admin = loginAdmin();
    tnotnull(admin);
    if (!admin) {
        return;
    }
    hash = makeHash("ops", CAROL_PASSWORD);
    teqi(changeUser(admin, "ops", "admin", hash, 0), 200);
    cookie = NULL;
    teqi(postLogin("ops", CAROL_PASSWORD, NULL, &cookie), 401, "A database user must not take a web.json5 user's name");
    tnull(cookie);
    teqi(changeUser(admin, "ops", NULL, NULL, 1), 200);
    rFree(hash);
    rFree(admin);
}

static void fiberMain(void *data)
{
    if (scenarioSetup()) {
        testRevocation();
        testCollision();
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
