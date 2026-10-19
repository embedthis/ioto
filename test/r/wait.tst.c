/**
    wait.tst.c - Unit tests for RWait persistent wait masks

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "testme.h"
#include "r.h"

#if !ME_WIN_LIKE

/*********************************** Locals ***********************************/

#define TIMEOUT (5 * 1000)

static RFiber *mainFiber;
static RWait  *testWait;
static int    pipeFd[2];
static int    handlerCalled = 0;
static int    freedRan = 0;
static int    freedMask = 0;

/************************************ Code ************************************/
/*
    Test: Simple listen/accept to verify events work
 */
static RSocket *testListen;
static RSocket *testClient;
static int     acceptCalled = 0;

static void testAcceptHandler(void *arg, RSocket *sp)
{
    (void) arg;
    acceptCalled++;
    tinfo("Accept handler called, acceptCalled=%d", acceptCalled);
    rFreeSocket(sp);
    rResumeFiber(mainFiber, (void*) "accepted");
}

static void testListenAccept(void)
{
    cchar *result;
    int   port, rc;

    mainFiber = rGetFiber();
    acceptCalled = 0;

    testListen = rAllocSocket();
    tnotnull(testListen);

    // Find a free port
    for (port = 9375; port < 9450; port++) {
        if (rListenSocket(testListen, "127.0.0.1", port, (RSocketProc) testAcceptHandler, NULL) == 0) {
            break;
        }
    }
    if (port >= 9450) {
        tfail();
        rFreeSocket(testListen);
        return;
    }
    tinfo("Listening on port %d", port);

    // Connect client
    testClient = rAllocSocket();
    tnotnull(testClient);
    rc = rConnectSocket(testClient, "127.0.0.1", port, rGetTicks() + TIMEOUT);
    ttrue(rc >= 0);
    if (rc < 0) {
        rFreeSocket(testClient);
        rFreeSocket(testListen);
        return;
    }
    tinfo("Client connected, waiting for accept handler...");

    // Wait for accept handler to be called
    result = rYieldFiber(0);
    tmatch(result, "accepted");
    teqi(acceptCalled, 1);

    tinfo("Listen/accept test passed");
    rFreeSocket(testClient);
    rFreeSocket(testListen);
}

/*
    Test 1: Basic wait handler triggers
 */
static void basicHandler(void *arg)
{
    (void) arg;
    handlerCalled++;
    tinfo("Basic handler called, handlerCalled=%d, mask=%d, eventMask=%d",
          handlerCalled, testWait->mask, testWait->eventMask);

    // Verify mask is still R_READABLE (not cleared by invokeHandler)
    teqi(testWait->mask, R_READABLE);

    rResumeFiber(mainFiber, (void*) "done");
}

static void testBasicWait(void)
{
    cchar *result;

    handlerCalled = 0;
    mainFiber = rGetFiber();

    if (pipe(pipeFd) < 0) {
        tfail();
        return;
    }
    fcntl(pipeFd[0], F_SETFL, O_NONBLOCK);
    fcntl(pipeFd[1], F_SETFL, O_NONBLOCK);

    testWait = rAllocWait(pipeFd[0]);
    tnotnull(testWait);

    tinfo("Setting wait handler for pipe fd=%d", pipeFd[0]);
    rSetWaitHandler(testWait, (RWaitProc) basicHandler, NULL, R_READABLE, 0, 0);
    teqi(testWait->mask, R_READABLE);

    // Write to trigger handler
    tinfo("Writing to pipe to trigger handler");
    write(pipeFd[1], "test", 4);

    // Wait for handler
    tinfo("Yielding to wait for handler");
    result = rYieldFiber(0);
    tmatch(result, "done");
    teqi(handlerCalled, 1);

    rFreeWait(testWait);
    close(pipeFd[0]);
    close(pipeFd[1]);
}

/*
    Test 2: Wakeup from foreign thread
    Verifies that rWakeup() can interrupt a blocked rWait() from another thread.
 */
static volatile int wakeupSent = 0;

static void *wakeupThreadProc(void *arg)
{
    (void) arg;
    usleep(50 * 1000);
    rWakeup();
    wakeupSent = 1;
    return NULL;
}

