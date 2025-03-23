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
} OpenAI;

static OpenAI *openai;


/**
    Submit a request to OpenAI Response API
    @description The following defaults are set: {model: 'gpt-4o-mini', truncation: 'auto'}.
        The API will aggregate the output text into "output_text" for convenience.
    @param props is a JSON object of Response API parameters
    @return Returns a JSON object with the response from the OpenAI Response API. Caller must free the returned JSON object with jsonFree.
    @stability Evolving
 */
PUBLIC Json *openaiResponse(Json *props);

/**
    Submit a request to OpenAI Chat Completion API
    @description The following defaults are set: model: gpt-4o-mini
    @param props is a JSON object of Response API parameters
    @return Returns a JSON object with the response from the OpenAI Response API. Caller must free the returned JSON object with jsonFree.
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

//  Internal
PUBLIC int openaiInit(cchar *endpoint, cchar *key, Json *config);
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
