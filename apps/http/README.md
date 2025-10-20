# Http (Server) App

The Http app is a simple, local, browser app to test the Ioto web server without any cloud-based services..

## Building

To select the "http" app and build Ioto, type:

    make APP=http

Note: You do not need to use a `configure` program when building via make. Instead, you can edit the **apps/http/ioto.json5** configuration file. Make will copy this file to the **state** directory and generate a **src/config.h** header that will conditionally compile the required services.

If you change the **apps/http/ioto.json5** settings, you may need to rebuild to reflect the changed configuration. To rebuild:

    make APP=http clean build

## Services

The configured services for the Http app are:

* database -- Enable the embedded database
* register -- Register with the Ioto Builder
* serialize -- Run a serialization service when making the device
* url -- Enable client HTTP request support
* web -- Enable the local embedded web server

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
| config    | Configuration files                                   |
| src       | C source code to link with Ioto                       |

## Key Files

| File                      | Purpose                                   |
| ------------------------- | ------------------------------------------|
| config/db.json5           | Database seed data with test users        |
| config/ioto.json5         | Primary Ioto configuration file           |
| config/schema.json5       | Database schema file                      |
| config/signature.json5    | HTTP Rest API signature security file     |
| config/web.json5          | Embedded web server configuration         |
| src/*.c                   | Device-side app service code              |
