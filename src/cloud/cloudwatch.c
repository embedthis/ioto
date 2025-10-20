/*
    cloudwatch.c - Cloud based logging to CloudWatch logs

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/*********************************** Includes *********************************/

#include "ioto.h"

#if SERVICES_CLOUD
/*********************************** Defines **********************************/

#define DEFAULT_BUF_SIZE   1024
#define DEFAULT_LINGER     (5 * TPS)
#define MAX_LINGER         (3600 * TPS)
#define AWS_EVENT_OVERHEAD 26

/*
    AWS max is 10K
 */
#define MAX_AWS_EVENTS     1000

/*
    AWS max is 1MB
 */
#define MAX_AWS_BUF_SIZE   (256 * 1024)
#define MAX_BUFFERS        4

static RBuf *logBuf;

/*********************************** Forwards *********************************/

static void bufferTimeout(IotoLog *log);
static int commitMessage(IotoLog *log);
static void flushBuf(IotoLog *log);
static int finalizeBuf(IotoLog *log);
static int getLogGroup(IotoLog *log);
static int getLogStream(IotoLog *log);
static void logMessageLine(IotoLog *log, cchar *value);
static void logHandler(cchar *type, cchar *source, cchar *msg);
static int logMessageEnd(IotoLog *log);
static int logMessageStart(IotoLog *log, Time time);
static void prepareBuf(IotoLog *log);
static void queueBuf(IotoLog *log);
static void serviceQueue(IotoLog *log, int count);
static void startTimeout(IotoLog *log);
static void stopTimeout(IotoLog *log);

/************************************* Code ***********************************/

PUBLIC IotoLog *ioAllocLog(cchar *path, cchar *region, int create, cchar *group, cchar *stream,
                           int maxEvents, int size, Ticks linger)
{
    IotoLog *log;

    assert(group && *group);

    log = rAllocType(IotoLog);
    if (maxEvents <= 0 || maxEvents > MAX_AWS_EVENTS) {
        maxEvents = MAX_AWS_EVENTS;
    }
    if (size <= 0 || size > MAX_AWS_BUF_SIZE) {
        size = MAX_AWS_BUF_SIZE;
    }
    if (linger < 0) {
        linger = DEFAULT_LINGER;
    }
    if (linger > MAX_LINGER) {
        linger = MAX_LINGER;
    }
    log->path = sclone(path);
    log->region = sclone(region);
    log->group = sclone(group);
    log->stream = sclone(stream);
    log->buffers = rAllocList(0, 0);
    log->create = create;

    // Set HIW to 80% to leave room to finalize buffer before sending
    log->eventsHiw = maxEvents / 100 * 80;
    log->maxEvents = maxEvents;
    log->max = size - 3;
    log->hiw = size / 100 * 80;
    log->linger = linger;

    prepareBuf(log);

    if (getLogGroup(log) < 0) {
        ioFreeLog(log);
        return 0;
    }
    return log;
}

PUBLIC void ioFreeLog(IotoLog *log)
{
    RBuf *buf;
    int  i;

    if (!log) {
        return;
    }
    for (ITERATE_ITEMS(log->buffers, buf, i)) {
        rFreeBuf(buf);
    }
    rFreeList(log->buffers);
    rFreeBuf(log->buf);
    rStopEvent(log->event);
    rFree(log->path);
    rFree(log->region);
    rFree(log->group);
    rFree(log->stream);
    rFree(log->sequence);
    rFree(log);
}

PUBLIC int ioEnableCloudLog(void)
{
    cchar *group;
    char  *stream;

    logBuf = rAllocBuf(256);
    group = jsonGet(ioto->config, 0, "log.group", IO_LOG_GROUP);
    stream = ioExpand(jsonGet(ioto->config, 0, "log.stream", IO_LOG_STREAM));

    ioto->log = ioAllocLog("ioto", ioto->awsRegion, 1, group, stream, -1, -1, -1);
    rSetLogHandler(logHandler);
    rFree(stream);
    return 0;
}

