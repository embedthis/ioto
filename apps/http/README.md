# Http (Server) App

The Http app is a simple, local, browser app to test the Ioto web server without any cloud-based services.

## Building

To build Ioto with all apps including the http app, type:

    make

To change which services are enabled, edit the **include/config.h** header file and rebuild:

    make clean build

The Ioto web server will listen on port 80 for HTTP and port 443 for HTTPS. You can change the ports in the web.json5 file.

## Test Users

The Http app defines two test users:

* admin
* guest

Both have a password of "demo". 

You can login using either account and then test accessing the various UI tabs. The guest account will have access to only a subset of pages.


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
| signatures.json5          | HTTP Rest API signature security file     |
| web.json5                 | Embedded web server configuration         |
| src/*.c                   | Device-side app service code              |
