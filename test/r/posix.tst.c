/*
    posix.tst.c - Process hygiene on Unix: umask, inherited descriptors and signals, rRun limits, signal handlers, log file opening and daemonizing.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

#if ME_UNIX_LIKE
#include    <sys/wait.h>

/*********************************** Locals ***********************************/

#define MAX_FDS     256

static bool baseline[MAX_FDS];         /* Descriptors that were inheritable before the runtime started */
static char markerPath[ME_MAX_FNAME];
static char atexitPath[ME_MAX_FNAME];
static char pidPath[ME_MAX_FNAME];

/************************************ Code ************************************/
/*
    Find the descriptors from 3 up that a child created by exec would inherit.
 */
static void snapshotInheritable(bool *set)
{
    int fd, flags;

    for (fd = 3; fd < MAX_FDS; fd++) {
        flags = fcntl(fd, F_GETFD);
        set[fd] = (flags >= 0 && !(flags & FD_CLOEXEC)) ? 1 : 0;
    }
}



static void umaskKept(void)
{
    mode_t original, mask;

    original = umask(0);
    umask(original);

    umask(077);
    rInitOs();
    mask = umask(077);
    teqi((int) mask, 077);

    //  Again, as a restart does
    rInitOs();
    mask = umask(077);
    teqi((int) mask, 077);


    umask(0);
    rInitOs();
    mask = umask(0);
    teqi((int) mask, 022);

    umask(027);
    rInitOs();
    mask = umask(027);
    teqi((int) mask, 027);

    umask(original);
}



static void descriptorsCloseOnExec(void)
{
    bool now[MAX_FDS];
    char *path;
    int  fd, rc, tempFd;

    sfmtbuf(markerPath, sizeof(markerPath), "data/temp-cloexec-%d.log", (int) getpid());
    unlink(markerPath);
    rc = rSetLogPath(markerPath, 1);
    teqi(rc, 0);
    fd = rGetLogFile();
    ttrue(fd > 2);

    path = NULL;
    tempFd = rOpenTempFile("data", "temp-cloexec", &path);
    ttrue(tempFd > 2);

    snapshotInheritable(now);
    for (fd = 3; fd < MAX_FDS; fd++) {
        if (now[fd] && !baseline[fd]) {
            tfail("descriptor %d would be inherited by a child process", fd);
        }
    }
    close(tempFd);
    unlink(path);
    rFree(path);
    rSetLogPath("stdout", 1);
    unlink(markerPath);
}



static void runChildState(void)
{
    char *output;
    char command[256];
    int  fd, rc, status;

    // The child must restore SIGPIPE so it terminates when its reader closes.
    status = rRun("sh -c \"kill -s PIPE $$\"", NULL);
    teqi(status, R_ERR_BAD_STATE);


    sfmtbuf(markerPath, sizeof(markerPath), "data/temp-child-%d.log", (int) getpid());
    unlink(markerPath);
    rc = rSetLogPath(markerPath, 1);
    teqi(rc, 0);
    fd = rGetLogFile();
    ttrue(fd > 2);
    sfmtbuf(command, sizeof(command), "sh -c \"echo leaked >&%d\"", fd);
    output = NULL;
    status = rRun(command, &output);
    tneqi(status, 0);
    rFree(output);
    rSetLogPath("stdout", 1);
    output = rReadFile(markerPath, NULL);
    tnotnull(output);
    tfalse(scontains(output, "leaked") != 0);
    rFree(output);
    unlink(markerPath);
}



static void runOutputLimit(void)
{
    Ticks start;
    char  *output;
    int   status;

    //  A runaway command would otherwise hold the test until the alarm ends it
    alarm(30);
    start = rGetTicks();

    status = rRun("yes", NULL);
    teqi(status, R_ERR_WONT_FIT);

    output = NULL;
    status = rRun("yes", &output);
    teqi(status, R_ERR_WONT_FIT);
    tnull(output);

    alarm(0);
    ttrue(rGetTicks() - start < 20 * TPS);

    //  Output below the limit is unaffected
    output = NULL;
    status = rRun("echo within the limit", &output);
    teqi(status, 0);
    tnotnull(output);
    tcontains(output, "within the limit");
    rFree(output);
}



static void handlersRemoved(struct sigaction *before)
{
    static const int signals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE };
    struct sigaction after;
    int              i;

    for (i = 0; i < (int) (sizeof(signals) / sizeof(signals[0])); i++) {
        sigaction(signals[i], NULL, &after);
        ttrue(after.sa_handler == before[i].sa_handler);
    }
}


static void handlersInstalled(struct sigaction *before)
{
    static const int signals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE };
    struct sigaction current;
    int              i;

    for (i = 0; i < (int) (sizeof(signals) / sizeof(signals[0])); i++) {
        sigaction(signals[i], NULL, &current);
        ttrue(current.sa_handler != before[i].sa_handler);
    }
}



