/**
    listen.tst.c - Test listen/accept with RWait

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "testme.h"
#include "r.h"

#if !ME_WIN_LIKE
/*********************************** Locals ***********************************/

#define TEST_PORT 19876

static RFiber *mainFiber;
static RWait  *listenWait;
static int    listenSock = -1;
static int    acceptCount = 0;

/***************************** Forward Declarations ***************************/

static void fiberMain(void *arg);

/************************************ Code ************************************/

static void acceptHandler(cvoid *arg, int mask)
{
    struct sockaddr_in addr;
    socklen_t          addrLen;
    int                clientSock;

    (void) arg;
    (void) mask;

    addrLen = sizeof(addr);
    clientSock = accept(listenSock, (struct sockaddr*) &addr, &addrLen);
    if (clientSock >= 0) {
        acceptCount++;
        close(clientSock);
    }
    // Mask should remain R_READABLE for next accept
    teqi(listenWait->mask, R_READABLE);

    rResumeFiber(mainFiber, (void*) "accepted");
}

static void testListenAccept(void)
{
    struct sockaddr_in addr;
    cchar              *result;
    int                clientSock, opt;

    mainFiber = rGetFiber();
    acceptCount = 0;

    // Create listening socket
    listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock < 0) {
        tfail();
        return;
    }
    opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    fcntl(listenSock, F_SETFL, O_NONBLOCK);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(TEST_PORT);

    if (bind(listenSock, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        close(listenSock);
        tfail();
        return;
    }
    if (listen(listenSock, 5) < 0) {
        close(listenSock);
        tfail();
        return;
    }

    // Set up wait handler for incoming connections
    listenWait = rAllocWait(listenSock);
    tnotnull(listenWait);

    rSetWaitHandler(listenWait, acceptHandler, NULL, R_READABLE, 0, 0);
    teqi(listenWait->mask, R_READABLE);

    // Connect a client
    clientSock = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSock < 0) {
        rFreeWait(listenWait);
        close(listenSock);
        tfail();
        return;
    }
    if (connect(clientSock, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        close(clientSock);
        rFreeWait(listenWait);
        close(listenSock);
        tfail();
        return;
    }
    close(clientSock);

    // Wait for accept handler
    result = rYieldFiber(0);
    tmatch(result, "accepted");
    teqi(acceptCount, 1);

    // Verify mask is still set for more accepts
    teqi(listenWait->mask, R_READABLE);

    rFreeWait(listenWait);
    close(listenSock);
}

static void fiberMain(void *arg)
{
    (void) arg;

    testListenAccept();

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
