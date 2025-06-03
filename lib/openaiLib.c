/*
 * OpenAI library Library Source
 */

#include "openai.h"

#if ME_COM_OPENAI



/********* Start of file src/openaiLib.c ************/

/*
    openai.c - OpenAI support
 */

/********************************** Includes **********************************/



#if ME_COM_OPENAI

/*********************************** Defines **********************************/

#ifndef OPENAI_MAX_URL
    #define OPENAI_MAX_URL 512                 /**< Sanity length of a URL */
#endif

static char *makeOutputText(Json *response);
static Json *processResponse(Json *request, Json *response, OpenAIAgent agent, void *arg);

/************************************ Code ************************************/

PUBLIC int openaiInit(cchar *endpoint, cchar *key, Json *config, int flags)
{
    if ((openai = rAllocType(OpenAI)) == NULL) {
        return R_ERR_MEMORY;
    }
    openai->endpoint = sclone(endpoint);
    openai->realTimeEndpoint = sreplace(endpoint, "https://", "wss://");
    openai->headers = sfmt("Authorization: Bearer %s\r\nContent-Type: application/json\r\n", key);
    openai->flags = flags;
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

    if (!openai) {
        return NULL;
    }
    request = props ? jsonClone(props, 0) : jsonAlloc(0);
    if (!jsonGet(request, 0, "model", 0)) {
        jsonSet(request, 0, "model", "gpt-4o-mini", JSON_STRING);
    }
    data = jsonToString(request, 0, NULL, JSON_STRICT);
    rDebug("openai", "Request: %s", jsonString(request, JSON_PRETTY));
    jsonFree(request);

    response = urlPostJson(SFMT(url, "%s/chat/completions", openai->endpoint), data, -1, "%s", openai->headers);
    rFree(data);

    rDebug("openai", "Response: %s", jsonString(response, JSON_PRETTY));
    //  Caller must free response
    return response;
}

/*
    Submit a request to OpenAI Responses API and process the response invoking agents/tools as required.
    Props is a JSON object of Response API parameters
    The default model is gpt-4o-mini, truncation is auto, and tools are unset.
    Caller must free the returned JSON object
    The callback to invoke tools and agents is "agent(arg)"
 */
PUBLIC Json *openaiResponses(Json *props, OpenAIAgent agent, void *arg)
{
    Json *next, *request, *response;
    Url  *up;
    char *data, *text, url[OPENAI_MAX_URL];

    if (!openai) {
        return NULL;
    }
    request = jsonClone(props, 0);
    if (!jsonGet(request, 0, "model", 0)) {
        jsonSet(request, 0, "model", "gpt-4o-mini", JSON_STRING);
    }
    if (!jsonGet(request, 0, "truncation", 0)) {
        jsonSet(request, 0, "truncation", "auto", JSON_STRING);
    }
    /*
        Submit the request using the authentication headers
     */
    do {
        data = jsonToString(request, 0, NULL, JSON_STRICT);
        if (openai->flags & AI_SHOW_REQ) {
            rInfo("openai", "Request: %s", jsonString(request, JSON_PRETTY));
        }
        up = urlAlloc(0);
        response = urlJson(up, "POST", SFMT(url, "%s/responses", openai->endpoint), data, -1, openai->headers);
        rFree(data);

        if (!response) {
            rError("openai", "Failed to submit request to OpenAI: %s", urlGetError(up));
            jsonFree(request);
            urlFree(up);
            return NULL;
        }
        urlFree(up);

        next = processResponse(request, response, agent, arg);

        jsonFree(request);
        request = next;
    } while (request != NULL);

    text = makeOutputText(response);
    if (openai->flags & AI_SHOW_RESP) {
        rInfo("openai", "Response Text: %s", text);
    }
    jsonSet(response, 0, "output_text", text, JSON_STRING);
    rFree(text);
    return response;
}

/*
    Process the OpenAI response and invoke the agents/tools as required
 */
