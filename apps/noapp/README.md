# NoApp

The noapp app builds Ioto without an application user interface. 

When Ioto builds, it must resolve application start/stop hook functions. The "noapp" provides the required device-side start/stop hooks via a main.c source file.

## Building

To select the "noapp" app and build Ioto, type:

    make APP=noapp

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
