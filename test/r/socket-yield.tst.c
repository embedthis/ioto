/**
    socket-yield.tst.c - A fiber reading a socket that is always readable must share the event loop

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "testme.h"
#include "r.h"

#if ME_UNIX_LIKE
#include <signal.h>
#include <sys/wait.h>

/*********************************** Locals ***********************************/

#define READ_FOR        1500            //  Milliseconds the reader consumes the stream
#define TICK            20              //  Milliseconds between ticker events
#define MAX_GAP         250             //  Largest tolerated delay between ticks
#define WATCHDOG        20              //  Seconds before a hang is reported as a failure

static RSocket *listener;
static RSocket *server;
static pid_t   blaster;
static int     accepted;
static int     readerDone;
static ssize   totalRead;
static Ticks   lastTick, maxGap;
static int     ticks;

/************************************ Code ************************************/

static void watchdog(int sig)
{
    static cchar msg[] = "reader never yielded and never finished\n";

    (void) sig;
    (void) !write(2, msg, sizeof(msg) - 1);
    if (blaster > 0) {
        kill(blaster, SIGKILL);
    }
    _exit(1);
}

static void acceptFn(void *arg, RSocket *sp)
{
    (void) arg;
    server = sp;
    accepted = 1;
}


static void blast(int port)
{
    struct sockaddr_in addr;
    static char        chunk[1 << 18];
    int                fd;

    memset(chunk, 'a', sizeof(chunk));
    fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((ushort) port);
    if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        _exit(2);
    }
    alarm(30);
    while (send(fd, chunk, sizeof(chunk), 0) > 0) {
    }
    _exit(0);
}

//  Measures how long other work waits while the reader runs
static void tick(void *data)
{
    Ticks now, gap;

    now = rGetTicks();
    gap = now - lastTick;
    if (gap > maxGap) {
        maxGap = gap;
    }
    lastTick = now;
    ticks++;
    if (!readerDone) {
        rStartEvent((REventProc) tick, NULL, TICK);
    }
}

//  Consume the stream flat out for a fixed time. Every call finds data waiting.
static void reader(void *arg)
{
    char  buf[4096];
    Ticks end;
    ssize nbytes;

    (void) arg;
    end = rGetTicks() + READ_FOR;
    while (rGetTicks() < end) {
        nbytes = rReadSocket(server, buf, sizeof(buf), rGetTicks() + 5 * TPS);
        if (nbytes <= 0) {
            break;
        }
        totalRead += nbytes;
    }
    readerDone = 1;
}

static void testReaderYields(void)
{
    int   port;
    Ticks deadline;

    listener = rAllocSocket();
    for (port = 23100; port < 23199; port++) {
        if (rListenSocket(listener, "127.0.0.1", port, (RSocketProc) acceptFn, NULL) == 0) {
            break;
        }
    }
    ttrue(port < 23199);
    if (port >= 23199) {
        rFreeSocket(listener);
        return;
    }
    if ((blaster = fork()) == 0) {
        blast(port);
    }
    deadline = rGetTicks() + 2000;
    while (!accepted && rGetTicks() < deadline) {
        rSleep(10);
    }
    ttrue(accepted);

    signal(SIGALRM, watchdog);
    alarm(WATCHDOG);
    lastTick = rGetTicks();
    rStartEvent((REventProc) tick, NULL, TICK);
    rSpawnFiber("reader", reader, NULL);
    while (!readerDone) {
        rSleep(10);
    }
    alarm(0);

    tinfo("Read %lld bytes, %d ticks, longest gap %lld ms", (long long) totalRead, ticks, (long long) maxGap);
    ttrue(totalRead > 1024 * 1024);
    ttrue(ticks >= (READ_FOR / TICK) / 4);
    ttrue(maxGap < MAX_GAP);

    kill(blaster, SIGKILL);
    waitpid(blaster, NULL, 0);
    blaster = 0;
    rFreeSocket(server);
    rFreeSocket(listener);
}

static void fiberMain(void *arg)
{
    (void) arg;
    testReaderYields();
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

#else
int main(void)
{
    return 0;
}
#endif

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
