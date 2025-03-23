/*
    aiApp.c -- Demonstration of AI APIs
 */
/********************************** Includes **********************************/

#include "ioto.h"

/*********************************** Forwards *********************************/

static void aiChatCompletionAction(Web *web);
static void aiChatResponseAction(Web *web);
static void aiChatRealTimeAction(Web *web);

#if EXAMPLES
static void aiResponseExample(void);
static void aiChatCompletionExample(void);
static void aiGetModelsExample(void);
#endif

/************************************ Code ************************************/

/*
    Called when Ioto is fully initialized. This runs on a fiber while the main fiber services events.
 */
int ioStart(void)
{
    if (jsonGetBool(ioto->config, 0, "ai.enable", 1)) {
        webAddAction(ioto->webHost, "/ai/chat/response", aiChatResponseAction, NULL);
        webAddAction(ioto->webHost, "/ai/chat/completion", aiChatCompletionAction, NULL);
        webAddAction(ioto->webHost, "/ai/chat/realtime", aiChatRealTimeAction, NULL);
        rInfo("ai", "AI started\n");
#if EXAMPLES
        aiResponseExample();
        aiChatCompletionExample();
        aiGetModelsExample();
#endif
    } else {
        rInfo("ai", "AI disabled");
    }
    return 0;
}

/*
    This is called when Ioto is shutting down
 */
void ioStop(void)
{
}

/*
    Sample web form to use the OpenAI Chat Completion API.
 */
static void aiChatCompletionAction(Web *web)
{
    Json *response;

    response = openaiChatCompletion(web->vars);
    webWriteJson(web, response, 0, NULL);
    webFinalize(web);
    jsonFree(response);
}

/*
    Sample web form to use the OpenAI Chat Response API.
 */
static void aiChatResponseAction(Web *web)
{
    Json *response;

    response = openaiResponse(web->vars);
    webWriteJson(web, response, 0, NULL);
    webFinalize(web);
    jsonFree(response);
}

/*
    Callback for the OpenAI Real Time API.
    This is called when a message is received from OpenAI.
 */
static void realTimeCallback(WebSocket *ws, int event, cchar *message, ssize len, Web *web)
{
    if (event == WS_EVENT_MESSAGE) {
        webSocketSend(web->webSocket, "%s", message);

    } else if (event == WS_EVENT_CLOSE) {
        rResumeFiber(ws->fiber, 0);

    } else if (event == WS_EVENT_ERROR) {
        rInfo("openai", "WebSocket error: %s", ws->errorMessage);
        rResumeFiber(ws->fiber, 0);
    }
}

/*
    Callback for the browser. This is called when a message is received from the browser.
 */
static void browserCallback(WebSocket *ws, int event, cchar *message, ssize len, Url *up)
{
    if (event == WS_EVENT_MESSAGE) {
        webSocketSend(up->webSocket, "%s", message);

    } else if (event == WS_EVENT_CLOSE) {
        rResumeFiber(ws->fiber, 0);

    } else if (event == WS_EVENT_ERROR) {
        rInfo("openai", "WebSocket error: %s", ws->errorMessage);
        rResumeFiber(ws->fiber, 0);
    }
}

static void aiChatRealTimeAction(Web *web)
{
    Url *up;

    if (!web->upgrade) {
        webError(web, 400, "Connection not upgraded to WebSocket");
        return;
    }
    if ((up = openaiRealTimeConnect(NULL)) == NULL) {
        webError(web, 400, "Cannot connect to OpenAI");
        return;
    }
    /*
        Create a proxy connection between the browser and the OpenAI server using WebSockets. 
        We cross link the two WebSocket objects so that we can send messages back and forth.
     */
    urlAsync(up, (WebSocketProc) realTimeCallback, web);
    webAsync(web, (WebSocketProc) browserCallback, up);

    //  Wait till either browser or OpenAI closes the connection
    rYieldFiber(0);

    urlFree(up);
    webFinalize(web);
}

#if EXAMPLES
/*
    Sample inline Response API request without web form to use the OpenAI API.
    This demonstrates how to construct the request JSON object.
 */
PUBLIC void aiResponseExample(void)
{
    Json  *request, *response;
    cchar *model, *text;
    char  buf[1024];

    cchar *vectorId = "PUT_YOUR_VECTOR_ID_HERE";

    /*
        SDEF is used to catentate literal strings into a single string.
        SFMT is used to format strings with variables.
        jsonParse converts the string into a json object.
     */
    model = ioGetConfig("ai.model", "gpt-4o-mini");
    request = jsonParse(SFMT(buf, SDEF({
        model: %s,
        input: 'What is the capital of the moon?',
        tools: [{
            type: 'file_search',
            vector_store_ids: ['%s'],
        }],
    }), model, vectorId), 0);

    response = openaiResponse(request);

    text = jsonGet(response, 0, "output_text", 0);
    printf("Response: %s\n", text);

    jsonFree(request);
    jsonFree(response);
}

PUBLIC void aiChatCompletionExample(void)
{
    Json  *request, *response;
    cchar *text;

    /*
        SDEF is used to catentate literal strings into a single string.
        SFMT is used to format strings with variables.
        jsonParse converts the string into a json object.
     */
    request = jsonParse(\
        "{messages: [{"
            "role: \"system\","
            "content: \"You are a helpful assistant.\""
        "},{"
            "role: \"user\","
            "content: \"What is the capital of the moon?\""
        "}]}", 0);
    jsonPrint(request);
    response = openaiChatCompletion(request);

    text = jsonGet(response, 0, "choices[0].message.content", 0);
    printf("Response: %s\n", text);

    jsonFree(request);
    jsonFree(response);
}

/*
    Get a list of OpenAI models.
 */
PUBLIC void aiGetModelsExample(void)
{
    Json *models;
    JsonNode *child, *data;
    int cid;
    
    models = openaiListModels();
    jsonPrint(models);
    data = jsonGetNode(models, 0, "data");

    //  Iterate over models.data
    for (ITERATE_JSON(models, data, child, cid)) {
        printf("%s\n", jsonGet(models, cid, "id", 0));
    }
    jsonFree(models);
}
#endif

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
