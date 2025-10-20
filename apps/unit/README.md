# UNit (Testing) App

The Unit app provides the server side test code and harness for the TestMe unit test suite.

## Building

To select the "unit" app and build Ioto, type:

    make APP=unit

Note: You do not need to use a `configure` program when building via make. Instead, you can edit the **apps/http/ioto.json5** configuration file. Make will copy this file to the **state** directory and generate a **src/config.h** header that will conditionally compile the required services.

If you change the **apps/http/ioto.json5** settings, you may need to rebuild to reflect the changed configuration. To rebuild:

    make APP=unit clean build

## Services

The configured services for the Http app are:

* database -- Enable the embedded database
* register -- Register with the Ioto Builder
* serialize -- Run a serialization service when making the device
* sync -- Enable the embedded database sync service
* url -- Enable client HTTP request support
* web -- Enable the local embedded web server

The Ioto web server will listen on port 80 for HTTP and port 443 for HTTPS. You can change the ports in the web.json5 file.

## Directories

| Directory | Purpose                                               |
| --------- | ------------------------------------------------------|
| config    | Configuration files                                   |
| src       | C source code to link with Ioto                       |

## Key Files

| File                      | Purpose                                   |
| ------------------------- | ------------------------------------------|
| config/db.json5           | Database seed data with test users        |
| config/ioto.json5         | Primary Ioto configuration file           |
| config/schema.json5       | Database schema file                      |
| config/web.json5          | Embedded web server configuration         |
| src/*.c                   | Device-side app service code              |
