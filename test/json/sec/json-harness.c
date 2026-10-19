/*
    json-harness.c - In-process fuzz harness for the JSON parser and serializer

    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
 */

/********************************** Includes **********************************/

#include "harness.h"

#ifndef SEC_LIBFUZZER
#include "fuzzcore/fuzz.c"
#endif

#include "json.h"

/*********************************** Locals ***********************************/

/*
    The r runtime must be initialised before any rAlloc, and the SECPORT_R
    binding routes secAlloc there too. libFuzzer calls the oracle repeatedly in
    one process, so this runs exactly once either way.
 */
static void initRuntime(void)
{
    static int initialised = 0;

    if (!initialised) {
        initialised = 1;
        rInit(0, 0);
    }
}

/*
    Copy the fuzz input into a NUL-terminated working buffer. The parser is
    in-situ and writes into the text it is given, so it must never see the
    engine's own buffer.
 */
static char *dupInput(cchar *input, size_t len)
{
    char *buf;

    buf = (char*) secAlloc(len + 1);
    memcpy(buf, input, len);
    buf[len] = '\0';
    return buf;
}

/********************************** Oracles ***********************************/

/*
    Serialize as strict JSON and require the strict parser to accept the result.

    An empty document is excluded: jsonParse accepts empty text and yields a
    tree with no nodes, whose serialization is empty, and JSON_STRICT_PARSE
    rejects an empty document by design. That is not a round-trip failure.
 */
static bool roundTripOracle(cchar *input, size_t len)
{
    Json *json, *again;
    char *buf, *text;
    bool ok;

    initRuntime();
    ok = true;
    buf = dupInput(input, len);

    if ((json = jsonParse(buf, 0)) != 0) {
        if (json->count > 0 && (text = jsonToString(json, 0, 0, JSON_JSON)) != 0) {
            if (*text) {
                if ((again = jsonParse(text, JSON_STRICT_PARSE)) == 0) {
                    fprintf(stderr, "sec: strict reparse rejected our own JSON output: %s\n", text);
                    ok = false;
                } else {
                    jsonFree(again);
                }
            }
            rFree(text);
        }
        jsonFree(json);
    }
    secFree(buf);
    return ok;
}

/*
    Blend a document into a second parse of the same bytes, and into itself.
    Self-blend is the case that has to re-parse its own source, which is where
    the ownership of the re-parsed text goes wrong if anyone frees it twice.
 */
static bool blendOracle(cchar *input, size_t len)
{
    Json *dest, *src;
    char *destText, *srcText;

    initRuntime();
    destText = dupInput(input, len);
    srcText = dupInput(input, len);

    if ((dest = jsonParse(destText, 0)) != 0) {
        if ((src = jsonParse(srcText, 0)) != 0) {
            jsonBlend(dest, 0, 0, src, 0, 0, 0);
            jsonBlend(dest, 0, 0, src, 0, 0, JSON_COMBINE);
            jsonBlend(dest, 0, "under", src, 0, 0, JSON_OVERWRITE);
            jsonFree(src);
        }
        //  Self-blend: source and destination are the same tree
        jsonBlend(dest, 0, "copy", dest, 0, 0, 0);
        jsonFree(dest);
    }
    secFree(srcText);
    secFree(destText);
    return true;
}

/*
    Drive the path machinery -- getNextTerm, findProperty, jquery, copyProperty --
    and jsonTemplate, none of which the other two oracles reach. These consume an
    untrusted key string as well as an untrusted document.

    The input is split at the first newline: the first line is the key, the rest
    is the document. With no newline the whole input is the key and a fixed
    document supplies something to resolve against, which is the shape libFuzzer
    reaches first from a one-line seed.

    The logical property asserted is that the path machinery agrees with itself:
    a key that jsonSet accepted must read back through jsonGet.

    That property is only asserted for keys made of path syntax -- name characters
    and the . [ ] ' " separators. Every byte string is still driven through the
    read and write paths for memory safety, but a whole JSON document used as a
    key is not a path, and asserting set/get symmetry over one produces noise
    rather than findings. "$" is excluded too: the array-append term deliberately
    resolves to a different node on the way out than it named on the way in.
 */
static bool isPathSyntax(cchar *key)
{
    cchar *cp;

    if (!key || !*key) {
        return 0;
    }
    for (cp = key; *cp; cp++) {
        if (!isalnum((uchar) * cp) && schr("_.[]'\"", *cp) == 0) {
            return 0;
        }
    }
    return schr(key, '$') == 0;
}

