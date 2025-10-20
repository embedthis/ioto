#include "ioto.h"

int main(int argc, char** argv)
{
    rInit((RFiberProc) ioInit, 0);

    //  Service requests until told to stop
    rServiceEvents();

    ioTerm();
    rTerm();
    return 0;
}

PUBLIC int ioStart(void)
{
    rInfo("sample", "Hello World\n");
    //  Your code here
    return 0;
}

PUBLIC void ioStop(void) {}

