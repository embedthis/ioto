/*
 * OpenAI library Library Source
 */

#include "openai.h"

#if ME_COM_OPENAI



/********* Start of file src/openai.c ************/

/*
    openai.c - OpenAI support
 */

/********************************** Includes **********************************/



#if ME_COM_OPENAI

/*********************************** Defines **********************************/

#ifndef OPENAI_MAX_URL
    #define OPENAI_MAX_URL    512              /**< Sanity length of a URL */
#endif

/************************************ Code ************************************/

PUBLIC int openaiInit(cchar *endpoint, cchar *key, Json *config)
{
    if ((openai = rAllocType(OpenAI)) == NULL) {
        return R_ERR_MEMORY;
    }
    openai->endpoint = sclone(endpoint);
    openai->realTimeEndpoint = sreplace(endpoint, "https://", "wss://");
    openai->headers = sfmt("Authorization: Bearer %s\r\nContent-Type: application/json\r\n", key);
    return 0;
}

PUBLIC void openaiTerm(void)
{
    rFree(openai->endpoint);
    rFree(openai->realTimeEndpoint);
    rFree(openai->headers);
    rFree(openai);
    openai = NULL;
}

/*
    Chat Completion API
    Default model gpt-4o-mini
 */
PUBLIC Json *openaiChatCompletion(Json *props)
{
    Json *request, *response;
    char *data, url[OPENAI_MAX_URL];

    request = props ? jsonClone(props, 0) : jsonAlloc(0);
    if (!jsonGet(request, 0, "model", 0)) {
        jsonSet(request, 0, "model", "gpt-4o-mini", JSON_STRING);
    }
    data = jsonToString(request, 0, NULL, JSON_STRICT);
    rDebug("openai", "Request: %s", jsonString(request));
    jsonFree(request);

    response = urlPostJson(SFMT(url, "%s/chat/completions", openai->endpoint), data, -1, "%s", openai->headers);
    rFree(data);

    rDebug("openai", "Response: %s", jsonString(response));
    //  Caller must free response
    return response;
}

/*
    Submit a request to OpenAI Response API
    Message is the prompt to submit
    Props is a JSON object of Response API parameters
    The default model is gpt-4o-mini, truncation is auto, and tools are unset.
    Caller must free the returned JSON object
 */
PUBLIC Json *openaiResponse(Json *props)
{
    Json     *request, *response;
    JsonNode *output, *child, *content, *item;
    char     *data, url[OPENAI_MAX_URL];
    cchar    *text, *type;
    RBuf     *buf;
    int      cid, iid;

    request = props ? jsonClone(props, 0) : jsonAlloc(0);
    if (!jsonGet(request, 0, "model", 0)) {
        jsonSet(request, 0, "model", "gpt-4o-mini", JSON_STRING);
    }
    if (!jsonGet(request, 0, "truncation", 0)) {
        jsonSet(request, 0, "truncation", "auto", JSON_STRING);
    }
    data = jsonToString(request, 0, NULL, JSON_STRICT);
    rDebug("openai", "Request: %s", jsonString(request));
    jsonFree(request);

    /*
        Submit the request using the authentication headers
     */
    response = urlPostJson(SFMT(url, "%s/responses", openai->endpoint), data, -1, "%s", openai->headers);
    rFree(data);

    /*
        Parse the response and aggregate the output text into "output_text" for convenience
     */
    buf = rAllocBuf(0);
    if (response && (output = jsonGetNode(response, 0, "output")) != 0) {
        for (ITERATE_JSON(response, output, child, cid)) {
            type = jsonGet(response, cid, "type", 0);
            if (smatch(type, "message")) {
                if ((content = jsonGetNode(response, cid, "content")) != 0) {
                    for (ITERATE_JSON(response, content, item, iid)) {
                        type = jsonGet(response, iid, "type", 0);
                        if (smatch(type, "output_text")) {
                            text = jsonGet(response, iid, "text", 0);
                            rPutToBuf(buf, "%s\n", text);
                        }
                    }
                }
            }
        }
    }
    jsonSet(response, 0, "output_text", rBufToStringAndFree(buf), JSON_STRING);
    rDebug("openai", "Response: %s", jsonString(response));
    //  Caller must free response
    return response;
}

/*
    Open a WebSocket connection to the OpenAI Real Time API
    This blocks until the connection is closed
 */
PUBLIC Url *openaiRealTimeConnect(Json *props)
{
    Json           *request;
    Url            *up;
    char           headers[256], url[OPENAI_MAX_URL];

    request = props ? jsonClone(props, 0) : jsonAlloc(0);
    if (!jsonGet(request, 0, "model", 0)) {
        jsonSet(request, 0, "model", "gpt-4o-realtime-preview-2024-12-17", JSON_STRING);
    }
    SFMT(headers, "%sOpenAI-Beta: realtime=v1\r\n", openai->headers);
    SFMT(url, "%s/realtime?model=%s", openai->realTimeEndpoint, jsonGet(request, 0, "model", 0));

    /*
        Use low-level API so we can proxy the browser WebSocket to the OpenAI WebSocket
     */
    up = urlAlloc(0);
    if (urlStart(up, "GET", url) < 0) {
        urlFree(up);
        return 0;
    }
    if (urlWriteHeaders(up, headers) < 0 || urlFinalize(up) < 0) {
        urlFree(up);
        return 0;
    }
    return up;
}

/*
    List openAI models. Returns a JSON object with a list of models of the form: [
        {
            id: "o1-mini-2024-09-12",
            object: "model",
            created: 1725648979,
            owned_by: "system"
        },
    ]
 */
PUBLIC Json *openaiListModels()
{
    char url[OPENAI_MAX_URL];

    return urlGetJson(SFMT(url, "%s/models", openai->endpoint), "%s", openai->headers);
}

#if FUTURE
/*
    Create embeddings
 */
PUBLIC Json *openaiCreateEmbeddings(cchar *model, cchar *input, cchar *encodingFormat)
{
    Json *response;
    char *data, url[OPENAI_MAX_URL];

    if (!model) {
        model = "text-embedding-ada-002";
    }
    if (!encodingFormat) {
        encodingFormat = "float";
    }
    data = sfmt("{\"model\": \"%s\", \"input\": \"%s\", \"encoding_format\": \"%s\"}",
                model, input, encodingFormat);
    SFMT(url, "%s/embeddings", openai->endpoint);
    response = urlPostJson(url, data, -1, "%s", openai->headers);
    rFree(data);
    return response;
}

/*
    Fine tune a model
 */
PUBLIC Json *openaiFineTune(cchar *training)
{
    Json *response;
    char *data, url[OPENAI_MAX_URL];

    data = sfmt("{\"training_file\": \"%s\"}", training);
    SFMT(url, "%s/fine_tuning/jobs", openai->endpoint);
    response = urlPostJson(url, data, -1, "%s", openai->headers);
    rFree(data);
    return response;
}
#endif /* FUTURE*/
#endif /* ME_COM_OPENAI */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

#else
void dummyOpenAI(){}
#endif /* ME_COM_OPENAI */
