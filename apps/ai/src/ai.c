/*
    ai.c -- Demonstration of AI OpenAI APIs
 */
/********************************** Includes **********************************/

#include "ioto.h"

/*********************************** Forwards *********************************/

static void aiChatCompletionAction(Web *web);
static void aiResponsesAction(Web *web);
static void aiPatientAction(Web *web);
static void aiStreamAction(Web *web);
static void aiChatRealTimeAction(Web *web);
static char *runAgentWorkflow(cchar *input, OpenAIAgent agent, void *arg);

// #define EXAMPLES 1
#if EXAMPLES
static void aiResponsesExample(void);
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
        //  Web page actions to invoke tests
        webAddAction(ioto->webHost, "/ai/responses", aiResponsesAction, NULL);
        webAddAction(ioto->webHost, "/ai/stream", aiStreamAction, NULL);
        webAddAction(ioto->webHost, "/ai/completion", aiChatCompletionAction, NULL);
        webAddAction(ioto->webHost, "/ai/realtime", aiChatRealTimeAction, NULL);
        webAddAction(ioto->webHost, "/ai/patient", aiPatientAction, NULL);
        rInfo("ai", "AI started\n");

#if EXAMPLES
        // Stand-alone examples
        aiResponsesExample();
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
    Sample web form to use the OpenAI Chat Completion API with chat.html.
 */
static void aiChatCompletionAction(Web *web)
{
    Json *response;

    if ((response = openaiChatCompletion(web->vars)) == NULL) {
        webError(web, 500, "Cannot issue request to OpenAI");
    } else {
        webWriteJson(web, response);
    }
    webFinalize(web);
    jsonFree(response);
}

/*
    Sample web form to use the OpenAI Responses API with responses.html
 */
static void aiResponsesAction(Web *web)
{
    Json *response;

    if ((response = openaiResponses(web->vars, NULL, 0)) == NULL) {
        webError(web, 500, "Cannot issue request to OpenAI");
    } else {
        webWriteJson(web, response);
    }
    webFinalize(web);
    jsonFree(response);
}

/*
    Get temperature agent. Part of the patient.html demo.
 */
static char *getTemp(void)
{
    static cchar *temps[] = { "36", "37", "38", "39", "40", "41", "42" };
    static int   index = 0;

    if (index >= sizeof(temps) / sizeof(temps[0])) {
        index = 0;
    }
    return sclone(temps[index++]);
}

/*
    Get emergency ambulance. Part of the patient.html demo.
 */
static char *callEmergency(void)
{
    rInfo("ai", "Calling demo ambulance");
    return sclone("Ambulance dispatched");
}

/*
    Patient.html agent callback.
 */
static char *agentCallback(cchar *name, Json *request, Json *response, void *arg)
{
    if (smatch(name, "getTemp")) {
        return getTemp();
    } else if (smatch(name, "callEmergency")) {
        return callEmergency();
    }
    return sclone("Unknown function, cannot comply with request.");
}

/*
    Web action to start the patient agent workflow. Uses the OpenAI Responses API with patient.html
 */
static void aiPatientAction(Web *web)
{
    cchar *input;
    char  *output;

    input = "How is the patient doing?";
    if ((output = runAgentWorkflow(input, agentCallback, 0)) == NULL) {
        webError(web, 500, "Cannot issue request to OpenAI");
    } else {
        webWrite(web, output, -1);
    }
    rFree(output);
    webFinalize(web);
}

/*
    This is a test of the AI agentic workflow. The device measures the patient's temperature and sends it to the AI.
    The AI then determines if the patient is in urgent need of medical attention. If so, it responds to have the
    device call the ambulance by using the local callEmergency() function.
 */
static char *runAgentWorkflow(cchar *input, OpenAIAgent agent, void *arg)
{
    Json *request, *response;
    char *text;

    request = jsonAlloc(0);
    jsonSetString(request, 0, "input", input);
    jsonSetString(request, 0, "model", ioGetConfig("ai.model", "gpt-4o-mini"));

    jsonSetString(request, 0, "instructions",
                  "Your are a doctor. You are given a patient temperature and you need to determine if the patient is in urgent need of medical attention. If so, call emergency response by using the callEmergency() function. In your response, state the patient's temperature in C and the result of your assessment. Do not give any other information.");

    jsonSetJsonFmt(request, 0, "tools", "%s",
                   SDEF([{
                             type: 'function',
                             name: 'getTemp',
                             description: 'Get the patient temperature',
                         }, {
                             type: 'function',
                             name: 'callEmergency',
                             description: 'Call emergency response as the patient is critically ill',
                         }]));

    /*
        This call may issue multiple requests to the OpenAI API. OpenAI will respond and may request that
        the agents/tools getTemp() and callEmergency() be called. The agentCallback() function will be called
        to handle the tool calls and then the result will be sent back to OpenAI to assess and determine a response.
     */
    if ((response = openaiResponses(request, agent, arg)) == NULL) {
        jsonFree(request);
        return sclone("Cannot determine treatment for patient.");
    }
    text = sclone(jsonGet(response, 0, "output_text", 0));
    jsonFree(request);
    jsonFree(response);
    return text;
}

/*
    Sample web form to use the streamed OpenAI Responses API.
 */
static void aiStreamCallback(Url *up, ssize id, cchar *event, cchar *data, void *arg)
{
    Web *web = arg;

    webWriteFmt(web, "id: %ld\nevent: %s\ndata: %s\n", id, event, data);
}

static void aiStreamAction(Web *web)
{
    Url *up;

    up = openaiStream(web->vars, (UrlSseProc) aiStreamCallback, web);
    if (up == NULL) {
        webError(web, 500, "Cannot connect to OpenAI");
        return;
    }
    urlWait(up);
    urlFree(up);
    webFinalize(web);
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
    urlWebSocketAsync(up, (WebSocketProc) realTimeCallback, web);
    webAsync(web, (WebSocketProc) browserCallback, up);

    //  Wait till either browser or OpenAI closes the connection
    rYieldFiber(0);

    urlFree(up);
    webFinalize(web);
}

#if EXAMPLES
/*
    Sample inline Responses API request without web form to use the OpenAI API.
    This demonstrates how to construct the request JSON object.
 */
PUBLIC void aiResponsesExample(void)
{
    Json  *request, *response;
    cchar *model, *text;
    char  buf[1024];

    cchar *vectorId = "PUT_YOUR_VECTOR_ID_HERE";

    /*
        SDEF is used to concatenate literal strings into a single string.
        SFMT is used to format strings with variables.
        jsonParse converts the string into a json object.
     */
    model = ioGetConfig("ai.model", "gpt-4o-mini");
    request = jsonParse(SFMT(buf, SDEF({
        model: '%s',
        input: 'What is the capital of the moon?',
        tools: [{
                    type: 'file_search',
                    vector_store_ids: ['%s'],
                }],
    }), model, vectorId), 0);

    response = openaiResponses(request, NULL, 0);

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
        SDEF is used to concatenate literal strings into a single string.
        SFMT is used to format strings with variables.
        jsonParse converts the string into a json object.
     */
    request = jsonParse( \
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
    Json     *models;
    JsonNode *child, *data;

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
