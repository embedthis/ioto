# Blank App

The blank app builds Ioto without an application user interface. 

When Ioto builds, it must resolve application start/stop hook functions. The "blank" provides the required device-side start/stop hooks via a main.c source file.

## Building

To select the "blank" app and build Ioto, type:

    make APP=blank

## Directories

| Directory | Purpose                                               |
| --------- | ------------------------------------------------------|
| config    | Configuration files                                   |
| src       | C source code to link with Ioto                       |

## Key Files

| File                | Purpose                                     |
| ------------------- | --------------------------------------------|
| config/ioto.json5   | Primary Ioto configuration file             |
| src/main.c          | Code to run when Ioto starts/stops          |
