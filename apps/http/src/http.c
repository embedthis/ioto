/*
    http.c -- App for the http app
 */
#include "http.h"
#include "httpUser.c"

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
    httpAddUser(ioto->webHost);
    return 0;
}

/*
    This is called when Ioto is shutting down
 */
PUBLIC void ioStop(void)
{
}