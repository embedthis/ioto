/*
    connect.tst.c - Connection stress test

    Loops forever creating raw socket connections to localhost:4260,
    closing each connection in an orderly fashion.
    Prints counter and TIME_WAIT count every 1000 connections.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "bench-test.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/*********************************** Defines **********************************/

#define CONNECT_HOST    "127.0.0.1"
#define CONNECT_PORT    4260
#define REPORT_INTERVAL 100

/************************************ Code ************************************/

/*
    Get TIME_WAIT socket count for specified port
 */
static int getTimeWaitCount(int port)
{
    FILE *fp;
    char cmd[256], line[256];
    int  count;

    count = 0;
    if (port > 0) {
        // macOS uses .port format, Linux uses :port format
        snprintf(cmd, sizeof(cmd), "netstat -an 2>/dev/null | grep '[.:]%d.*TIME_WAIT' 2>/dev/null | wc -l", port);
    } else {
        snprintf(cmd, sizeof(cmd), "netstat -an 2>/dev/null | grep 'TIME_WAIT' 2>/dev/null | wc -l");
    }
    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp)) {
            count = atoi(line);
        }
        pclose(fp);
    }
    return count;
}

/*
    Connect and close socket in orderly fashion
    Returns 0 on success, -1 on error
 */
static int connectAndClose(void)
{
    struct sockaddr_in addr;
    int                fd, rc, one;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    fcntl(fd, F_SETFL, O_NONBLOCK);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CONNECT_PORT);
    inet_pton(AF_INET, CONNECT_HOST, &addr.sin_addr);

    rc = connect(fd, (struct sockaddr*) &addr, sizeof(addr));
    if (rc < 0 && errno != EINPROGRESS) {
        close(fd);
        return -1;
    }
    if (rc < 0) {
        fd_set         writefds;
        struct timeval tv;

        FD_ZERO(&writefds);
        FD_SET(fd, &writefds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        rc = select(fd + 1, NULL, &writefds, NULL, &tv);
        if (rc <= 0) {
            close(fd);
            return -1;
        }
        int       error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            close(fd);
            return -1;
        }
    }
    fcntl(fd, F_SETFL, 0);

    one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    // Orderly close: shutdown first, then close
    shutdown(fd, SHUT_RDWR);
    close(fd);

    return 0;
}

/*
    Connection stress test
 */
static void connectStress(void)
{
    int64 count, errors, startTime;
    int   timeWaits, rc;

    tinfo("Connection stress test to %s:%d\n", CONNECT_HOST, CONNECT_PORT);
    tinfo("Press Ctrl+C to stop\n\n");

    count = 0;
    errors = 0;
    startTime = rGetTicks();

    while (rState < R_STOPPING) {
        rc = connectAndClose();
        if (rc < 0) {
            errors++;
            if (errors <= 5) {
                tinfo("Connection error at count %lld (errno %d)\n", (long long) count, errno);
            }
            rSleep(10);
        } else {
            count++;
        }
        if (count > 0 && (count % REPORT_INTERVAL) == 0) {
            timeWaits = getTimeWaitCount(CONNECT_PORT);
            tinfo("Connections: %lld, TIME_WAITs: %d, Errors: %lld, Rate: %.0f/sec",
                  (long long) count, timeWaits, (long long) errors,
                  (count * 1000.0) / (rGetTicks() - startTime));
        }
    }
}

/*
    Main fiber entry point
 */
static void fiberMain(void *data)
{
    connectStress();
    rStop();
}

int main(int argc, char *argv[])
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