static void testWakeup(void)
{
    Ticks start, elapsed;

    tinfo("Testing rWakeup() from foreign thread");
    wakeupSent = 0;

    //  Start a thread that will call rWakeup() after 50ms
    if (rCreateThread("wakeup-thread", wakeupThreadProc, NULL) != 0) {
        tfail();
        return;
    }

    //  Wait with a long timeout (5 seconds) - should be woken up much sooner
    start = rGetTicks();
    rWait(rGetTicks() + 5000);
    elapsed = rGetTicks() - start;

    tinfo("rWait returned after %lld ms", (long long) elapsed);

    //  Should return within ~500ms (50ms sleep + overhead), not 5000ms
    ttrue(elapsed < 500);

    //  Wait for thread to complete
    while (!wakeupSent) {
        usleep(10 * 1000);
    }
}

/*
    Test 3: Early return optimization - setting same mask is a no-op
 */
static void testEarlyReturn(void)
{
    if (pipe(pipeFd) < 0) {
        tfail();
        return;
    }
    fcntl(pipeFd[0], F_SETFL, O_NONBLOCK);

    testWait = rAllocWait(pipeFd[0]);
    tnotnull(testWait);

    // Set initial mask
    rSetWaitMask(testWait, R_READABLE, 0);
    teqi(testWait->mask, R_READABLE);

    // Set same mask again - should be a no-op
    rSetWaitMask(testWait, R_READABLE, 0);
    teqi(testWait->mask, R_READABLE);

    // Change mask
    rSetWaitMask(testWait, R_WRITABLE, 0);
    teqi(testWait->mask, R_WRITABLE);

    // Clear mask
    rSetWaitMask(testWait, 0, 0);
    teqi(testWait->mask, 0);

    rFreeWait(testWait);
    close(pipeFd[0]);
    close(pipeFd[1]);
}

/*
    The TestMe comparison macros expand their arguments twice, so every call below with a side
    effect is bound to a local before it is asserted.
 */

/*
    The notifier descriptor. An embedder integrating the runtime into another event loop selects
    on it, so it must be a real descriptor for the whole life of the runtime.
 */
static void testWaitFd(void)
{
    int fd, again;

    fd = rGetWaitFd();
#if ME_EVENT_NOTIFIER == R_EVENT_KQUEUE || ME_EVENT_NOTIFIER == R_EVENT_EPOLL
    ttrue(fd >= 0);

    /*
        Stable - an embedder registers it once.
     */
    again = rGetWaitFd();
    teqi(again, fd);
#else
    //  select() has no backing descriptor
    (void) again;
    ttrue(fd == -1 || fd >= 0);
#endif
}


/*
    Changing a wait mask has to survive being set to the same value, cleared, and set again.
    rSetWaitMask short-circuits when the mask is unchanged, so the repeats are the point.
 */
static void testWaitMaskTransitions(void)
{
    RWait *wp;
    int   fds[2], rc;

    rc = pipe(fds);
    teqi(rc, 0);

    wp = rAllocWait(fds[0]);
    tnotnull(wp);

    rSetWaitMask(wp, R_READABLE, 0);
    teqi((int) wp->mask, R_READABLE);

    /*
        The same mask again is a no-op, not a duplicate registration.
     */
    rSetWaitMask(wp, R_READABLE, 0);
    teqi((int) wp->mask, R_READABLE);

    rSetWaitMask(wp, R_READABLE | R_WRITABLE, 0);
    teqi((int) wp->mask, R_READABLE | R_WRITABLE);

    rSetWaitMask(wp, 0, 0);
    teqi((int) wp->mask, 0);

    /*
        And back on again after being cleared.
     */
    rSetWaitMask(wp, R_WRITABLE, 0);
    teqi((int) wp->mask, R_WRITABLE);

    /*
        A null wait object is ignored rather than dereferenced.
     */
    rSetWaitMask(NULL, R_READABLE, 0);

    rFreeWait(wp);
    close(fds[0]);
    close(fds[1]);
}


static void waitBlockFiber(void *arg)
{
    RWait *wp = (RWait*) arg;
    int   mask;

    /*
        Block until something resumes this fiber. rFreeWait must be that something.
     */
    mask = rWaitForIO(wp, R_READABLE, rGetTicks() + 10 * TPS);
    freedMask = mask;
    freedRan = 1;
}


/*
    Freeing a wait object must resume any fiber blocked on it rather than leaving it parked
    until its deadline. The fiber is told which conditions it is being released for.
 */
