/*
    ioto-fixtures.h - Fixture copies of Ioto web/r functions under test

    FIXTURE COPY — these functions are verbatim extracts from:
        webDecode   : ioto/web/src/utils.c:123  (commit as-of authoring)
        stoix       : ioto/web/src/r/rLib.c:8621

    They exist here because pulling in the full Ioto r+crypt+uctx+web runtime
    solely to compile two leaf functions would require a complete Ioto build
    environment. The fixtures let fuzzcore run under SECPORT_PORTABLE (pure
    libc) without any server dependency, which is the correct trade-off for a
    deterministic, portable harness.

    The harness oracle is verified against the real signatures of webDecode and
    stoix, the URL decoder and the chunk-size parser.

    DO NOT modify these copies independently of the originals — they must stay
    in sync. To update: copy the body verbatim from the source file and note
    the commit hash in the comment below.

    Source origin: ioto repo, branch master (2026-06 snapshot)
 */

#ifndef _h_IOTO_FIXTURES
#define _h_IOTO_FIXTURES 1

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/*
    Substitute types matching the Ioto r runtime.
    In the real runtime these are defined in r.h.
    Guard against redefinition when harness.h has already pulled in secport.h.
 */
#ifndef _h_SECPORT
typedef const char  cchar;
#endif
typedef long long   int64;
typedef ssize_t     ssize;

/*
    slen — fixture shim matching rLib.c:8126.
    Returns 0 for NULL (null-tolerant per Ioto convention).
 */
static inline size_t slen(cchar *s)
{
    return s ? strlen(s) : 0;
}

/*
    sclone — fixture shim for mprUriDecode / sclone usage in the Ioto stack.
    Allocates via malloc; caller must free.
 */
static inline char *sclone(cchar *str)
{
    size_t len;
    char   *copy;

    if (!str) str = "";
    len = strlen(str);
    copy = (char *) malloc(len + 1);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

/*
    stoix — FIXTURE COPY of ioto/web/src/r/rLib.c:8621
    Radix integer parse. radix=16 is the chunk-size path under test.
 */
static inline int64 stoix(cchar *str, char **end, int radix)
{
    int64 result;

    if (!str || !*str) {
        return 0;
    }
    result = strtoll(str, end, radix);
    return result;
}

/*
    webDecode — FIXTURE COPY of ioto/web/src/utils.c:123
    URL percent-decode in-situ. Decodes %XX sequences and '+' → ' '.
    Returns str on success, 0 if str is NULL.
 */
static inline char *webDecode(char *str)
{
    char   *ip, *op;
    size_t len;
    int    num, i, c;

    if (str == 0) {
        return 0;
    }
    len = slen(str);
    for (ip = op = str; len > 0 && *ip; ip++, op++) {
        if (*ip == '+') {
            *op = ' ';
        } else if (*ip == '%' && isxdigit((unsigned char) ip[1]) && isxdigit((unsigned char) ip[2]) &&
                   !(ip[1] == '0' && ip[2] == '0')) {
            /* Convert %nn to a single character */
            ip++;
            for (i = 0, num = 0; i < 2; i++, ip++) {
                c = tolower((unsigned char) *ip);
                if (c >= 'a' && c <= 'f') {
                    num = (num * 16) + 10 + c - 'a';
                } else {
                    num = (num * 16) + c - '0';
                }
            }
            *op = (char) num;
            ip--;
        } else {
            *op = *ip;
        }
        len--;
    }
    *op = '\0';
    return str;
}

#endif /* _h_IOTO_FIXTURES */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
 */