static void logHandler(cchar *type, cchar *source, cchar *msg)
{
    cchar *str;

    if (rEmitLog(type, source)) {
        rFormatLog(logBuf, type, source, msg);
        str = rBufToString(logBuf);
        if (ioto->log) {
            ioLogMessage(ioto->log, 0, msg);
        } else {
            write(rGetLogFile(), str, (uint) rGetBufLength(logBuf));
        }
    }
}

/*
    Log a single message
 */
PUBLIC int ioLogMessage(IotoLog *log, Time time, cchar *msg)
{
    if (!log) {
        return R_ERR_BAD_STATE;
    }
    if (logMessageStart(log, time) < 0) {
        return R_ERR_TIMEOUT;
    }
    logMessageLine(log, msg);
    return logMessageEnd(log);
}

static int logMessageStart(IotoLog *log, Time time)
{
    Time now;

    //  Do not use asserts here
    if (!log || !log->buf) {
        return R_ERR_BAD_STATE;
    }
    if (!ioto->awsAccess) {
        rError("log", "AWS keys not configured for CloudWatch logging");
        return R_ERR_NOT_READY;
    }
    now = rGetTime();
    if (time == 0) {
        time = now;

    } else if (time > (now + 2 * 3600 * TPS) || time < (now - (14 * 86400 * TPS) + 3600 * TPS)) {
        // Ignore events more than 2 hrs in the future or almost 14 days old
        rTrace("log", "Ignore out of range event %s", rFormatLocalTime(NULL, time));
        return R_ERR_TIMEOUT;
    }
    if (!log->bufStarted) {
        log->bufStarted = time;

#if KEEP
    } else if ((time - log->bufStarted) >= (23 * 3600 * TPS)) {
        //  Message is more than 23 hours after first message in buffer. AWS won't accept a span of > 24 hours.
        flushBuf(log);
#endif
    }
    rPutToBuf(log->buf, "{\"timestamp\":%lld,\"message\":", time);
    return 0;
}

/*
    Add a message line to the buffer
 */
static void logMessageLine(IotoLog *log, cchar *value)
{
    //  Do not use asserts here
    if (!log || !log->buf) {
        return;
    }
    jsonPutValueToBuf(log->buf, value, JSON_JSON);
}

static int logMessageEnd(IotoLog *log)
{
    //  Do not use asserts here
    if (!log || !log->buf) {
        return R_ERR_BAD_STATE;
    }
    log->events++;
    rPutStringToBuf(log->buf, "},");
    return commitMessage(log);
}

/*
    Commit a message to AWS
 */
static int commitMessage(IotoLog *log)
{
    if (!log || !log->buf) {
        return R_ERR_BAD_STATE;
    }
    if (log->events >= log->eventsHiw || (int) rGetBufLength(log->buf) >= log->hiw) {
        flushBuf(log);
        return 0;
    }
    startTimeout(log);
    return 0;
}

static void startTimeout(IotoLog *log)
{
    assert(log);

    if (!log->event) {
        log->event = rStartEvent((REventProc) bufferTimeout, log, log->linger);
    }
}

static void stopTimeout(IotoLog *log)
{
    if (log->event) {
        rStopEvent(log->event);
        log->event = 0;
    }
}

static void bufferTimeout(IotoLog *log)
{
    assert(log);

    if (log->event) {
        log->event = 0;
        flushBuf(log);
    }
}

static void flushBuf(IotoLog *log)
{
    if (!log->sending) {
        stopTimeout(log);
        finalizeBuf(log);
        queueBuf(log);
    }
}

static void queueBuf(IotoLog *log)
{
    RBuf *buf;

    //  Must immediately create a new buffer to capture any messages from here
    buf = log->buf;
    log->buf = 0;
    prepareBuf(log);

    if (rGetListLength(log->buffers) >= MAX_BUFFERS) {
        rDebug("log", "Discarding buffer due to queue overflow %d/%d", rGetListLength(log->buffers), MAX_BUFFERS);
        rFreeBuf(buf);
    } else {
        rPushItem(log->buffers, buf);
        serviceQueue(log, 0);
    }
}

static void serviceQueue(IotoLog *log, int count)
{
    RBuf   *buf;
    Url    *up;
    Json   *json;
    cchar  *data;
    size_t len;
    int    status;

    assert(log);

    if (log->sending || ((buf = rPopItem(log->buffers)) == 0)) {
        return;
    }
    if (++count > 10) {
        return;
    }
    log->sending = buf;

    data = rBufToString(buf);
    len = rGetBufLength(buf);
    up = urlAlloc(0);

    // print("Sending log data for %s to cloudwatch %s %s/%s", log->path, log->region, log->group, log->stream);

    status = aws(up, ioto->awsRegion, "logs", "Logs_20140328.PutLogEvents", data, len, 0);

    if (status != URL_CODE_OK) {
        rError("log", "AWS request error, status code %d, response %s", up->status, urlGetResponse(up));
        //  Try to repair
        if (up->status == URL_CODE_BAD_REQUEST && scontains("Bad sequence", up->rx->start)) {
            getLogGroup(log);
        }
    } else {
        if ((json = urlGetJsonResponse(up)) == 0) {
            rError("log", "Cannot parse AWS response for log message: %s\n", urlGetResponse(up));
        } else {
            rFree(log->sequence);
            log->sequence = jsonGetClone(json, 0, "nextSequenceToken", 0);
            jsonFree(json);
        }
    }
    urlFree(up);

    rFreeBuf(log->sending);
    log->sending = 0;
    serviceQueue(log, count);
}

static void prepareBuf(IotoLog *log)
{
    assert(log);

    if (!log->buf) {
        log->buf = rAllocBuf(DEFAULT_BUF_SIZE);
    } else {
        rFlushBuf(log->buf);
    }
    log->events = 0;
    log->bufStarted = 0;
    rPutStringToBuf(log->buf, "{\"logEvents\":[");
}

static int finalizeBuf(IotoLog *log)
{
    RBuf *buf;

    assert(!log->sending);

    buf = log->buf;

    // Erase trailing comma after last event (JSON Ugh!)
    rAdjustBufEnd(buf, -1);

    if (log->sequence && *log->sequence) {
        rPutToBuf(log->buf, "],\"logGroupName\":\"%s\",\"logStreamName\":\"%s\",\"sequenceToken\":\"%s\"}",
                  log->group, log->stream, log->sequence);
    } else {
        rPutToBuf(log->buf, "],\"logGroupName\":\"%s\",\"logStreamName\":\"%s\"}", log->group, log->stream);
    }
    return 0;
}

static int createLogGroup(IotoLog *log)
{
    Url  *up;
    char *data;
    int  status;

    assert(log);

    up = urlAlloc(0);
    data = sfmt("{\"logGroupName\":\"%s\"}", log->group);

    status = aws(up, log->region, "logs", "Logs_20140328.CreateLogGroup", data, 0, NULL);

    rFree(data);

    if (status != URL_CODE_OK) {
        rError("log", "Cannot create group %s, %s", log->group, urlGetResponse(up));
        urlFree(up);
        return R_ERR_CANT_CREATE;
    }
    urlFree(up);
    return 0;
}

static int describeLogGroup(IotoLog *log)
{
    Url      *up;
    Json     *json;
    JsonNode *groups, *child;
    cchar    *name, *nextToken;
    char     *data;
    int      sid, status;

    assert(log);

    up = urlAlloc(0);
    json = 0;
    nextToken = 0;
    do {
        if (nextToken) {
            data = sfmt("{\"logGroupNamePrefix\":\"%s\",\"nextToken\":\"%s\"}", log->group, nextToken);
        } else {
            data = sfmt("{\"logGroupNamePrefix\":\"%s\"}", log->group);
        }
        status = aws(up, log->region, "logs", "Logs_20140328.DescribeLogGroups", data, 0, 0);
        rFree(data);

        if (status != URL_CODE_OK) {
            rError("log", "Cannot describe log groups");
            urlFree(up);
            return R_ERR_BAD_STATE;
        }
        json = urlGetJsonResponse(up);

        if ((sid = jsonGetId(json, 0, "logGroups")) <= 0) {
            rError("log", "Cannot find logSGroups in response");
            jsonFree(json);
            urlFree(up);
            return R_ERR_BAD_FORMAT;
        }
        groups = jsonGetNode(json, sid, 0);
        for (ITERATE_JSON(json, groups, child, id)) {
            name = jsonGet(json, id, "logGroupName", 0);
            if (smatch(name, log->group)) {
                urlFree(up);
                jsonFree(json);
                return 0;
            }
        }
        nextToken = jsonGet(json, 0, "nextToken", 0);
        jsonFree(json);
    } while (nextToken);

    urlFree(up);
    return R_ERR_CANT_FIND;
}