static void testFreeWaitResumesFiber(void)
{
    RWait  *wp;
    RFiber *fiber;
    int    fds[2], rc;

    rc = pipe(fds);
    teqi(rc, 0);

    wp = rAllocWait(fds[0]);
    tnotnull(wp);

    freedRan = 0;
    freedMask = 0;

    fiber = rAllocFiber("wait-block", waitBlockFiber, wp);
    tnotnull(fiber);
    rStartFiber(fiber, NULL);

    /*
        Let it reach the wait.
     */
    rSleep(50);
    teqi(freedRan, 0);

    rFreeWait(wp);
    rSleep(50);

    teqi(freedRan, 1);
    ttrue(freedMask != 0);

    close(fds[0]);
    close(fds[1]);
}


/*
    A wait handler registered with a deadline must be armed with it. The deadline is the only
    thing that reclaims an idle handler-driven wait, so discarding it makes such a wait immortal.
 */
static void deadlineHandler(void *arg)
{
    (void) arg;
}

static void testWaitHandlerDeadlineArmed(void)
{
    RWait *wp;
    Ticks deadline;
    int   fds[2], rc;

    rc = pipe(fds);
    teqi(rc, 0);

    wp = rAllocWait(fds[0]);
    tnotnull(wp);

    deadline = rGetTicks() + 60 * TPS;
    rSetWaitHandler(wp, (RWaitProc) deadlineHandler, NULL, R_READABLE, deadline, 0);
    ttrue(wp->deadline == deadline);

    /*
        Re-arming with an unchanged mask must still move the deadline. rSetWaitMask short-circuits
        on the mask, so the deadline update has to sit before that early return.
     */
    deadline = rGetTicks() + 90 * TPS;
    rSetWaitHandler(wp, (RWaitProc) deadlineHandler, NULL, R_READABLE, deadline, 0);
    ttrue(wp->deadline == deadline);

    //  And a zero deadline disarms it again
    rSetWaitHandler(wp, (RWaitProc) deadlineHandler, NULL, R_READABLE, 0, 0);
    ttrue(wp->deadline == 0);

    rFreeWait(wp);
    close(fds[0]);
    close(fds[1]);
}

/*
    Test: a handler-driven wait that never becomes readable is invoked with R_TIMEOUT
 */
static RWait *timeoutWait;
static int   timeoutCalled = 0;
static int   timeoutEventMask = 0;

static void timeoutHandler(void *arg)
{
    (void) arg;
    timeoutCalled++;
    timeoutEventMask = timeoutWait->eventMask;
    //  Disarm so an unhandled deadline cannot re-fire on every loop iteration
    rSetWaitMask(timeoutWait, 0, 0);
}

static void testWaitHandlerTimeoutFires(void)
{
    int fds[2], rc;

    rc = pipe(fds);
    teqi(rc, 0);
    fcntl(fds[0], F_SETFL, O_NONBLOCK);

    timeoutCalled = 0;
    timeoutEventMask = 0;

    timeoutWait = rAllocWait(fds[0]);
    tnotnull(timeoutWait);

    //  Nothing is ever written to the pipe, so only the deadline can invoke the handler
    rSetWaitHandler(timeoutWait, (RWaitProc) timeoutHandler, NULL, R_READABLE, rGetTicks() + 200, 0);

    rSleep(1000);

    teqi(timeoutCalled, 1);
    ttrue(timeoutEventMask & R_TIMEOUT);

    rFreeWait(timeoutWait);
    timeoutWait = 0;
    close(fds[0]);
    close(fds[1]);
}


static int   staleFds[2][2];
static RWait *staleWaits[2];
static int   reusedFds[2];
static RWait *reusedWait;
static int   reusedNumber;
static int   staleHandled;
static int   reusedCalled;

//  Handlers drain their pipe: epoll is level-triggered and would otherwise report it again
static void reusedHandler(void *arg)
{
    char buf[8];

    (void) arg;
    reusedCalled++;
    (void) read(reusedFds[0], buf, sizeof(buf));
}

static void staleHandler(void *arg)
{
    char buf[8];
    int  which, other;

    which = (int) (ssize) arg;
    (void) read(staleFds[which][0], buf, sizeof(buf));
    if (staleHandled++) {
        return;
    }
    other = !which;
    reusedNumber = staleFds[other][0];
    rFreeWait(staleWaits[other]);
    staleWaits[other] = 0;
    close(staleFds[other][0]);
    if (pipe(reusedFds) < 0) {
        return;
    }
    fcntl(reusedFds[0], F_SETFL, O_NONBLOCK);
    reusedWait = rAllocWait(reusedFds[0]);
    rSetWaitHandler(reusedWait, (RWaitProc) reusedHandler, NULL, R_READABLE, 0, 0);
}

