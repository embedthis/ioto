# ESP32-Blink App

The Blink app is a GPIO LED blinking app. This app is useful to test the building, flashing and execution of an Ioto app on an ESP32 board.

The Demo App demonstrates

* How to build the Ioto component
* How to add code to call Ioto 
* Read Ioto configuration
* Blink a LED

This sample will:

* Create an app using the ESP-IDF
* Download and build the Ioto device agent component
* Build a flashable image for an ESP32 device.

Please read the [README-ESP32.md](../../README-ESP32.md) first for details about building and flashing ESP32 apps with Ioto.

### Background

The Ioto device agent is extended via an Demo module. There are three files:

File | Description
-|-
main.c | Code for the Demo extension

This module uses the Ioto `ioStart` and `ioStop` hooks to start and stop the extension. When linked with the Ioto agent library.
