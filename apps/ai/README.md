# AI App

The AI app is an app that demonstrates using the OpenAI LLM APIs.

The AI App demonstrates:

* How to download and build the Ioto agent with custom extensions
* How to create a custom database schema
* How to create a device cloud and manager app
* How to send device data to the cloud

This sample will:

* Using the OpenAI Chat Complations API.
* Using the new OpenAI Chat Response API.
* Using the OpenAI Chat Real-Time API.

The AI App can be used regardless of whether cloud-based management is enabled.

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

* Using the OpenAI Chat Complations API.
* Using the new OpenAI Chat Response API.
* Using the OpenAI Chat Real-Time API.

It will also create create 3 web pages:

* chat.html - to demonstrate the Chat Completions API
* response.html - to demonstarte the Response API
* realtime.html - to demonstrate the Real-Time API

## How It Works

Each web page is a simple ChatBot that issues requests to the Ioto local web server. These web pages are similar to the consumer ChatGPT web site. 

The `aiApp` registers a web request action handler that responds to the web requests and in-turn issues API calls to the OpenAI service. Responses are then passed back to the web page to display.

### Code

The `apps/ai/src/aiApp.c` contains the source code for the app. There are also some commented out EXAMPLE sections that demonstrate using the APIs without web pages.

This app uses the Ioto `ioStart` and `ioStop` hooks to start and stop the app. When linked with the Ioto agent library, these hooks replace the stub functions and are called by Ioto on startup and shutdown.

The ioStart routine checks if the `ai.enable` property is true in the `ioto.json5` configuration file. If true, it registers the web action handlers.

## Directories

| Directory | Purpose                      |
| --------- | -----------------------------|
| config    | Configuration files          |
| src       | AI App C source code       |

## Key Files

| File                      | Purpose                                   |
| ------------------------- | ------------------------------------------|
| config/ioto.json5         | Primary Ioto configuration file           |
| schema.json5              | Complete database schema file             |
| src/*.c                   | Device-side app service code              |
