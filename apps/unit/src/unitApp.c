/*
    app.c -- Support for unit tests
 */
#include "unit.h"

/*
    App setup called when Ioto starts.
 */
PUBLIC int ioStart(void)
{
    cchar *suite;

    if ((suite = ioto->cmdTest) == 0) {
        if ((suite = getenv("IOTO_TEST")) == 0) {
            rError("unit", "No test suite specified");
            // return R_ERR_CANT_FIND;
        }
    }
#if SERVICES_CLOUD
    if (ioto->provisionService) {
        ioOnConnect((RWatchProc) unitTest, (void*) suite, 0);
    } else {
        unitTest(suite);
    }
#else
    unitTest(suite);
#endif
    return 0;
}

/*
    This is called when Ioto is shutting down
 */
PUBLIC void ioStop(void)
{
}
