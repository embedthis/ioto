Authenticate web requests
===

This sample shows how to authenticate requests to the web server with a login form and a session.

The login action verifies the password against the stored hash with `webVerifyUserPassword` and logs the user in with the role configured for that user, never a role taken from the request. Every page except `login.html` requires the `user` role. Logout is a POST.

Notes
-----

You can use the Ioto program as a web server or write your own main() and link against the Ioto library.

Requirements
---
* [Ioto](https://www.embedthis.com/ioto/download.html)

To build:
---
    make

Create an account:
---
No account ships with the sample. Generate a password hash with the `password` command from the Ioto build (`build/bin/password`):

    password --algorithm bcrypt --realm web --password 'CHOOSE-A-PASSWORD' alice

Add the `alice: { password: ..., role: 'user' }` entry it prints to `auth.users` in [web.json5](web.json5), without the trailing `#` comment, which is not valid JSON5. The `--realm` must match `auth.realm`, as the realm is part of the hash.

To run:
---
    make run

Then browse to:
    http://127.0.0.1:8080/login.html

The server listens on the loopback interface only.

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
