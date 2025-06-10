/*
    link-agent-main/sample.c - Sample linking with the Ioto agent library and using the Ioto main()

    This sample provides its own main().
 */

/********************************** Includes **********************************/

#include "ioto.h"

/************************************* Code ***********************************/
/*
    Start processing. This runs in a fiber.
*/
PUBLIC int ioStart(void)
{
    rInfo("sample", "Hello World\n");

    //  Exit the sample
    rStop();
    return 0;
}


PUBLIC void ioStop(void) {}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
