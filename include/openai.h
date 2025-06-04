/*
    openai.h -- Ioto OpenAI Header

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_OPENAI_H
#define _h_OPENAI_H 1

/********************************** Includes **********************************/

#include "me.h"
#include "r.h"
#include "json.h"
#include "url.h"

#ifdef __cplusplus
extern "C" {
#endif

#if ME_COM_OPENAI
/*********************************** Defines **********************************/

struct OpenAIRealTime;
typedef struct OpenAI {
    char *endpoint;                     /**< OpenAI endpoint */
    char *realTimeEndpoint;             /**< OpenAI real time endpoint */
    char *headers;                      /**< OpenAI headers */
    int flags;                          /**< OpenAI flags */
} OpenAI;

/**
    OpenAI Agent callback function
    @param name is the name of the agent
    @param request is the request JSON object
    @param response is the response JSON object
    @param arg is a user argument
    @return Allocated text to be added to the response from the agent/tool.
 */
typedef char*(*OpenAIAgent)(cchar *name, Json *request, Json *response, void *arg);

/**
    Submit a request to OpenAI Responses API
    @description The following defaults are set: {model: 'gpt-4o-mini', truncation: 'auto'}.
        The API will aggregate the output text into "output_text" for convenience.
    @param props is a JSON object of Response API parameters
    @param agent is a callback function that will be called for each chunk of the response
    @param arg is a user argument that will be passed to the callback function
    @return Returns a JSON object with the response from the OpenAI Response API. Caller must free the returned JSON
       object with jsonFree.
    @stability Evolving
 */
PUBLIC Json *openaiResponses(Json *props, OpenAIAgent agent, void *arg);

/**
    Submit a request to OpenAI Response API and stream the response
    @description The following defaults are set: {model: 'gpt-4o-mini', truncation: 'auto'}.
        The API will aggregate the output text into "output_text" for convenience.
    @param props is a JSON object of Response API parameters
    @param callback is a callback function that will be called for each chunk of the response
    @param arg is a user argument that will be passed to the callback function
    @return Returns an Url object on success, or 0 on failure
    @stability Evolving
 */
PUBLIC Url *openaiStream(Json *props, UrlSseProc callback, void *arg);

/**
    Submit a request to OpenAI Chat Completion API
    @description The following defaults are set: model: gpt-4o-mini
    @param props is a JSON object of Response API parameters
    @return Returns a JSON object with the response from the OpenAI Response API. Caller must free the returned JSON
       object with jsonFree.
    @stability Evolving
 */
PUBLIC Json *openaiChatCompletion(Json *props);

/**
    Submit a request to OpenAI Real Time API
    @param props is a JSON object of Real Time API parameters
    @return Returns an OpenAIRealTime object on success, or 0 on failure
    @stability Evolving
 */
PUBLIC Url *openaiRealTimeConnect(Json *props);

/*
    List openAI models.
    @returns Returns a JSON object with a list of models of the form: [{id, object, created, owned_by}]
    @stability Evolving
 */
PUBLIC Json *openaiListModels(void);

#if FUTURE
PUBLIC Json *openaiCreateEmbeddings(cchar *model, cchar *input, cchar *encodingFormat);
PUBLIC Json *openaiFineTune(cchar *data);
#endif

/*
    Init flags
 */
#define AI_SHOW_NONE 0x1               /**< Trace nothing */
#define AI_SHOW_REQ  0x2               /**< Trace request */
#define AI_SHOW_RESP 0x8               /**< Trace response */

/**
    Initialize the OpenAI API
    @param endpoint is the OpenAI endpoint
    @param key is the OpenAI key
    @param config is a JSON object of configuration parameters
    @param flags is a bitmask of flags
    @stability Internal
 */
PUBLIC int openaiInit(cchar *endpoint, cchar *key, Json *config, int flags);

/**
    Terminate the OpenAI API
    @stability Internal
 */
PUBLIC void openaiTerm(void);

#endif /* ME_COM_OPENAI */


#ifdef __cplusplus
}
#endif
#endif /* _h_OPENAI_H */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
