/*
    ecoApp.c --
 */
#include "eco.h"

/************************************* Code ***********************************/
/*
    Called when Ioto starts.
 */
PUBLIC int ioStart(void)
{
    //  See if the demo is enabled
    if (jsonGetBool(ioto->config, 0, "demo.enable", 0)) {
        //  Wait till connected to the device cloud
        ioOnConnect((RWatchProc) ecoManager, 0, 0);
    }
    return 0;
}

/*
    This is called when Ioto is shutting down
 */
PUBLIC void ioStop(void)
{
}
