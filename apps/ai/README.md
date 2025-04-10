# AI App

The AI app is an app that demonstrates using the OpenAI LLM APIs.

This sample will demonstrate how to use the OpenAI api to:

* Use the OpenAI Chat Complations API (chat.html).
* Use the new OpenAI Response API (responses.html).
* Use the new OpenAI Response API with streaming (stream.html).
* Use the new OpenAI Response API and invoke sub-agents (patient.html).
* Use the OpenAI Chat Real-Time API (realtime.html).

The AI App does not require cloud-based management to be enabled.

## Steps

<!-- no toc -->
- [Download Agent](#download-agent)
- [Build Agent](#build-agent)

### Download Ioto Agent

To download the Ioto agent, click `Download` from the Builder Products list and save the source distribution to your system. The eval version of Ioto will be fine for this solution.

<img src="https://www.embedthis.com/images/builder/product-list.avif" alt="Product List"><br>

### Build Agent

To build the Ioto agent with AI extensions, first extract the source files from the downloaded archive:

    $ tar xvfz ioto-eval-src.tgz

Then build Ioto with the AI app, by typing:

    $ make APP=ai

This will build Ioto, the AI app and will copy the AI config files to the top-level `state/config` directory.

The `state/site` directory will contain some test web pages.

### Run Agent

The easiest way to run the Ioto agent with AI extensions is to type:

```bash
$ make run
```

### Using the AI Demo

When Ioto is run, the AI app will register 3 action handlers to demonstrate:

* aiChatCompletionAction - For the OpenAI Chat Complations API.
* aiResponsesAction - For the new OpenAI Chat Responses API.
* aiPatientAction - For the OpenAI Chat Responses API and invoke sub-agents.
* aiStreamAction - For the OpenAI Chat Responses API in streaming mode.
* aiChatRealTimeAction - For the OpenAI Chat Real-Time API.

It will also create create 5 web pages:

* chat.html - to demonstrate the Chat Completions API
* responses.html - to demonstarte the Response API
* patient.html - to demonstarte the Response API with sub-agents
* stream.html - to demonstarte the Response API in streaming mode
* realtime.html - to demonstrate the Real-Time API

## How It Works

The **patient.html** test demonstrates an AI agentic workflow. The demo measures the patient's temperature on the device by calling getTemp() and sends it to the cloud LLM which determines if the patient is in urgent need of medical attention. If so, it responds to have the device workflow call the ambulance by using the local callEmergency() function. The web page has two buttons to start and stop the monitoring process.

The other demos use a web page similar to the consumer ChatGPT web site. Each web page is a simple ChatBot that issues requests to the Ioto local web server. These requests are then relayed to the OpenAI service and the responses are passed back to the web page to display.

The `aiApp` registers a web request action handler that responds to the web requests and in-turn issues API calls to the OpenAI service. Responses are then passed back to the web page to display.

### Code

The `apps/ai/src/aiApp.c` contains the source code for the app. There are also some commented out EXAMPLE sections that demonstrate using the APIs without web pages.

This app uses the Ioto `ioStart` and `ioStop` hooks to start and stop the app. When linked with the Ioto agent library, these hooks replace the stub functions and are called by Ioto on startup and shutdown.

The ioStart routine checks if the `ai.enable` property is true in the `ioto.json5` configuration file. If true, it registers the web action handlers.

## Directories

| Directory | Purpose                      |
| --------- | -----------------------------|
| config    | Configuration files          |
| src       | AI App C source code         |

## Key Files

| File                      | Purpose                                   |
| ------------------------- | ------------------------------------------|
| config/ioto.json5         | Primary Ioto configuration file           |
| schema.json5              | Complete database schema file             |
| src/*.c                   | Device-side app service code              |
