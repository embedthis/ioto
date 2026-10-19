Get a URL
===

This sample demonstrates the urlGet API to get a URL from example.com.

This sample does not provide its own main().

It verifies the web site's certificate against the root CA bundle configured by `tls.authority` in `ioto.json5` (by default `certs/roots.crt`).

Requirements
---
* [Ioto](https://www.embedthis.com/ioto/download.html)

To build:
---
    make

To run:
---
    make run

or
    ./sample URL

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
