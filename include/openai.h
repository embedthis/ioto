/*
    openai.h - OpenAI API client library for embedded systems

    This library provides a C interface to OpenAI's APIs including Chat Completion,
    Responses API with agent callbacks, Real-Time API with WebSocket connections, and streaming
    responses with Server-Sent Events. Built on the EmbedThis Safe Runtime foundation with
    modular integration for JSON parsing, URL handling, and cryptographic functions.

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
/**
    OpenAI client configuration structure
    @description Contains the configuration settings for connecting to OpenAI services.
    This structure is managed internally by the library.
    @stability Evolving
 */
typedef struct OpenAI {
    char *endpoint;                     /**< OpenAI API endpoint URL (default: https://api.openai.com/v1) */
    char *realTimeEndpoint;             /**< Real-time WebSocket endpoint URL for streaming connections */
    char *headers;                      /**< HTTP headers including authorization bearer token */
    int flags;                          /**< Configuration flags controlling tracing and behavior */
} OpenAI;

/**
    OpenAI Agent callback function for processing responses
    @description This callback is invoked during streaming responses to allow custom processing
    of agent responses. The callback can modify or augment the response data.
    @param name Agent name identifier
    @param request Original JSON request object sent to OpenAI
    @param response JSON response object received from OpenAI
    @param arg User-defined argument passed through from the calling function
    @return Allocated string to be added to the response. Caller must free using rFree. Return NULL if no additional
       content.
    @stability Evolving
 */
typedef char*(*OpenAIAgent)(cchar *name, Json *request, Json *response, void *arg);

/**
    Submit a request to OpenAI Responses API with agent callbacks
    @description Submit a request to the OpenAI Responses API which supports agent-based interactions.
    The API automatically sets default values: model='gpt-4o-mini', truncation='auto'.
    Response text is aggregated into "output_text" field for convenient access.
    Agent callbacks are invoked for processing structured responses.
    @param props JSON object containing Response API parameters. Required fields depend on the specific API endpoint.
    Common fields include 'messages', 'model', 'max_tokens', 'temperature'.
    @param agent Callback function invoked for each response chunk. May be NULL if no agent processing required.
    @param arg User-defined argument passed to the agent callback function. May be NULL.
    @return JSON object containing the complete response from OpenAI. Contains aggregated "output_text" field.
    Returns NULL on failure. Caller must free using jsonFree.
    @stability Evolving
 */
PUBLIC Json *openaiResponses(Json *props, OpenAIAgent agent, void *arg);

/**
    Submit a request to OpenAI Response API with streaming response handling
    @description Submit a request and receive responses via Server-Sent Events (SSE) streaming.
    Default values are automatically set: model='gpt-4o-mini', truncation='auto'.
    The response text is aggregated into "output_text" for convenient access.
    Streaming allows real-time processing of responses as they arrive.
    @param props JSON object containing Response API parameters. Must include required fields for the target endpoint.
    @param callback SSE callback function invoked for each streaming chunk. Cannot be NULL.
    @param arg User-defined argument passed to the callback function. May be NULL.
    @return Url object representing the active streaming connection on success. Returns NULL on failure.
    Use urlClose to terminate the stream when finished.
    @stability Evolving
 */
PUBLIC Url *openaiStream(Json *props, UrlSseProc callback, void *arg);

/**
    Submit a request to OpenAI Chat Completion API
    @description Submit a synchronous request to the OpenAI Chat Completion API for text generation.
    The default model is set to 'gpt-4o-mini' if not specified in props.
    This is a blocking call that returns the complete response.
    @param props JSON object containing Chat Completion API parameters. Common fields include:
    'messages' (required array), 'model', 'max_tokens', 'temperature', 'top_p', 'stream'.
    @return JSON object containing the complete response from OpenAI Chat Completion API.
    Response includes 'choices' array with generated content. Returns NULL on failure.
    Caller must free using jsonFree.
    @stability Evolving
 */
PUBLIC Json *openaiChatCompletion(Json *props);

/**
    Connect to OpenAI Real-Time API via WebSocket
    @description Establish a WebSocket connection to the OpenAI Real-Time API for bidirectional
    real-time communication. This enables voice and streaming text interactions with low latency.
    The connection supports full-duplex communication for interactive applications.
    @param props JSON object containing Real-Time API connection parameters.
    May include 'model', 'voice', 'input_audio_format', 'output_audio_format'.
    @return Url object representing the active WebSocket connection on success.
    Returns NULL on connection failure. Use urlClose to terminate the connection.
    @stability Evolving
 */
PUBLIC Url *openaiRealTimeConnect(Json *props);

/**
    List available OpenAI models
    @description Retrieve a list of all available models from the OpenAI API.
    This includes both OpenAI's base models and any fine-tuned models available to your account.
    @return JSON object containing an array of model objects. Each model object includes:
    'id' (model identifier), 'object' (type), 'created' (timestamp), 'owned_by' (owner).
    Returns NULL on failure. Caller must free using jsonFree.
    @stability Evolving
 */
PUBLIC Json *openaiListModels(void);

#if FUTURE
PUBLIC Json *openaiCreateEmbeddings(cchar *model, cchar *input, cchar *encodingFormat);
PUBLIC Json *openaiFineTune(cchar *data);
#endif

/**
    @name Initialization flags
    @description Configuration flags for controlling OpenAI library behavior and debugging output.
    These flags can be combined using bitwise OR operations.
 */
/**@{*/
#define AI_SHOW_NONE 0x1               /**< Disable all tracing output */
#define AI_SHOW_REQ  0x2               /**< Enable tracing of outgoing requests to OpenAI */
#define AI_SHOW_RESP 0x8               /**< Enable tracing of incoming responses from OpenAI */
/**@}*/

/**
    Initialize the OpenAI API client library
    @description Initialize the OpenAI client with endpoint, authentication, and configuration.
    This must be called before using any other OpenAI API functions.
    The library will validate the API key and establish the connection parameters.
    @param endpoint OpenAI API base endpoint URL. Use NULL for default "https://api.openai.com/v1".
    @param key OpenAI API key for authentication. Must be a valid API key starting with "sk-".
    @param config JSON object containing additional configuration parameters. May be NULL for defaults.
    Optional fields include timeout settings and custom headers.
    @param flags Bitmask of AI_SHOW_* flags controlling debug output and tracing behavior.
    @return Returns 0 on successful initialization. Returns negative error code on failure.
    @stability Internal
 */
PUBLIC int openaiInit(cchar *endpoint, cchar *key, Json *config, int flags);

/**
    Terminate the OpenAI API client library
    @description Clean up resources and terminate the OpenAI client library.
    This should be called when the application no longer needs OpenAI services.
    All active connections and allocated resources will be freed.
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
