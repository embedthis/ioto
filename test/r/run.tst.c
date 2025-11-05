/**
    run.tst.c - Unit tests for the Run command

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/************************************ Code ************************************/

static void runCommand()
{
    char *result;
    int  status;

#if ME_WIN_LIKE
    // Windows cmd.exe tests
    status = rRun("cmd.exe /c echo hello world", &result);
    teqi(status, 0);
    ttrue(result && *result);
    tmatch("hello world", strim(result, "\r\n", R_TRIM_END));
    rFree(result);

    status = rRun("cmd.exe /c ver", &result);
    teqi(status, 0);
    ttrue(result && *result);
    ttrue(scontains(result, "Windows") || scontains(result, "Microsoft"));
    rFree(result);

    // PowerShell test
    status = rRun("powershell.exe -Command \"Write-Output 'test powershell'\"", &result);
    teqi(status, 0);
    ttrue(result && *result);
    tmatch("test powershell", strim(result, "\r\n", R_TRIM_END));
    rFree(result);

    // Test with arguments containing spaces
    status = rRun("cmd.exe /c echo \"arg with spaces\"", &result);
    teqi(status, 0);
    ttrue(result && *result);
    tmatch("\"arg with spaces\"", strim(result, "\r\n", R_TRIM_END));
    rFree(result);

#elif VXWORKS
    // VxWorks doesn't support rRun yet - test should fail gracefully
    status = rRun("test", &result);
    teqi(status, R_ERR_BAD_STATE);
    tnull(result);

#else
    // Unix/Linux commands
    char host[256];

    gethostname(host, sizeof(host));
    status = rRun("hostname", &result);
    teqi(status, 0);
    ttrue(result && *result);
    tmatch(host, strim(result, "\n", R_TRIM_END));
    rFree(result);

    status = rRun("echo a b c d e f g", &result);
    teqi(status, 0);
    ttrue(result && *result);
    tmatch("a b c d e f g", strim(result, "\n", R_TRIM_END));
    rFree(result);
#endif
}

static void fiberMain(void *arg)
{
    runCommand();
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
