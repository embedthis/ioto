Embedthis Ioto Samples
===

These samples are configured to use a locally built Ioto with all services enabled.

The Makefiles assume GCC/Clang on Linux or Mac.

The following samples are available:

* [aws-s3](aws-s3/README.md)                            Send a file to AWS S3
* [link-agent-main](link-agent-main/README.md)          Embed Ioto into an application and use the Ioto main()
* [mqtt](mqtt/README.md)                                Basic use of the MQTT API
* [own-main](own-main/README.md)                        Embed Ioto into a main program using the embedding API
* [thread](thread/README.md)                            How to create and communicate with threads
* [url-fetch](url-fetch/README.md)                      Use urlFetch API to fetch a URL from a web site
* [url-get](url-get/README.md)                          Use urlGet API to get a URL from example.com
* [web-auth](web-auth/README.md)                        How to authenticate requests to the web server
* [web-dynamic](web-dynamic/README.md)                  How to generate dynamic responses from the web server
* [web-static](web-static/README.md)                    How to create a web server to serve static requests
* [web-upload](web-upload/README.md)                    How to receive file upload requests

### Building

You will need to download and build Ioto and then modify the defines.mk to point to your Ioto directory.

* Ioto - [https://admin.embedthis.com/](https://admin.embedthis.com)

To build the samples, see the per-sample README instructions. Many can run without extra build steps.

To build all, use:

    make build

### Documentation

The full product documentation is supplied in HTML format under the doc directory. This is also available online at:

* https://www.embedthis.com/ioto/doc/


### Licensing

Please see:

* https://www.embedthis.com/about/terms.html


Support
---

Support is available via the Builder portal at:

https://admin.embedthis.com/

Copyright
---

Copyright (c) Embedthis Software. All Rights Reserved. Embedthis and Appweb are trademarks of
Embedthis Software, LLC. Other brands and their products are trademarks of their respective holders.
