Create a web server to serve upload requests
===

This sample shows how to receive upload requests.

This sample uses Ioto to create the web server. It does not provide its own main().

Notes
-----

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
    http://127.0.0.1:8080/upload.html

The server listens on the loopback interface only. Uploaded files are stored in `state/uploads`, a private directory the server creates with mode 0700. Do not set `upload.dir` to a shared directory such as `/tmp`.

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
