Embedthis Ioto Samples
===

This directory contains samples for the Ioto Agent and Ioto API.

The Ioto Agent samples demonstrate how to build and link code with the Ioto Agent in various ways.
The Ioto API samples are shell scripts that demonstrate how to use the Ioto API to interact with the Ioto Cloud.

The samples are under two directories:

Directory | Description
--------- | -----------
[api](api)     | API script samples for the Ioto Cloud
[agent](agent) | Samples for the Ioto Agent

## API Samples

The `api` directory contains a set of shell scripts to demonstrate various API calls to the Ioto Cloud API.

File | Description
---- | -----------
[device-command.sh](api/device-command.sh) | Send a command to a device
[find-items.sh](api/find-items.sh) | Find items in the Ioto Cloud database
[get-item.sh](api/get-item.sh) | Get an item from the Ioto Cloud database
[get-metric.sh](api/get-metric.sh) | Get a metric from the Ioto Cloud database
[login.sh](api/login.sh) | Authenticate as a login user with the Ioto Cloud
[set-item.sh](api/set-item.sh) | Set an item in the Ioto Cloud database
[update.sh](api/update.sh) | Check with the cloud for device updates

The `login.sh` script demonstrates how to authenticate as a login user with the Ioto Cloud. Most other scripts
require a valid API token. Each script requires a set of environment variables to be set that is documented in the script.

Variable | Description
-|-
IOTO_COGNITO | The Cognito endpoint to use for authentication
IOTO_CLIENT_ID | The Cognito client ID to use for authentication
IOTO_USERNAME | The username of the user to authenticate as
IOTO_PASSWORD | The password of the user to authenticate as
IOTO_API | The API endpoint of the Ioto Cloud
IOTO_HOST | The hostname of the Ioto Cloud
IOTO_TOKEN | The CloudAPI token to use for the Ioto Cloud API

## Agent Samples

These samples are configured to use a locally built Ioto with all services enabled.

The Makefiles assume GCC/Clang on Linux or Mac.

The following samples are available:

Directory | Description
-|-
[aws-s3](agent/aws-s3/README.md)                            | Send a file to AWS S3
[link-agent-main](agent/link-agent-main/README.md)          | Embed Ioto into an application and use the Ioto main()
[mqtt](agent/mqtt/README.md)                                | Basic use of the MQTT API
[own-main](agent/own-main/README.md)                        | Embed Ioto into a main program using the embedding API
[thread](agent/thread/README.md)                            | How to create and communicate with threads
[url-fetch](agent/url-fetch/README.md)                      | Use urlFetch API to fetch a URL from a web site
[url-get](agent/url-get/README.md)                          | Use urlGet API to get a URL from example.com
[web-auth](agent/web-auth/README.md)                        | How to authenticate requests to the web server
[web-dynamic](agent/web-dynamic/README.md)                  | How to generate dynamic responses from the web server
[web-static](agent/web-static/README.md)                    | How to create a web server to serve static requests
[web-upload](agent/web-upload/README.md)                    | How to receive file upload requests

### Building

You will need to download and build Ioto and then modify the defines.mk to point to your Ioto directory. 
Go to the Builder to download Ioto:

    [https://admin.embedthis.com/](https://admin.embedthis.com)

To build the samples, see the per-sample README instructions. Many can run without extra build steps.

To build all, use:

    make build

### Documentation

The full product documentation is supplied in HTML format under the doc directory. This is also available online at:

    https://www.embedthis.com/ioto/doc/

### Licensing

    https://www.embedthis.com/about/terms.html

### Support

Support is available via the Builder portal at:

    https://admin.embedthis.com/

### Copyright

Copyright (c) Embedthis Software. All Rights Reserved. Embedthis and Appweb are trademarks of
Embedthis Software, LLC. Other brands and their products are trademarks of their respective holders.
