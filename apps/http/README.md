# Http (Server) App

The Http app is a simple, local, browser app to test the Ioto web server without any cloud-based services.

## Building

To build Ioto with all apps including the http app, type:

    make

To change which services are enabled, edit the **include/config.h** header file and rebuild:

    make clean build

The Ioto web server listens on 127.0.0.1 port 8080 for HTTP and port 4443 for HTTPS. You can change the endpoints in the web.json5 file.

## User Accounts

This app ships with **no user accounts**, so no login will succeed until you create one. That is deliberate: it previously seeded `admin`, `guest` and `super` accounts that all shared the password "demo", and those hashes were published in the source distribution, so any device built from this template carried a known administrative password. The agent refuses a login with the password "demo" and reports any account that still has it when it starts.

Generate a password hash from the top of the Ioto source tree after `make`. The hash covers the username and the realm, so give the username the account will log in with, and the realm the server uses: `auth.realm` in `web.json5`, else `name`, else `web`.

```bash
build/bin/password --cipher blowfish --realm web --password 'your-password' admin
```

It prints the record to add to the `User` table in `db.json5`; set the role you want:

```json5
{
    User: [
        { username: 'admin', password: 'BF1:...', role: 'admin' },
    ],
}
```

`db.json5` is loaded only when the `User` table is empty, so it seeds on first run and never overwrites accounts you have since created. To apply a changed `db.json5`, remove `state/db/` (if the database holds nothing else) and restart. The roles available are defined under `auth.roles` in `web.json5`: `user`, `admin`, `owner` and `super`, each inheriting the one before it. A `user` account sees only a subset of the UI tabs, which is the quickest way to check that authorization is working.

Login and logout are the agent's built-in handlers at `auth.login` (`/api/public/login`) and `auth.logout` (`/api/public/logout`, POST only). A failed login answers `401 Invalid username or password` whatever the cause, and is logged under the `auth` source. After 10 failures from one client address within 5 minutes, further attempts are answered `429` until the 5 minutes have passed (`auth.maxFailures`, `auth.failurePeriod`). Changing a user's role or removing a user in the `User` table takes effect immediately: the user's open sessions end and the next login carries the new role.

The pages under `site/` load their script from `/public/auth.js`, so they run under the `default-src 'self'` Content-Security-Policy set in `web.json5`. Keep page logic in external scripts rather than weakening the policy.


## Directories

| Directory | Purpose                                               |
| --------- | ------------------------------------------------------|
| src       | C source code to link with Ioto                       |

## Key Files

| File                      | Purpose                                   |
| ------------------------- | ------------------------------------------|
| db.json5                  | Database seed data (defines no users)     |
| ioto.json5                | Primary Ioto configuration file           |
| schema.json5              | Database schema file                      |
| signatures.json5          | HTTP Rest API signature security file     |
| web.json5                 | Embedded web server configuration         |
| src/*.c                   | Device-side app service code              |