static Json *processResponse(Json *request, Json *response, OpenAIAgent agent, void *arg)
{
    JsonNode *item;
    cchar    *name, *toolId, *type;
    char     *result;
    int      count;

    if (openai->flags & AI_SHOW_RESP) {
        rInfo("openai", "Response: %s", jsonString(response, JSON_PRETTY));
    }
    if (!smatch(jsonGet(response, 0, "output[0].type", 0), "function_call")) {
        //  No agents/tools required
        return NULL;
    }
    request = jsonClone(request, 0);
    jsonSetJsonFmt(request, 0, "input[$]", "{role: 'user', content: '%s'}", jsonGet(request, 0, "input", 0));
    jsonBlend(request, 0, "input[$]", response, 0, "output[0]", 0);

    /*
        Invoke all the required agents & tools
     */
    count = 0;
    for (ITERATE_JSON_KEY(response, 0, "output", item, tid)) {
        type = jsonGet(response, tid, "type", 0);
        if (!smatch(type, "function_call")) {
            continue;
        }
        name = jsonGet(response, tid, "name", 0);
        toolId = jsonGet(response, tid, "call_id", 0);

        /*
            Invoke the agent/tool to process and get a result
         */
        if ((result = agent(name, request, response, arg)) == 0) {
            rError("openai", "Agent %s returned NULL", name);
            count = 0;
            break;
        }

        jsonSetJsonFmt(request, 0, "input[$]",
                       "{type: 'function_call_output', call_id: '%s', output: '%s'}", toolId, result);
        rFree(result);
        count++;
    }
    if (count == 0) {
        jsonFree(request);
        return NULL;
    }
    return request;
}

static char *makeOutputText(Json *response)
{
    JsonNode *child, *item;
    RBuf     *buf;

    buf = rAllocBuf(0);
    for (ITERATE_JSON_KEY(response, 0, "output", child, cid)) {
        if (smatch(jsonGet(response, cid, "type", 0), "message")) {
            for (ITERATE_JSON_KEY(response, cid, "content", item, iid)) {
                if (smatch(jsonGet(response, iid, "type", 0), "output_text")) {
                    rPutToBuf(buf, "%s\n", jsonGet(response, iid, "text", 0));
                }
            }
        }
    }
    return rBufToStringAndFree(buf);
}

PUBLIC Url *openaiStream(Json *props, UrlSseProc callback, void *arg)
{
    Url  *up;
    Json *request;
    char *data, url[OPENAI_MAX_URL];
    int  status;

    if (!openai) {
        return NULL;
    }
    request = props ? jsonClone(props, 0) : jsonAlloc(0);
    if (!jsonGet(request, 0, "model", 0)) {
        jsonSet(request, 0, "model", "gpt-4o-mini", JSON_STRING);
    }
    if (!jsonGet(request, 0, "truncation", 0)) {
        jsonSet(request, 0, "truncation", "auto", JSON_STRING);
    }
    jsonSetBool(request, 0, "stream", 1);
    data = jsonToString(request, 0, NULL, JSON_STRICT);
    rDebug("openai", "Request: %s", jsonString(request, JSON_PRETTY));
    jsonFree(request);

    /*
        Submit the request using the authentication headers
     */
    up = urlAlloc(0);
    status = urlFetch(up, "POST", SFMT(url, "%s/responses", openai->endpoint), data, -1, "%s", openai->headers);
    rFree(data);
    if (status != URL_CODE_OK) {
        urlFree(up);
        return NULL;
    }
    urlSseAsync(up, callback, arg);
    return up;
}

/*
    Open a WebSocket connection to the OpenAI Real Time API
    This blocks until the connection is closed
 */
PUBLIC Url *openaiRealTimeConnect(Json *props)
{
    if (!openai) {
        return NULL;
    }
    Json *request;
    Url  *up;
    char headers[256], url[OPENAI_MAX_URL];

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
    List openAI models. Returns a JSON object with a list of models of the form: [{id, object, created, owned_by}]
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
