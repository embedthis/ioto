/*
    logs.c - Capture logs, files or command output to the cloud

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

#if SERVICES_LOGS
/************************************ Locals **********************************/
/*
    Initial input buffer size
 */
#define MAX_LINE 2048

typedef struct Log {
    IotoLog *log;                   /* Log capture */
    char *path;                     /* Log filename */
    RBuf *buf;                      /* Input buffer */
    RWait *wait;                    /* Wait on IO */
    FILE *fp;                       /* File pointer for the file or command pipe */
    Offset pos;                     /* File position */
    ino_t inode;                    /* File inode number */
    dev_t dev;                      /* File dev number */
    cchar *command;                 /* Command to run */
    cchar *continuation;            /* Continuation line prefix */
    bool lines : 1;                 /* Output is composed of one line entries (with continuations) */
    bool tail : 1;                  /* Capture from the file tail */
#if LINUX && HAS_INOTIFY
    int notifyFd;                   /* inotify fd */
    RWait *notifyWait;              /* Wait on IO */
#endif
    int wfd;                        /* Watch fd */
} Log;

typedef struct WalkContext {
    RList *list;
    cchar *filename;
    Time latest;
} WalkContext;

static Log *allocLog(Json *json, int id, cchar *path);
static void closeLog(Log *lp);
static void freeLog(Log *lp);
static int openLog(Log *lp);
static void readLog(Log *lp);
static void setWaitMask(Log *lp);
static int startLog(Log *lp);
static int startLogService(void);
static void logEvent(Log *lp);
#if LINUX && HAS_INOTIFY
static void logNotify(Log *lp, int mask, int fd);
#endif

/************************************* Code ***********************************/

PUBLIC int ioInitLogs(void)
{
    ioto->logs = rAllocList(0, 0);

    startLogService();
    return 0;
}

PUBLIC void ioTermLogs(void)
{
    Log *lp;
    int next;

    for (ITERATE_ITEMS(ioto->logs, lp, next)) {
        freeLog(lp);
    }
    rFreeList(ioto->logs);
    ioto->logs = 0;
}

static Log *allocLog(Json *json, int id, cchar *path)
{
    Log   *lp;
    cchar *group;
    char  *stream;
    bool  create;
    int   linger, maxEvents, maxSize;

    lp = rAllocType(Log);

    lp->command = jsonGet(json, id, "command", 0);
    lp->continuation = jsonGet(json, id, "continuation", " \t");
    lp->lines = jsonGetBool(json, id, "lines", lp->command ? 0 : 1);
    lp->tail = smatch(jsonGet(json, id, "position", "end"), "end");

#if LINUX && HAS_INOTIFY
    lp->notifyFd = -1;
#endif
    lp->wfd = -1;
    lp->path = sclone(path);

    create = jsonGetBool(json, id, "create", 1);
    maxEvents = jsonGetInt(json, id, "maxEvents", -1);
    maxSize = jsonGetInt(json, id, "maxSize", -1);
    linger = jsonGetInt(json, id, "linger", -1);
    group = jsonGet(json, id, "group", 0);

    ioSetTemplateVar("filename", rBasename(path));
    stream = ioExpand(jsonGet(json, id, "stream", "${hostname}-${filename}"));

    if ((lp->log = ioAllocLog(lp->path, ioto->awsRegion, create, group, stream, maxEvents, maxSize, linger)) == 0) {
        freeLog(lp);
        rFree(stream);
        return 0;
    }
    rFree(stream);

#if LINUX && HAS_INOTIFY
    if ((lp->notifyFd = inotify_init()) < 0) {
        rError("logs", "Cannot initialize inotify");
        freeLog(lp);
        return 0;
    }
    lp->notifyWait = rAllocWait(lp->notifyFd);
    rSetWaitHandler(lp->notifyWait, (RWaitProc) logNotify, lp, R_MODIFIED, 0, 0);
#endif
    return lp;
}

PUBLIC void freeLog(Log *lp)
{
    assert(lp);

    if (!lp) {
        return;
    }
    ioFreeLog(lp->log);
    if (lp->fp) {
        rFreeWait(lp->wait);
        if (lp->command) {
            pclose(lp->fp);
        } else {
            fclose(lp->fp);
        }
        lp->fp = 0;
    }
#if LINUX && HAS_INOTIFY
    if (lp->notifyFd >= 0) {
        rFreeWait(lp->notifyWait);
        // This automatically releases all watch descriptors
        close(lp->notifyFd);
        lp->notifyFd = -1;
    }
#endif
    rFree(lp->path);
    rFreeBuf(lp->buf);
    rFree(lp);
}

