/*
    auth-config.tst.c - Test authentication configuration loading

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void testAuthConfig()
{
    Json    *config;
    WebHost *host;
    WebUser *user;
    char    configStr[1024];

    webInit();

    // Create minimal test configuration with authentication
    config = jsonParse(SFMT(configStr, SDEF({
        web: {
            listen: ['http://localhost:4100'],
            auth: {
                realm: 'Test Realm',
                authType: 'digest',
                algorithm: 'SHA-256',
                secret: 'test-secret-1234567890abcdef',
                roles: {
                    public: [],
                    user: ['view', 'read'],
                    admin: ['user', 'edit', 'delete']
                },
                users: {
                    alice: {
                        password: 'a665a45920422f9d417e4867efdc4fb8a04a1f3fff1fa07e998e86f7f7a27ae3',
                        role: 'admin'
                    },
                    bob: {
                        password: '5d41402abc4b2a76b9719d911017c592',
                        role: 'user'
                    }
                }
            }
        }
    })), 0);

    ttrue(config != NULL, "Config should parse successfully");
    if (!config) {
        webTerm();
        return;
    }

    // Allocate host with authentication config
    host = webAllocHost(config, 0);
    ttrue(host != NULL, "Host should allocate successfully");
    if (!host) {
        jsonFree(config);
        webTerm();
        return;
    }

    // Verify authentication settings loaded
    ttrue(host->users != NULL, "Users hash should be allocated");
    ttrue(host->realm != NULL, "Realm should be set");
    ttrue(smatch(host->realm, "Test Realm"), "Realm should match config");
    ttrue(smatch(host->authType, "digest"), "Auth type should be digest");
    ttrue(smatch(host->algorithm, "SHA-256"), "Algorithm should be SHA-256");
    ttrue(smatch(host->secret, "test-secret-1234567890abcdef"), "Secret should match");

    // Verify users loaded
    user = webLookupUser(host, "alice");
    ttrue(user != NULL, "User alice should exist");
    if (user) {
        ttrue(smatch(user->username, "alice"), "Username should be alice");
        ttrue(user->password != NULL, "Password should be set");
        ttrue(smatch(user->role, "admin"), "Role should be admin");
        ttrue(user->abilities != NULL, "Abilities should be computed");

        // Check that admin has inherited abilities from user role
        ttrue(webUserCan(user, "view"), "Admin should have view ability from user role");
        ttrue(webUserCan(user, "edit"), "Admin should have edit ability");
    }

    user = webLookupUser(host, "bob");
    ttrue(user != NULL, "User bob should exist");
    if (user) {
        ttrue(smatch(user->username, "bob"), "Username should be bob");
        ttrue(smatch(user->role, "user"), "Role should be user");
        ttrue(webUserCan(user, "view"), "User should have view ability");
        ttrue(!webUserCan(user, "edit"), "User should NOT have edit ability");
    }

    // Verify non-existent user
    user = webLookupUser(host, "charlie");
    ttrue(user == NULL, "Non-existent user should return NULL");

    // Cleanup
    webFreeHost(host);
    webTerm();

    tinfo("Configuration loading tests passed");
}

static void fiberMain(void *arg)
{
    testAuthConfig();
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