static bool queryOracle(cchar *input, size_t len)
{
    Json  *json;
    char  *buf, *key, *doc, *text;
    cchar *readback;
    bool  ok;

    initRuntime();
    ok = true;
    buf = dupInput(input, len);

    if ((doc = strchr(buf, '\n')) != 0) {
        *doc++ = '\0';
        key = buf;
    } else {
        key = buf;
        doc = "{a: 1, b: {c: [1, 2, {d: 'x'}]}, arr: ['one', 'two'], name: 'John'}";
    }
    /*
        jsonParse copies its argument. An earlier revision cloned doc first and never
        freed the clone on the success path -- one leak per execution, which took the
        libFuzzer campaign to its 2 GB limit twice before the cause was found. The
        other two oracles run tens of millions of executions clean; when one oracle
        alone creeps, suspect the oracle.
     */
    if ((json = jsonParse(doc, 0)) != 0) {
        //  Read paths. Every one of these walks getNextTerm and findProperty.
        jsonGet(json, 0, key, "default");
        jsonGetNode(json, 0, key);
        jsonGetId(json, 0, key);
        jsonGetType(json, 0, key);
        jsonGetLength(json, 0, key);
        jsonGetBool(json, 0, key, 0);
        jsonGetNum(json, 0, key, 0);

        /*
            Template expansion over the document's own bytes. rFree is a macro that
            evaluates its argument twice, so rFree(jsonTemplate(...)) ran the expansion
            twice and leaked the first result -- one leak per execution, which crept the
            campaign to its RSS limit. Take the result into a variable first.
         */
        text = jsonTemplate(json, doc, 0);
        rFree(text);
        text = jsonTemplate(json, doc, 1);
        rFree(text);

        //  Write paths. jsonSet creates intervening nodes, so this drives
        //  jquery's create branch, insertNodes and growNodes.
        if (isPathSyntax(key) && jsonSet(json, 0, key, "sentinel", JSON_STRING) >= 0) {
            readback = jsonGet(json, 0, key, 0);
            if (!smatch(readback, "sentinel")) {
                fprintf(stderr, "sec: set/get disagree for key \"%s\": read back %s\n",
                        key, readback ? readback : "(null)");
                ok = false;
            }
        }
        jsonRemove(json, 0, key);
        text = jsonToString(json, 0, key, JSON_JSON);
        rFree(text);
        jsonFree(json);
    }
    secFree(buf);
    return ok;
}

#if defined(JSON_ORACLE_BLEND)
    #define ORACLE      blendOracle
    #define ORACLE_NAME "blend"
#elif defined(JSON_ORACLE_QUERY)
    #define ORACLE      queryOracle
    #define ORACLE_NAME "query"
#else
    #define ORACLE      roundTripOracle
    #define ORACLE_NAME "round-trip"
#endif

/****************************** libFuzzer entry *******************************/

#ifdef SEC_LIBFUZZER
SEC_LIBFUZZER_ENTRY(ORACLE)

#else

/*********************************** main *************************************/

int main(int argc, char **argv)
{
    FuzzConfig config;
    FuzzRunner *runner;
    cchar      *replay;
    int        crashes, leaked;

    (void) argc;
    (void) argv;

    initRuntime();

    memset(&config, 0, sizeof(config));
    config.stop = false;
    config.verbose = false;
    config.crashDir = "crashes/json";

    runner = fuzzInit(&config);
    fuzzSetOracle(runner, ORACLE);

    /*
        Seeds are the shapes the suite already asserts: the JSON5 relaxations,
        the malformed inputs from test/error.tst.c and test/fuzz.tst.c, and the
        numeric tokens from test/parsefail.tst.c that issue 10 is about.
        strlen() supplies the length so a literal and its length cannot drift.
     */
    {
        static cchar *seeds[] = {
            "{}",
            "[]",
            "{a: 1}",
            "{\"a\": 1}",
            "{a: 'one', b: \"two\", c: `three`}",
            "{a: {b: {c: [1, 2, {d: null}]}}}",
            "[1,2,3,]",
            "{a: 1, /* comment */ b: 2} // trailing",
            "{a: true, b: false, c: null, d: undefined}",
            "{a: 12x}",
            "{a: 01}",
            "{a: 1.}",
            "{a: 1e}",
            "{a: -}",
            "{a: 1-2}",
            "{a: 0x10, b: Infinity, c: NaN}",
            "{re: /ab+c/}",
            "{a: '\\u0041\\n\\t\\\\'}",
            "{v: \"\\u00e9\\u4e2d\\uD83D\\uDCA9\"}",
            "{v: \"\\uD83D\"}",
            "{v: \"\\uDCA9x\"}",
            "{\"a\":,}",
            "{'${name}': 'v', name: 'John'}",
            "{blend: ['x'], base: 1}",
            "{profile: 'dev', conditional: {profile: {dev: {x: 1}}}}",
            "{",
            "}",
            "[",
            "]",
            "{a:",
            "\"unterminated",
            "\"\\u12",
            "{re: /abc}",
            "[1 [2]]",
            "{a: .5}",
            ":::::",
            ",,,,,",
            "\x01",
            "",
        };
        size_t s;
        for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
            fuzzAddCorpus(runner, seeds[s], strlen(seeds[s]));
        }
    }
    fuzzLoadDir(runner, "corpus");

    replay = getenv("FUZZ_REPLAY");
    if (replay) {
        printf("Replay mode: %s\n", replay);
        fuzzClearCorpus(runner);
        fuzzLoadArtifact(runner, replay);
    }
    printf("Oracle: %s\n", ORACLE_NAME);

    crashes = fuzzRun(runner);
    fuzzReport(runner);
    fuzzFree(runner);

    leaked = fuzzLeakCheck();

    return (crashes > 0 || leaked > 0) ? 1 : 0;
}

#endif /* SEC_LIBFUZZER */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
 */
