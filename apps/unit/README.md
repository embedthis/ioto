# UNit (Testing) App

The Unit app provides the server side test code and harness for the TestMe unit test suite.

## Building

To build Ioto with all apps including the unit app, type:

    make

To build just the unit app:

    make unit

To change which services are enabled, edit the **include/config.h** header file and rebuild:

    make clean build

The Ioto web server will listen on port 80 for HTTP and port 443 for HTTPS. You can change the ports in the web.json5 file.

## Directories

| Directory | Purpose                                               |
| --------- | ------------------------------------------------------|
| src       | C source code to link with Ioto                       |

## Key Files

| File                      | Purpose                                   |
| ------------------------- | ------------------------------------------|
| db.json5                  | Database seed data with test users        |
| ioto.json5                | Primary Ioto configuration file           |
| schema.json5              | Database schema file                      |
| web.json5                 | Embedded web server configuration         |
| src/*.c                   | Device-side app service code              |
