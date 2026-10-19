/*
    clone.tst.c - Verify that cloning never returns a truncated tree.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#include "test.h"

#ifndef ME_JSON_MAX_RECURSION
#define ME_JSON_MAX_RECURSION 32
#endif

static void cloneDepthTest(int depth, bool succeeds)
{
    Json     *src, *dest;
    JsonNode *sp, *dp;
    RBuf     *buf;
    char     *text;
    int      i;

    buf = rAllocBuf(128);
    for (i = 0; i < depth; i++) {
        rPutStringToBuf(buf, "{a:");
    }
    rPutStringToBuf(buf, "42");
    for (i = 0; i < depth; i++) {
        rPutCharToBuf(buf, '}');
    }
    text = rBufToStringAndFree(buf);
    src = parse(text);
    ttrue(src != NULL);
    dest = jsonClone(src, 0);
    if (succeeds) {
        ttrue(dest != NULL);
        if (dest) {
            ttrue(dest->error == NULL);
            teqi(dest->count, src->count);
            for (i = 0; i < src->count && i < dest->count; i++) {
                sp = &src->nodes[i];
                dp = &dest->nodes[i];
                teqi(dp->type, sp->type);
                teqi(dp->last, sp->last);
                if (sp->name) {
                    tmatch(dp->name, sp->name);
                    ttrue(dp->name != sp->name);
                }
                if (sp->value) {
                    tmatch(dp->value, sp->value);
                    ttrue(dp->value != sp->value);
                }
            }
        }
    } else {
        ttrue(dest == NULL);
    }
    teqi(src->count, depth + 1);
    ttrue(src->error == NULL);
    tmatch(src->nodes[depth].value, "42");
    jsonFree(dest);
    jsonFree(src);
    rFree(text);
}

int main(void)
{
    rInit(0, 0);
    cloneDepthTest(ME_JSON_MAX_RECURSION, true);
    /* The root blend call is depth zero; primitive leaves do not recurse. */
    cloneDepthTest(ME_JSON_MAX_RECURSION + 1, true);
    cloneDepthTest(ME_JSON_MAX_RECURSION + 2, false);
    cloneDepthTest(100, false);
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