static void testStaleEventForReusedFd(void)
{
    int i;

    staleHandled = reusedCalled = 0;
    reusedWait = 0;
    for (i = 0; i < 2; i++) {
        if (pipe(staleFds[i]) < 0) {
            tfail();
            return;
        }
        fcntl(staleFds[i][0], F_SETFL, O_NONBLOCK);
        staleWaits[i] = rAllocWait(staleFds[i][0]);
        rSetWaitHandler(staleWaits[i], (RWaitProc) staleHandler, (void*) (ssize) i, R_READABLE, 0, 0);
    }
    //  Both ready before the next wait, so both events arrive in one batch
    write(staleFds[0][1], "x", 1);
    write(staleFds[1][1], "x", 1);
    rSleep(200);

    ttrue(staleHandled >= 1);
    ttrue(reusedWait != NULL);
    if (reusedWait && reusedFds[0] == reusedNumber) {
        teqi(reusedCalled, 0);

        //  A real event on the new descriptor is still delivered
        write(reusedFds[1], "y", 1);
        rSleep(200);
        teqi(reusedCalled, 1);
    } else {
        tinfo("New pipe did not reuse descriptor %d, cannot test", reusedNumber);
    }
    for (i = 0; i < 2; i++) {
        if (staleWaits[i]) {
            rFreeWait(staleWaits[i]);
            close(staleFds[i][0]);
        }
        close(staleFds[i][1]);
    }
    if (reusedWait) {
        rFreeWait(reusedWait);
        close(reusedFds[0]);
        close(reusedFds[1]);
    }
}


#define PAIR_BIG    (8 * 1024 * 1024)

typedef struct Pair {
    RSocket *listener;
    RSocket *server;                    //  Accepted end that the reader and writer fibers use
    RSocket *client;                    //  Test end
    ssize   readResult[2];
    ssize   writeResult;
    int     reads;                      //  Completed reads
    int     writeDone;
    int     readerDone;
    int     accepted;
    int     port;
} Pair;

static Pair pair;

static void pairAccept(void *arg, RSocket *sp)
{
    (void) arg;
    pair.server = sp;
    pair.accepted = 1;
}


static bool pairWait(int *flag, int milliseconds)
{
    Ticks deadline;

    deadline = rGetTicks() + milliseconds;
    while (!*flag && rGetTicks() < deadline) {
        rSleep(10);
    }
    return *flag != 0;
}

static int pairOpen(void)
{
    int port;

    memset(&pair, 0, sizeof(pair));
    pair.listener = rAllocSocket();
    for (port = 23100; port < 23199; port++) {
        if (rListenSocket(pair.listener, "127.0.0.1", port, (RSocketProc) pairAccept, NULL) == 0) {
            break;
        }
    }
    if (port >= 23199) {
        rFreeSocket(pair.listener);
        pair.listener = 0;
        return R_ERR_CANT_OPEN;
    }
    pair.port = port;
    pair.client = rAllocSocket();
    if (rConnectSocket(pair.client, "127.0.0.1", port, rGetTicks() + TIMEOUT) < 0 || !pairWait(&pair.accepted, 2000)) {
        rFreeSocket(pair.client);
        rFreeSocket(pair.listener);
        pair.client = pair.listener = 0;
        return R_ERR_CANT_CONNECT;
    }
    return 0;
}

static void pairClose(void)
{
    rFreeSocket(pair.server);
    rFreeSocket(pair.client);
    rFreeSocket(pair.listener);
}

static void pairReader(void *arg)
{
    char buf[16];
    int  i;

    (void) arg;
    for (i = 0; i < 2; i++) {
        pair.readResult[i] = rReadSocket(pair.server, buf, sizeof(buf), rGetTicks() + 20 * TPS);
        pair.reads++;
        if (pair.readResult[i] <= 0) {
            break;
        }
    }
    pair.readerDone = 1;
}

