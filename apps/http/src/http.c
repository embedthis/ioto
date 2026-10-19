/*
    http.c -- App for the http app
 */
#include "http.h"

/************************************* Code ***********************************/
/*
    App setup called when Ioto starts.
 */
PUBLIC int ioStart(void)
{
    char *path;

    if (!dbFindOne(ioto->db, "User", NULL, NULL)) {
        rInfo("app", "Load db.json5");
        path = rGetFilePath("@config/db.json5");
        dbLoadData(ioto->db, path);
        rFree(path);
    }
    /*
        Say so rather than starting silently with a login nobody can pass. This template used to
        seed three accounts sharing the password "demo", published in the source distribution, so a
        device shipped from it carried a known administrative password. It now seeds none, and the
        empty table is the expected first-run state, not an error.
     */
    if (!dbFindOne(ioto->db, "User", NULL, NULL)) {
        rInfo("app", "No user accounts are defined, so no login will succeed.");
        rInfo("app", "Create one with: build/bin/password --cipher blowfish --realm web "
              "--password 'your-password' admin");
        rInfo("app", "then add the hash to the User table in config/db.json5. See that file.");
    }
    httpAddUser(ioto->webHost);
    return 0;
}

/*
    This is called when Ioto is shutting down
 */
PUBLIC void ioStop(void)
{
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */