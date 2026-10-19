/*
    depth.tst.c - Bound serialization recursion, including template expansion.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#include "test.h"

#if ME_UNIX_LIKE
#include <pthread.h>
#endif

#ifndef ME_JSON_MAX_RECURSION
#define ME_JSON_MAX_RECURSION 32
#endif

static char *nestedText(int depth, bool objects)
{
    RBuf *buf;
    int  i;

    buf = rAllocBuf(128);
    for (i = 0; i < depth; i++) {
        rPutStringToBuf(buf, objects ? "{\"a\":" : "[");
    }
    rPutStringToBuf(buf, "0");
    for (i = 0; i < depth; i++) {
        rPutStringToBuf(buf, objects ? "}" : "]");
    }
    return rBufToStringAndFree(buf);
}

static void serializationDepthTest(void)
{
    Json *json;
    RBuf *buf;
    char *text, *result;
    int  objects, rc;

    for (objects = 0; objects <= 1; objects++) {
        text = nestedText(ME_JSON_MAX_RECURSION, objects != 0);
        json = parse(text);
        ttrue(json != NULL);
        result = jsonToString(json, 0, NULL, JSON_JSON);
        tmatch(result, text);
        rFree(result);
        jsonFree(json);
        rFree(text);

        text = nestedText(ME_JSON_MAX_RECURSION + 1, objects != 0);
        json = parse(text);
        ttrue(json != NULL);
        result = jsonToString(json, 0, NULL, JSON_JSON);
        ttrue(result == NULL);
        rFree(result);
        result = jsonToString(json, 0, NULL, JSON_HUMAN);
        ttrue(result == NULL);
        rFree(result);
        buf = rAllocBuf(128);
        rc = jsonPutToBuf(buf, json, 0, JSON_JSON);
        ttrue(rc < 0);
        rFreeBuf(buf);
        jsonFree(json);
        rFree(text);
    }
}

static void expansionDepthTest(void)
{
    Json *json;
    char *nested, *text, *result;

    nested = nestedText(ME_JSON_MAX_RECURSION + 1, false);
    text = sfmt("{\"value\":%s,\"template\":\"${value}\"}", nested);
    json = parse(text);
    ttrue(json != NULL);
    /* Only serialize the shallow string; its expansion reaches the deep subtree. */
    result = jsonToString(json, 0, "template", JSON_JSON | JSON_EXPAND);
    ttrue(result == NULL);
    rFree(result);
    jsonFree(json);
    rFree(text);
    rFree(nested);
}

#if ME_UNIX_LIKE
static void *serializeOnSmallStack(void *arg)
{
    return jsonToString((Json*) arg, 0, NULL, JSON_JSON);
}

static void smallStackTest(void)
{
    pthread_attr_t attr;
    pthread_t      thread;
    Json           *json;
    char           *text;
    void           *result;
    int            rc;
    long           minStack;
    size_t         stackSize;

    text = nestedText(5000, false);
    json = parse(text);
    ttrue(json != NULL);
    rc = pthread_attr_init(&attr);
    teqi(rc, 0);
    // Linux aarch64 can require more than 32K even when setstacksize accepts that size.
    minStack = sysconf(_SC_THREAD_STACK_MIN);
    stackSize = 32 * 1024;
    if (minStack > 0 && (size_t) minStack > stackSize) {
        stackSize = (size_t) minStack;
    }
    rc = pthread_attr_setstacksize(&attr, stackSize);
    teqi(rc, 0);
    rc = pthread_create(&thread, &attr, serializeOnSmallStack, json);
    teqi(rc, 0);
    if (rc == 0) {
        rc = pthread_join(thread, &result);
        teqi(rc, 0);
        ttrue(result == NULL);
        rFree(result);
    }
    pthread_attr_destroy(&attr);
    jsonFree(json);
    rFree(text);
}
#endif

int main(void)
{
    rInit(0, 0);
    serializationDepthTest();
    expansionDepthTest();
#if ME_UNIX_LIKE
    smallStackTest();
#endif
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
