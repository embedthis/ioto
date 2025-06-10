Creating a static web server.
===

This sample shows how to create a web server using the Ioto library to serve static requests.
It provides its own main() and does not start other Ioto services.

Notes
-----

You can use the supplied Ioto program as a web server or write your own main() and link against the Ioto library.

Requirements
---
* [Ioto](https://www.embedthis.com/ioto/download.html)

To build:
---
    make

To run:
---
    make run

Then browse to:
    http://localhost/index.html

Code:
---
* [Makefile](Makefile) - Makefile build instructions
* [sample.c](sample.c) - Main program

Documentation:
---
* [Ioto Documentation](https://www.embedthis.com/ioto/doc/index.html)

<!--
See Also:
---
* [typical-server - Fully featured server and embedding API](../typical-server/README.md)
-->
