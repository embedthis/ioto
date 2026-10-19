/**
    win.tst.c - Windows runtime hygiene

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "testme.h"
#include "r.h"

#if ME_WIN_LIKE

/*********************************** Locals ***********************************/

#define WALK_DIR ".test/win"

/************************************ Code ************************************/

/*
    Repeated startup detects window classes left registered by shutdown.
 */
static void testStartStop(void)
{
    _invalid_parameter_handler before;
    HWND                       window;
    int                        i, rc;

    before = _get_invalid_parameter_handler();
    for (i = 0; i < 3; i++) {
        rc = rInit(NULL, NULL);
        teqi(rc, 0, "rInit must succeed on cycle %d", i);
        window = rGetHwnd();
        ttrue(window != NULL, "The runtime window must exist while running");
        ttrue(_get_invalid_parameter_handler() != before, "The runtime installs its handler");
        rTerm();
        ttrue(_get_invalid_parameter_handler() == before, "The prior handler must be restored");
        ttrue(rGetHwnd() == NULL, "The window handle must be cleared");
        tfalse(IsWindow(window), "The window must be destroyed");
    }
}

static void testRunOutputLimit(void)
{
    char  *result;
    Ticks start;
    int   status;


    start = rGetTicks();
    result = NULL;
    status = rRun("cmd.exe /c \"for /L %i in (1,1,100000000) do @echo xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"",
                  &result);
    teqi(status, R_ERR_WONT_FIT);
    tnull(result);
    ttrue(rGetElapsedTicks(start) < 60 * TPS, "The runaway command must be stopped");

    start = rGetTicks();
    status = rRun("cmd.exe /c \"for /L %i in (1,1,100000000) do @echo xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"",
                  NULL);
    teqi(status, R_ERR_WONT_FIT);
    ttrue(rGetElapsedTicks(start) < 60 * TPS, "The runaway command must be stopped");

    //  Output within the limit is complete
    result = NULL;
    status = rRun("cmd.exe /c echo within the limit", &result);
    teqi(status, 0);
    tnotnull(result);
    if (result) {
        tmatch(strim(result, "\r\n", R_TRIM_END), "within the limit");
    }
    rFree(result);
}

static void testRunStatus(void)
{
    char *result;
    int  status;

    //  A failed command reports its status and returns no output
    result = NULL;
    status = rRun("cmd.exe /c \"echo discarded & exit 3\"", &result);
    teqi(status, 3);
    tnull(result);


    result = NULL;
    status = rRun("powershell.exe -NoProfile -Command \"[Console]::In.ReadToEnd().Length\"", &result);
    teqi(status, 0);
    tnotnull(result);
    if (result) {
        tmatch(strim(result, "\r\n", R_TRIM_END), "0");
    }
    rFree(result);

    //  A command that is not there
    result = NULL;
    status = rRun("no-such-command-here.exe", &result);
    ttrue(status < 0);
    tnull(result);
}

static void makeTree(void)
{
    char *dirs[] = { ".test", WALK_DIR, WALK_DIR "/a", WALK_DIR "/a/b", WALK_DIR "/c" };
    char *files[] = { WALK_DIR "/top.txt", WALK_DIR "/a/one.txt", WALK_DIR "/a/b/two.txt", WALK_DIR "/c/three.txt" };
    int  fd, i;

    for (i = 0; i < (int) (sizeof(dirs) / sizeof(*dirs)); i++) {
        _mkdir(dirs[i]);
    }
    for (i = 0; i < (int) (sizeof(files) / sizeof(*files)); i++) {
        fd = open(files[i], O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, 0644);
        if (fd >= 0) {
            close(fd);
        }
    }
}

static void removeTree(void)
{
    rRun("cmd.exe /c rmdir /s /q .test\\win", NULL);
}

static bool hasEntry(RList *list, cchar *name)
{
    cchar *item;
    int   next;

    for (ITERATE_ITEMS(list, item, next)) {
        if (smatch(item, name)) {
            return 1;
        }
    }
    return 0;
}


static void testNestedWalk(void)
{
    RList *list;

    list = rGetFiles(WALK_DIR, "**", R_WALK_FILES | R_WALK_RELATIVE);
    tnotnull(list);
    if (list) {
        teqz(rGetListLength(list), 4);
        ttrue(hasEntry(list, "top.txt"));
        ttrue(hasEntry(list, "a\\one.txt"));
        ttrue(hasEntry(list, "a\\b\\two.txt"));
        ttrue(hasEntry(list, "c\\three.txt"));
        rFreeList(list);
    }
    list = rGetFiles(WALK_DIR, "**", R_WALK_DIRS | R_WALK_RELATIVE);
    tnotnull(list);
    if (list) {
        teqz(rGetListLength(list), 3);
        rFreeList(list);
    }
}


static void testJunctionNotFollowed(void)
{
    RList *list;
    int   status;

    status = rRun("cmd.exe /c mklink /J .test\\win\\a\\b\\loop .test\\win", NULL);
    if (status != 0) {
        tinfo("Skipped: Cannot create a junction here");
        return;
    }
    list = rGetFiles(WALK_DIR, "**", R_WALK_FILES | R_WALK_DIRS | R_WALK_RELATIVE);
    tnotnull(list, "The walk must finish");
    if (list) {
        ttrue(hasEntry(list, "a\\b\\two.txt"));
        tfalse(hasEntry(list, "a\\b\\loop\\top.txt"), "The junction must not be followed");
        tfalse(hasEntry(list, "a\\b\\loop\\a"), "The junction must not be followed");
        rFreeList(list);
    }
}

static void fiberMain(void *arg)
{
    (void) arg;

    testRunOutputLimit();
    testRunStatus();
    makeTree();
    testNestedWalk();
    testJunctionNotFollowed();
    removeTree();
    rStop();
}

int main(void)
{
    testStartStop();

    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

#else /* !ME_WIN_LIKE */

int main(void)
{
    tskip("Windows runtime hygiene is not applicable on this platform");
    return 0;
}

#endif /* ME_WIN_LIKE */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