static int startLogService(void)
{
    Log      *lp;
    Json     *json;
    JsonNode *child, *files;
    RList    *list;
    cchar    *path;
    int      id, next;

    if (!ioto->logs) return 0;

    json = ioto->config;
    files = jsonGetNode(json, 0, "files");

    if (files) {
        for (ITERATE_JSON(json, files, child, id)) {
            if (jsonGetBool(json, id, "enable", 1) == 1) {
                path = jsonGet(json, id, "path", 0);
                list = rGetFiles("/", path, R_WALK_FILES);
                for (ITERATE_ITEMS(list, path, next)) {
                    if ((lp = allocLog(json, id, path)) != 0) {
                        rAddItem(ioto->logs, lp);
                    }
                }
            }
        }
    }
    for (ITERATE_ITEMS(ioto->logs, lp, next)) {
        startLog(lp);
    }
    return 0;
}

static int startLog(Log *lp)
{
    assert(lp);

#if LINUX && HAS_INOTIFY
    struct stat info;
    /*
        On Linux, delay opening the file until we get an inotify event.
        This scales better as we can watch many files without consuming a file descriptor.
        Note: this is a watch fd and not a real file descriptor.
     */
    if (lp->command) {
        if (openLog(lp) < 0) {
            return R_ERR_CANT_OPEN;
        }
    } else {
        if ((lp->wfd = inotify_add_watch(lp->notifyFd, lp->path, IN_CREATE | IN_MOVE | IN_MODIFY)) < 0) {
            int err = errno;
            if (rAccessFile(lp->path, R_OK) == 0) {
                rError("logs", "Cannot add watch for %s, errno %d", lp->path, err);
            }
        } else if (stat(lp->path, &info) == 0) {
            lp->inode = info.st_ino;
            lp->pos = info.st_size;
        }
    }
#else
    /*
        Need to open the file here as we use the open file descriptor on BSD to wait for I/O events
     */
    if (openLog(lp) < 0) {
        return R_ERR_CANT_OPEN;
    }
#endif
    return 0;
}

#if LINUX && HAS_INOTIFY
static void logNotify(Log *lp, int mask, int fd)
{
    struct inotify_event *event;
    char                 buf[ME_BUFSIZE];
    ssize                len;
    int                  i;

    assert(lp);

    len = read(fd, buf, sizeof(buf));
    for (i = 0; i < len; ) {
        event = (struct inotify_event*) &buf[i];
        if (lp->wfd == event->wd) {
            logEvent(lp);
            break;
        }
        i += sizeof(struct inotify_event) + event->len;
    }
}
#endif

static void logEvent(Log *lp)
{
    assert(lp);

#if LINUX && HAS_INOTIFY
    if (!lp->fp && openLog(lp) < 0) {
        return;
    }
#endif
    readLog(lp);
    setWaitMask(lp);
}

static void setWaitMask(Log *lp)
{
    if (lp->command) {
        rSetWaitMask(lp->wait, R_READABLE, lp->wait->deadline);
#if MACOSX
    } else {
        Ticks deadline = lp->wait ? lp->wait->deadline : 0;
        rSetWaitMask(lp->wait, ((int64) R_MODIFIED) | ((int64) NOTE_WRITE << 32) | R_READABLE, deadline);
#endif
    }
}