static void pairWriter(void *arg)
{
    char *data;

    (void) arg;
    data = rAlloc(PAIR_BIG);
    memset(data, 'w', PAIR_BIG);
    pair.writeResult = rWriteSocket(pair.server, data, PAIR_BIG, rGetTicks() + 20 * TPS);
    rFree(data);
    pair.writeDone = 1;
}

//  Read and discard everything the client has until the writer finishes
static void pairDrain(void)
{
    char    buf[65536];
    ssize   nbytes;
    Ticks   deadline;

    deadline = rGetTicks() + 10 * TPS;
    while (!pair.writeDone && rGetTicks() < deadline) {
        nbytes = rReadSocketSync(pair.client, buf, sizeof(buf));
        if (nbytes <= 0) {
            rSleep(5);
        }
    }
}

static void testReaderAndWriterWaitTogether(void)
{
    ssize rc;

    if (pairOpen() < 0) {
        tfail();
        return;
    }
    rSpawnFiber("pair-reader", pairReader, NULL);
    rSleep(50);
    rSpawnFiber("pair-writer", pairWriter, NULL);
    rSleep(100);
    teqi(pair.writeDone, 0);

    //  The reader must be woken by data while the writer is still blocked
    rc = rWriteSocket(pair.client, "x", 1, rGetTicks() + TIMEOUT);
    teqz(rc, 1);
    ttrue(pairWait(&pair.reads, 2000));
    teqz(pair.readResult[0], 1);
    teqi(pair.writeDone, 0);

    //  Draining the client lets the writer finish
    pairDrain();
    ttrue(pairWait(&pair.writeDone, 5000));
    teqz(pair.writeResult, PAIR_BIG);

    //  The writer's completion must leave the reader registered
    rc = rWriteSocket(pair.client, "y", 1, rGetTicks() + TIMEOUT);
    teqz(rc, 1);
    if (!pairWait(&pair.readerDone, 2000)) {
        //  Reader stranded. The wait object may already be gone, so leave the sockets alone.
        tfail();
        return;
    }
    teqz(pair.readResult[1], 1);
    pairClose();
}

//  Closing the socket must release both waiters
static void testCloseReleasesReaderAndWriter(void)
{
    if (pairOpen() < 0) {
        tfail();
        return;
    }
    rSpawnFiber("pair-reader", pairReader, NULL);
    rSleep(50);
    rSpawnFiber("pair-writer", pairWriter, NULL);
    rSleep(100);
    teqi(pair.reads, 0);
    teqi(pair.writeDone, 0);

    rCloseSocket(pair.server);
    ttrue(pairWait(&pair.writeDone, 2000));
    ttrue(pairWait(&pair.readerDone, 2000));
    ttrue(pair.writeResult < 0);
    ttrue(pair.readResult[0] < 0);
    pairClose();
}

//  A second waiter on the same side is refused, and the first is undisturbed
static int secondReadResult = -1;

static void secondReader(void *arg)
{
    secondReadResult = rWaitForIO((RWait*) arg, R_READABLE, rGetTicks() + 5 * TPS);
}

static void testSecondWaiterRefused(void)
{
    ssize rc;

    if (pairOpen() < 0) {
        tfail();
        return;
    }
    secondReadResult = -1;
    rSpawnFiber("pair-reader", pairReader, NULL);
    rSleep(50);
    rSpawnFiber("pair-second", secondReader, rGetSocketWait(pair.server));
    rSleep(50);
    teqi(secondReadResult, 0);

    rc = rWriteSocket(pair.client, "x", 1, rGetTicks() + TIMEOUT);
    teqz(rc, 1);
    ttrue(pairWait(&pair.reads, 2000));
    teqz(pair.readResult[0], 1);

    //  Let the reader's second read finish by closing
    rCloseSocket(pair.server);
    ttrue(pairWait(&pair.readerDone, 2000));
    pairClose();
}

/* Deadlines must fire while other descriptors remain busy. */
#define BUSY_DEADLINE   300             //  Milliseconds until the quiet descriptor times out
#define BUSY_SLACK      150             //  Allowed lateness
#define BUSY_RUN        1500            //  Milliseconds the noise thread runs for
#define BUSY_PIPES      8
#define BUSY_THREADS    4

static int           busyNoise[BUSY_PIPES][2], busyQuiet[2];
static RWait         *busyWaits[BUSY_PIPES];
static volatile int  busyStop, busyThreadsDone;
static Ticks         busyFired, busyStart;
static int           busyMask = -1;
static int           busyDrained;

