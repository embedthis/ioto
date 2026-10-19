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

/*
    rMakeArgs splits a command line into an argv vector. The whole vector is one allocation, so
    a single rFree releases it.

    The TestMe comparison macros expand their arguments twice, so every call with a side effect
    is bound to a local before it is asserted.
 */
static void makeArgs()
{
    char  **argv;
    ssize argc;

    argv = NULL;
    argc = rMakeArgs("one two three", &argv, 0);
    teqz(argc, 3);
    tnotnull(argv);
    tmatch(argv[0], "one");
    tmatch(argv[1], "two");
    tmatch(argv[2], "three");
    tnull(argv[3]);
    rFree(argv);

    /*
        A single word, and runs of whitespace between words.
     */
    argv = NULL;
    argc = rMakeArgs("solo", &argv, 0);
    teqz(argc, 1);
    tmatch(argv[0], "solo");
    rFree(argv);

    argv = NULL;
    argc = rMakeArgs("  a   b  ", &argv, 0);
    teqz(argc, 2);
    tmatch(argv[0], "a");
    tmatch(argv[1], "b");
    rFree(argv);

    /*
        Quoted arguments keep their spaces and lose their quotes.
     */
    argv = NULL;
    argc = rMakeArgs("cmd \"arg with spaces\" last", &argv, 0);
    teqz(argc, 3);
    tmatch(argv[0], "cmd");
    tmatch(argv[1], "arg with spaces");
    tmatch(argv[2], "last");
    rFree(argv);

    argv = NULL;
    argc = rMakeArgs("cmd 'single quoted' last", &argv, 0);
    teqz(argc, 3);
    tmatch(argv[1], "single quoted");
    rFree(argv);

    /*
        argsOnly reserves argv[0] for the caller to fill in with the program name.
     */
    argv = NULL;
    argc = rMakeArgs("one two", &argv, 1);
    teqz(argc, 3);
    tmatch(argv[0], "");
    tmatch(argv[1], "one");
    tmatch(argv[2], "two");
    rFree(argv);

    /*
        Nothing to parse is an error, not an empty vector.
     */
    argv = NULL;
    argc = rMakeArgs("", &argv, 0);
    teqz(argc, R_ERR_BAD_ARGS);
    argv = NULL;
    argc = rMakeArgs("   ", &argv, 0);
    teqz(argc, R_ERR_BAD_ARGS);
    argv = NULL;
    argc = rMakeArgs(NULL, &argv, 0);
    teqz(argc, R_ERR_BAD_ARGS);
}


/*
    The rRun failure paths. Each returns rather than aborting, and none leaks the output buffer.
 */
static void runErrors()
{
    char *result;
    int  status;

    status = rRun(NULL, &result);
    teqi(status, R_ERR_BAD_ARGS);
    status = rRun("", &result);
    teqi(status, R_ERR_BAD_ARGS);

#if !ME_WIN_LIKE && !VXWORKS
    /*
        A command that exits non-zero returns its exit status, and the output is not returned.
     */
    result = NULL;
    status = rRun("sh -c \"exit 3\"", &result);
    teqi(status, 3);
    tnull(result);

    /*
        A command that does not exist fails in the child, which exits 127.
     */
    result = NULL;
    status = rRun("no-such-command-here", &result);
    teqi(status, 127);
    tnull(result);

    /*
        Output is optional: a caller that only wants the status passes null.
     */
    status = rRun("echo discarded", NULL);
    teqi(status, 0);

    /*
        Output larger than one buffer forces the read loop to grow.
     */
    result = NULL;
    status = rRun("sh -c \"head -c 200000 /dev/zero | tr '\\\\0' 'x'\"", &result);
    teqi(status, 0);
    tnotnull(result);
    ttrue(slen(result) > 65536);
    rFree(result);

    /*
        A command that is not empty but parses to no arguments is refused before the fork. The
        empty-string case above is caught earlier, by the explicit guard on the argument.
     */
    result = NULL;
    status = rRun("   \t  ", &result);
    teqi(status, R_ERR_BAD_ARGS);
    tnull(result);

    /*
        A child killed by a signal is not the same as one that exited non-zero: WIFEXITED is false,
        so the exit status is meaningless and the runtime must report the signal instead.
     */
    result = NULL;
    status = rRun("sh -c \"kill -9 $$\"", &result);
    teqi(status, R_ERR_BAD_STATE);
    tnull(result);

    /*
        Output beyond R_RUN_MAX_OUTPUT stops the read loop rather than growing without bound. The
        child keeps writing into a pipe nobody drains and is reaped by the wait below.
     */
    result = NULL;
    status = rRun("sh -c \"head -c 2000000 /dev/zero | tr '\\\\0' 'y'\"", &result);
    ttrue(result == NULL || slen(result) <= R_RUN_MAX_OUTPUT);
    rFree(result);
#endif
}


static void fiberMain(void *arg)
{
    runCommand();
    makeArgs();
    runErrors();
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