static void logSignal(void)
{
    bool emit;

    rSetLogFilter("error", "app", 1);
    emit = rEmitLog("debug", "other");
    tfalse(emit);

    raise(SIGUSR2);
    emit = rEmitLog("debug", "other");
    ttrue(emit);

    rSetLogFilter("all", "all", 1);
}



static void logNoFollow(void)
{
    char target[ME_MAX_FNAME], link[ME_MAX_FNAME];
    char *contents;
    int  rc;

    sfmtbuf(target, sizeof(target), "data/temp-target-%d.txt", (int) getpid());
    sfmtbuf(link, sizeof(link), "data/temp-link-%d.log", (int) getpid());
    unlink(target);
    unlink(link);
    rc = (int) rWriteFile(target, "original", 8, 0600);
    teqi(rc, 8);
    rc = symlink(rBasename(target), link);
    teqi(rc, 0);

    rc = rSetLogPath(link, 1);
    teqi(rc, R_ERR_CANT_OPEN);
    rSetLogPath("stdout", 1);

    //  The target was not touched
    contents = rReadFile(target, NULL);
    tnotnull(contents);
    tmatch(contents, "original");
    rFree(contents);
    unlink(link);
    unlink(target);
}


static void atexitHandler(void)
{
    int fd;

    if ((fd = open(atexitPath, O_CREAT | O_WRONLY, 0600)) >= 0) {
        close(fd);
    }
}



static void daemonize(void)
{
    struct stat info, null;
    char        report[256];
    ssize       length;
    char        *contents;
    pid_t       pid;
    int         fd, fds[2], i, rc, status;

    sfmtbuf(markerPath, sizeof(markerPath), "data/temp-daemon-%d.out", (int) getpid());
    sfmtbuf(atexitPath, sizeof(atexitPath), "data/temp-daemon-%d.atexit", (int) getpid());
    sfmtbuf(pidPath, sizeof(pidPath), "data/temp-daemon-%d.pid", (int) getpid());
    unlink(markerPath);
    unlink(atexitPath);
    unlink(pidPath);
    rSetPidPath(pidPath);

    rc = pipe(fds);
    teqi(rc, 0);
    if ((pid = fork()) == 0) {
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]);
        close(fds[1]);
        atexit(atexitHandler);
        if (rDaemonize() != 0) {
            _exit(90);
        }
        //  The daemon
        fd = open(markerPath, O_CREAT | O_WRONLY | O_TRUNC, 0600);
        if (fd < 0) {
            _exit(91);
        }
        memset(&null, 0, sizeof(null));
        memset(&info, 0, sizeof(info));
        fstat(STDIN_FILENO, &info);
        stat("/dev/null", &null);
        sfmtbuf(report, sizeof(report), "leader=%d stdin=%s\n", getsid(0) == getpid() ? 1 : 0,
                (S_ISCHR(info.st_mode) && info.st_rdev == null.st_rdev) ? "null" : "other");
        if (write(fd, report, slen(report)) < 0) {
            _exit(92);
        }
        close(fd);
        _exit(0);
    }
    ttrue(pid > 0);
    close(fds[0]);
    close(fds[1]);

    //  The original process exits at once with success
    rc = (int) waitpid(pid, &status, 0);
    teqi(rc, (int) pid);
    ttrue(WIFEXITED(status));
    teqi(WEXITSTATUS(status), 0);


    tfalse(rFileExists(atexitPath));

    //  Wait for the daemon to report, but not for long
    for (i = 0; i < 50 && !rFileExists(markerPath); i++) {
        usleep(100 * 1000);
    }
    ttrue(rFileExists(markerPath));
    for (i = 0; i < 50; i++) {
        contents = rReadFile(markerPath, NULL);
        if (contents && slen(contents) > 0) {
            break;
        }
        rFree(contents);
        contents = NULL;
        usleep(100 * 1000);
    }
    tnotnull(contents);
    length = (ssize) slen(contents);
    ttrue(length > 0);
    tcontains(contents, "leader=1");
    tcontains(contents, "stdin=null");
    rFree(contents);

    rSetPidPath(NULL);
    unlink(markerPath);
    unlink(atexitPath);
    unlink(pidPath);
}


int main(void)
{
    struct sigaction before[4];

    snapshotInheritable(baseline);
    sigaction(SIGSEGV, NULL, &before[0]);
    sigaction(SIGBUS, NULL, &before[1]);
    sigaction(SIGILL, NULL, &before[2]);
    sigaction(SIGFPE, NULL, &before[3]);

    rInit(0, 0);
    handlersInstalled(before);

    umaskKept();
    descriptorsCloseOnExec();
    runChildState();
    runOutputLimit();
    logSignal();
    logNoFollow();
    daemonize();

    rTerm();
    handlersRemoved(before);
    return 0;
}

#else

int main(void)
{
    tskip("Unix process hygiene is not applicable on this platform");
    return 0;
}
#endif /* ME_UNIX_LIKE */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