static int getLogGroup(IotoLog *log)
{
    int rc;

    if ((rc = describeLogGroup(log)) < 0) {
        if (rc == R_ERR_CANT_FIND) {
            if (log->create) {
                if (createLogGroup(log) < 0) {
                    return R_ERR_CANT_CREATE;
                }
            } else {
                rError("log", "Cannot find log group %s", log->group);
                return R_ERR_CANT_FIND;
            }
        } else {
            return R_ERR_BAD_STATE;
        }
    }
    return getLogStream(log);
}

static int createLogStream(IotoLog *log)
{
    Url  *up;
    char *data;
    int  status;

    assert(log);

    up = urlAlloc(0);
    data = sfmt("{\"logGroupName\":\"%s\",\"logStreamName\":\"%s\"}", log->group, log->stream);

    status = aws(up, log->region, "logs", "Logs_20140328.CreateLogStream", data, 0, 0);
    rFree(data);

    if (status != URL_CODE_OK) {
        rError("log", "Cannot create stream %s in group %s, %s", log->stream, log->group, urlGetResponse(up));
        urlFree(up);
        return R_ERR_CANT_CREATE;
    }
    urlFree(up);
    return 0;
}

static int describeStream(IotoLog *log)
{
    Url      *up;
    Json     *json;
    JsonNode *streams, *child;
    cchar    *name, *nextToken;
    char     *data;
    int      sid, status;

    assert(log);

    up = urlAlloc(0);
    nextToken = 0;
    do {
        if (nextToken) {
            data = sfmt("{\"logGroupName\":\"%s\",\"logStreamNamePrefix\":\"%s\",\"nextToken\":\"%s\"}",
                        log->group, log->stream, nextToken);
        } else {
            data = sfmt("{\"logGroupName\":\"%s\",\"logStreamNamePrefix\":\"%s\"}", log->group, log->stream);
        }
        status = aws(up, log->region, "logs", "Logs_20140328.DescribeLogStreams", data, 0, 0);
        rFree(data);

        if (status != URL_CODE_OK) {
            rError("log", "Cannot describe log streams for group %s", log->group);
            urlFree(up);
            return R_ERR_BAD_STATE;
        }
        json = urlGetJsonResponse(up);
        if ((sid = jsonGetId(json, 0, "logStreams")) <= 0) {
            rError("log", "Cannot find logStreams in response");
            jsonFree(json);
            urlFree(up);
            return R_ERR_BAD_FORMAT;
        }
        streams = jsonGetNode(json, sid, 0);

        for (ITERATE_JSON(json, streams, child, id)) {
            name = jsonGet(json, id, "logStreamName", 0);
            if (smatch(name, log->stream)) {
                rFree(log->sequence);
                log->sequence = jsonGetClone(json, id, "uploadSequenceToken", 0);
                jsonFree(json);
                urlFree(up);
                return 0;
            }
        }
        nextToken = jsonGet(json, 0, "nextToken", 0);
        jsonFree(json);
    } while (nextToken);

    urlFree(up);
    return R_ERR_CANT_FIND;
}

/*
    Describe the stream and get the sequence number for submitting events
 */
static int getLogStream(IotoLog *log)
{
    int rc;

    assert(log);

    rFree(log->sequence);
    log->sequence = 0;

    if ((rc = describeStream(log)) < 0) {
        if (rc != R_ERR_BAD_STATE && createLogStream(log) < 0) {
            return R_ERR_CANT_CREATE;
        }
    }
    return 0;
}

#else
void dummyCloudLog(void)
{
}
#endif /* SERVICES_CLOUD */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