static int openLog(Log *lp)
{
    struct stat info;

    assert(lp);

    if (lp->command) {
        rTrace("logs", "Run command: %s", lp->command);
        assert(lp->fp == 0);
        /*
            SECURITY Acceptable:: The command is configured by device developer and is deemed secure.
         */
        if ((lp->fp = popen(lp->command, "r")) == 0) {
            rError("logs", "Cannnot open command \"%s\", errno %d", lp->command, errno);
            return R_ERR_CANT_OPEN;
        }
    } else {
        if (lp->fp) {
            if (ferror(lp->fp)) {
                fclose(lp->fp);
                lp->fp = 0;
            }
        }
        if (!lp->fp) {
            if ((lp->fp = fopen(lp->path, "r")) == 0) {
                //  Continue
                rTrace("logs", "Cannot open \"%s\"", lp->path);
                return 0;
            }
        }
        if (lp->pos == 0) {
            if (lp->tail) {
                fseek(lp->fp, 0, SEEK_END);
            } else {
                fseek(lp->fp, 0, SEEK_SET);
            }
        } else {
            // Check if the file is the same inode as the last open. If so, use the last know position
            if (fstat(fileno(lp->fp), &info) == 0 && info.st_ino == lp->inode) {
                if (fseek(lp->fp, lp->pos, SEEK_SET) < 0) {
                    fseek(lp->fp, 0, SEEK_END);
                }
            }
        }
        lp->pos = ftell(lp->fp);
        if (fstat(fileno(lp->fp), &info) == 0) {
            lp->inode = info.st_ino;
        }
    }
    if (!lp->buf) {
        lp->buf = rAllocBuf(ME_BUFSIZE);
    }
    lp->wait = rAllocWait(fileno(lp->fp));

    if (lp->command) {
        rSetWaitHandler(lp->wait, (RWaitProc) logEvent, lp, R_READABLE, 0, 0);
#if MACOSX
    } else {
        rSetWaitHandler(lp->wait, (RWaitProc) logEvent, lp,
                        ((int64) R_MODIFIED) | ((int64) NOTE_WRITE << 32) | R_READABLE, 0, 0);
#endif
    }
    return 0;
}

static void closeLog(Log *lp)
{
    struct stat info;
    int         fd, status;

    assert(lp);

    if (lp->fp) {
        if (lp->command) {
            fd = fileno(lp->fp);
            if ((status = pclose(lp->fp)) != 0) {
                rError("logs", "Bad exit status for command \"%s\", status %d, fd %d, errno %d ECHILD %d",
                       lp->command, status, fd, errno, ECHILD);
            }
        } else {
            lp->pos = ftell(lp->fp);
            fd = fileno(lp->fp);
            if (fstat(fd, &info) == 0) {
                lp->inode = info.st_ino;
            }
            rFreeWait(lp->wait);
            lp->wait = 0;
            fclose(lp->fp);
            lp->wfd = -1;
        }
        lp->fp = 0;
    }
}

static void readLog(Log *lp)
{
    RBuf  *buf;
    FILE  *fp;
    char  *eol, *pos, *sol;
    ssize nbytes;

    assert(lp);
    buf = lp->buf;
    assert(buf);
    fp = lp->fp;
    assert(fp);
    clearerr(fp);

    while (1) {
        if (rGetBufSpace(buf) < (ME_BUFSIZE / 2)) {
            rGrowBuf(buf, max(buf->buflen / 2, ME_BUFSIZE));
        }
        /*
            This will not block. This function is only ever called get here as the result of an I/O event.
            We test feof/ferror bbelow before we read more data.
         */
        if ((nbytes = fread(rGetBufEnd(buf), 1, (int) rGetBufSpace(buf) - 1, fp)) == 0) {
            break;
        }
        rAdjustBufEnd(buf, nbytes);
        rAddNullToBuf(buf);

        if (lp->lines) {
            eol = 0;
            for (sol = pos = rGetBufStart(buf); rGetBufLength(buf) > 0; pos = eol) {
                if ((eol = strchr(pos, '\n')) == 0) {
                    if (rGetBufLength(buf) < MAX_LINE) {
                        // No new line - need more data
                        break;
                    }
                    rGrowBuf(buf, 2);
                    eol = rGetBufEnd(buf);
                }
                if (eol[1] && schr(lp->continuation, eol[1]) != 0) {
                    // Continuation
                    eol++;
                    continue;
                }
                *eol++ = '\0';
                /*
                    Capture log message
                 */
                ioLogMessage(lp->log, 0, sol);
                rAdjustBufStart(buf, eol - sol);
                sol = eol;
            }
            // len = eol ? (eol - sol) : rGetBufLength(buf);
            rCompactBuf(buf);
        }
        if (ferror(fp) || feof(fp)) {
            if (rGetBufLength(buf) > 0 && !lp->lines) {
                rFlushBuf(buf);
            }
            break;
        }
    }
    if (ferror(fp) || (feof(fp) && lp->command)) {
        closeLog(lp);
    } else {
        lp->pos = ftell(lp->fp);
    }
}

#else
void dummyLogs(void)
{
}
#endif /* SERVICES_LOGS */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
