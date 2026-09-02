# Blank App

The blank app builds Ioto without an application user interface. 

When Ioto builds, it must resolve application start/stop hook functions. The "blank" provides the required device-side start/stop hooks via a main.c source file.

## Building

To build Ioto with all apps including the blank app, type:

    make

To build just the blank app:

    make blank

## Directories

| Directory | Purpose                                               |
| --------- | ------------------------------------------------------|
| src       | C source code to link with Ioto                       |

## Key Files

| File                | Purpose                                     |
| ------------------- | --------------------------------------------|
| ioto.json5          | Primary Ioto configuration file             |
| src/main.c          | Code to run when Ioto starts/stops          |
