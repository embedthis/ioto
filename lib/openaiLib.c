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

/*********************************** Locals ***********************************/

static OpenAI *openai;

/*********************************** Defines **********************************/

static char *makeOutputText(Json *response);
static Json *processResponse(Json *request, Json *response, OpenAIAgent agent, void *arg);

/************************************ Code ************************************/

PUBLIC int openaiInit(cchar *endpoint, cchar *key, Json *config, int flags)
{
    if (!endpoint || *endpoint == '\0') {
        return R_ERR_BAD_ARGS;
    }
    if (!key || *key == '\0') {
        return R_ERR_BAD_ARGS;
    }
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
    if (!openai) {
        return;
    }
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
    Url  *up;
    Json *request, *response;
    char *data, *url;

    if (!openai || !props || props->count == 0) {
        return NULL;
    }
    response = NULL;
    request = props ? jsonClone(props, 0) : jsonAlloc();
    if (!jsonGet(request, 0, "model", 0)) {
        jsonSet(request, 0, "model", "gpt-4o-mini", JSON_STRING);
    }
    data = jsonToString(request, 0, NULL, JSON_JSON);
    rDebug("openai", "Request: %s", jsonString(request, JSON_HUMAN));
    jsonFree(request);

    url = sfmt("%s/chat/completions", openai->endpoint);
    up = urlAlloc(0);
    response = urlJson(up, "POST", url, data, 0, "%s", openai->headers);
    if (!response) {
        rDebug("openai", "Failed to submit request to OpenAI: %s", urlGetError(up));
    } else {
        rDebug("openai", "Response: %s", jsonString(response, JSON_HUMAN));
    }
    rFree(url);
    rFree(data);
    urlFree(up);
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
    char *data, *text, *url;

    if (!openai || !props || props->count == 0) {
        return NULL;
    }
    request = props ? jsonClone(props, 0) : jsonAlloc();
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
        data = jsonToString(request, 0, NULL, JSON_JSON);
        if (openai->flags & AI_SHOW_REQ) {
            rInfo("openai", "Request: %s", jsonString(request, JSON_HUMAN));
        }
        up = urlAlloc(0);
        url = sfmt("%s/responses", openai->endpoint);
        response = urlJson(up, "POST", url, data, 0, openai->headers);
        rFree(url);
        rFree(data);

        if (!response) {
            rTrace("openai", "Failed to submit request to OpenAI: %s", urlGetError(up));
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
    int      count, id;

    if (openai->flags & AI_SHOW_RESP) {
        rInfo("openai", "Response: %s", jsonString(response, JSON_HUMAN));
    }
    if (!smatch(jsonGet(response, 0, "output[0].type", 0), "function_call")) {
        //  No agents/tools required
        return NULL;
    }
    request = jsonClone(request, 0);
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
        if (name == NULL || toolId == NULL) {
            rTrace("openai", "Agent call from response is missing name or call_id");
            continue;
        }
        /*
            Invoke the agent/tool to process and get a result
         */
        if ((result = agent(name, request, response, arg)) == 0) {
            rTrace("openai", "Agent %s returned NULL", name);
            count = 0;
            break;
        }
        id = jsonSet(request, jsonGetId(request, 0, "input"), "[$]", NULL, JSON_OBJECT);
        if (id >= 0) {
            jsonSet(request, id, "type", "function_call_output", JSON_STRING);
            jsonSet(request, id, "call_id", toolId, JSON_STRING);
            jsonSet(request, id, "output", result, JSON_STRING);
        }
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
    char *data, *url;
    int  status;

    if (!openai) {
        return NULL;
    }
    request = props ? jsonClone(props, 0) : jsonAlloc();
    if (!jsonGet(request, 0, "model", 0)) {
        jsonSet(request, 0, "model", "gpt-4o-mini", JSON_STRING);
    }
    if (!jsonGet(request, 0, "truncation", 0)) {
        jsonSet(request, 0, "truncation", "auto", JSON_STRING);
    }
    jsonSetBool(request, 0, "stream", 1);
    data = jsonToString(request, 0, NULL, JSON_JSON);
    rDebug("openai", "Request: %s", jsonString(request, JSON_HUMAN));
    jsonFree(request);

    /*
        Submit the request using the authentication headers
     */
    up = urlAlloc(0);
    url = sfmt("%s/responses", openai->endpoint);
    status = urlFetch(up, "POST", url, data, 0, "%s", openai->headers);
    rFree(url);
    rFree(data);
    if (status != URL_CODE_OK) {
        urlFree(up);
        return NULL;
    }
    urlSseRun(up, callback, arg, up->rx, 0);
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
    char *headers, *url;

    request = props ? jsonClone(props, 0) : jsonAlloc();
    if (!jsonGet(request, 0, "model", 0)) {
        jsonSet(request, 0, "model", "gpt-4o-realtime-preview-2024-12-17", JSON_STRING);
    }
    headers = sfmt("%sOpenAI-Beta: realtime=v1\r\n", openai->headers);
    url = sfmt("%s/realtime?model=%s", openai->realTimeEndpoint, jsonGet(request, 0, "model", 0));

    /*
        Use low-level API so we can proxy the browser WebSocket to the OpenAI WebSocket
     */
    up = urlAlloc(0);
    if (urlStart(up, "GET", url) < 0) {
        urlFree(up);
        jsonFree(request);
        rFree(headers);
        rFree(url);
        return 0;
    }
    if (urlWriteHeaders(up, headers) < 0 || urlFinalize(up) < 0) {
        urlFree(up);
        jsonFree(request);
        rFree(headers);
        rFree(url);
        return 0;
    }
    jsonFree(request);
    rFree(headers);
    rFree(url);
    return up;
}

/*
    List openAI models. Returns a JSON object with a list of models of the form: [{id, object, created, owned_by}]
 */
PUBLIC Json *openaiListModels(void)
{
    char *url;
    Json *response;

    if (!openai) {
        return NULL;
    }
    url = sfmt("%s/models", openai->endpoint);
    response = urlGetJson(url, "%s", openai->headers);
    rFree(url);
    return response;
}

#if FUTURE
/*
    Create embeddings
 */
PUBLIC Json *openaiCreateEmbeddings(cchar *model, cchar *input, cchar *encodingFormat)
{
    Json *obj, *response;
    char *data, *url;

    if (!openai) {
        return NULL;
    }
    if (!model) {
        model = "text-embedding-ada-002";
    }
    if (!encodingFormat) {
        encodingFormat = "float";
    }
    obj = jsonAlloc();
    jsonSet(obj, 0, "model", model, JSON_STRING);
    jsonSet(obj, 0, "input", input, JSON_STRING);
    jsonSet(obj, 0, "encoding_format", encodingFormat, JSON_STRING);
    data = jsonString(obj, JSON_JSON);

    url = sfmt("%s/embeddings", openai->endpoint);
    response = urlPostJson(url, data, -1, "%s", openai->headers);
    rFree(url);
    jsonFree(obj);
    return response;
}

/*
    Fine tune a model
 */
PUBLIC Json *openaiFineTune(cchar *training)
{
    Json *obj, *response;
    char *data, *url;

    if (!openai) {
        return NULL;
    }
    obj = jsonAlloc();
    jsonSet(obj, 0, "training_file", training, JSON_STRING);
    data = jsonString(obj, JSON_JSON);

    url = sfmt("%s/fine_tuning/jobs", openai->endpoint);
    response = urlPostJson(url, data, -1, "%s", openai->headers);
    rFree(url);
    jsonFree(obj);
    return response;
}
#endif /* FUTURE*/
#else
void dummyOpenAI(void)
{
}
#endif /* ME_COM_OPENAI */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

#else
void dummyOpenAI(){}
#endif /* ME_COM_OPENAI */
