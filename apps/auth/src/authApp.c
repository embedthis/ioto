/*
    app.c -- App for the auth app
 */
#include "auth.h"

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
    authAddUser(ioto->webHost);
    return 0;
}

/*
    This is called when Ioto is shutting down
 */
PUBLIC void ioStop(void)
{
}