static void *busyThreadProc(void *arg)
{
    Ticks  end;
    char   byte = 'n';
    int    i;

    (void) arg;
    end = rGetTicks() + BUSY_RUN;


    while (!busyStop && rGetTicks() < end) {
        for (i = 0; i < BUSY_PIPES; i++) {
            (void) !write(busyNoise[i][1], &byte, 1);
        }
    }
    __sync_fetch_and_add(&busyThreadsDone, 1);
    return NULL;
}

static void busyNoiseHandler(void *arg, int mask)
{
    char buf[256];
    int  fd = (int) (ssize) arg;

    (void) mask;
    while (read(fd, buf, sizeof(buf)) > 0) {
        busyDrained++;
    }
}

static void busyQuietHandler(void *arg, int mask)
{
    RWait *wp = (RWait*) arg;

    if (!busyFired) {
        busyFired = rGetTicks();
        busyMask = mask;
    }
    rSetWaitMask(wp, 0, 0);
}

static void testDeadlineFiresUnderTraffic(void)
{
    RWait *quietWait;
    Ticks elapsed, limit;
    int   i, rc;

    for (i = 0; i < BUSY_PIPES; i++) {
        rc = pipe(busyNoise[i]);
        teqi(rc, 0);
        fcntl(busyNoise[i][0], F_SETFL, fcntl(busyNoise[i][0], F_GETFL) | O_NONBLOCK);
        fcntl(busyNoise[i][1], F_SETFL, fcntl(busyNoise[i][1], F_GETFL) | O_NONBLOCK);
        busyWaits[i] = rAllocWait(busyNoise[i][0]);
        rSetWaitHandler(busyWaits[i], (RWaitProc) busyNoiseHandler, (void*) (ssize) busyNoise[i][0], R_READABLE, 0,
                        R_WAIT_MAIN_FIBER);
    }
    rc = pipe(busyQuiet);
    teqi(rc, 0);
    fcntl(busyQuiet[0], F_SETFL, fcntl(busyQuiet[0], F_GETFL) | O_NONBLOCK);

    busyStop = busyThreadsDone = 0;
    busyFired = 0;
    busyMask = -1;
    busyDrained = 0;

    quietWait = rAllocWait(busyQuiet[0]);
    busyStart = rGetTicks();
    rSetWaitHandler(quietWait, (RWaitProc) busyQuietHandler, quietWait, R_READABLE, busyStart + BUSY_DEADLINE,
                    R_WAIT_MAIN_FIBER);

    for (i = 0; i < BUSY_THREADS; i++) {
        if (rCreateThread("busy-noise", busyThreadProc, NULL) != 0) {
            tfail();
            return;
        }
    }
    //  Bounded: the noise stops by itself, and the check below runs before then
    limit = busyStart + BUSY_RUN + 500;
    while (!busyFired && rGetTicks() < limit) {
        rSleep(10);
    }
    busyStop = 1;
    while (busyThreadsDone < BUSY_THREADS) {
        usleep(1000);
    }
    ttrue(busyDrained > 100);
    ttrue(busyFired != 0);
    elapsed = busyFired - busyStart;
    tinfo("Deadline of %d ms fired after %lld ms under traffic", BUSY_DEADLINE, (long long) elapsed);
    ttrue(elapsed >= BUSY_DEADLINE - 10);
    ttrue(elapsed < BUSY_DEADLINE + BUSY_SLACK);
    teqi(busyMask, 0);

    for (i = 0; i < BUSY_PIPES; i++) {
        rFreeWait(busyWaits[i]);
        close(busyNoise[i][0]);
        close(busyNoise[i][1]);
    }
    rFreeWait(quietWait);
    close(busyQuiet[0]);
    close(busyQuiet[1]);
}

static void fiberMain(void *arg)
{
    (void) arg;

    testListenAccept();
    testBasicWait();
    testWakeup();
    testEarlyReturn();

    testWaitFd();
    testWaitMaskTransitions();
    testFreeWaitResumesFiber();
    testWaitHandlerDeadlineArmed();
    testWaitHandlerTimeoutFires();
    testStaleEventForReusedFd();
    testReaderAndWriterWaitTogether();
    testCloseReleasesReaderAndWriter();
    testSecondWaiterRefused();
    testDeadlineFiresUnderTraffic();

    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

#else /* WIN */

int main(void)
{
    return 0;
}

#endif /* !WIN */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
