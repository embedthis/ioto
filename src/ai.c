/*
    ai.c - AI support
 */

/********************************** Includes **********************************/

#include "ioto.h"

/************************************ Code ************************************/
#if SERVICES_AI

PUBLIC int ioInitAI(void)
{
    cchar *endpoint, *key, *show;
    int   flags = 0;

    /*
        FUTURE: key = rLookupName(ioto->keys, "OPENAI_KEY")
     */
    if ((key = getenv("OPENAI_API_KEY")) == NULL) {
        if ((key = jsonGet(ioto->config, 0, "ai.key", 0)) == NULL) {
            rInfo("openai", "OPENAI_API_KEY not set, define in environment or in config ai.key");
            // Allow rest of services to initialize
            return 0;
        }
    }
    endpoint = jsonGet(ioto->config, 0, "endpoint", "https://api.openai.com/v1");

    show = ioto->cmdAIShow ? ioto->cmdAIShow : jsonGet(ioto->config, 0, "log.show", 0);
    if (!show || !*show) {
        show = getenv("AI_SHOW");
    }
    flags = 0;
    if (show) {
        if (schr(show, 'H') || schr(show, 'R')) {
            flags |= AI_SHOW_REQ;
        }
        if (schr(show, 'h') || schr(show, 'r')) {
            flags |= AI_SHOW_RESP;
        }
    }
    return openaiInit(endpoint, key, ioto->config, flags);
}

PUBLIC void ioTermAI(void)
{
    openaiTerm();
}
#endif /* SERVICES_AI */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
