/*
    auth-roles-array.tst.c - Test legacy array format for roles configuration

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void testLegacyRolesArray()
{
    Json    *config;
    WebHost *host;
    WebUser *user;
    char    configStr[1024];

    webInit();

    // Create minimal test configuration with legacy array-based roles
    config = jsonParse(SFMT(configStr, SDEF({
        web: {
            listen: ['http://localhost:4100'],
            auth: {
                realm: 'Test Realm',
                authType: 'digest',
                algorithm: 'SHA-256',
                secret: 'test-secret-1234567890abcdef',
                roles: ['user', 'admin', 'owner', 'super'],
                users: {
                    alice: {
                        password: 'a665a45920422f9d417e4867efdc4fb8a04a1f3fff1fa07e998e86f7f7a27ae3',
                        role: 'admin'
                    },
                    bob: {
                        password: '5d41402abc4b2a76b9719d911017c592',
                        role: 'user'
                    },
                    charlie: {
                        password: 'a665a45920422f9d417e4867efdc4fb8a04a1f3fff1fa07e998e86f7f7a27ae3',
                        role: 'super'
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

    // Test bob (user role - first in array)
    user = webLookupUser(host, "bob");
    ttrue(user != NULL, "User bob should exist");
    if (user) {
        ttrue(smatch(user->role, "user"), "Bob's role should be user");
        ttrue(webUserCan(user, "public"), "User role should have public ability");
        ttrue(webUserCan(user, "user"), "User role should have user ability");
        ttrue(!webUserCan(user, "admin"), "User role should NOT have admin ability");
        ttrue(!webUserCan(user, "owner"), "User role should NOT have owner ability");
        ttrue(!webUserCan(user, "super"), "User role should NOT have super ability");
    }

    // Test alice (admin role - second in array)
    user = webLookupUser(host, "alice");
    ttrue(user != NULL, "User alice should exist");
    if (user) {
        ttrue(smatch(user->role, "admin"), "Alice's role should be admin");
        ttrue(webUserCan(user, "public"), "Admin role should have public ability");
        ttrue(webUserCan(user, "user"), "Admin role should inherit user ability");
        ttrue(webUserCan(user, "admin"), "Admin role should have admin ability");
        ttrue(!webUserCan(user, "owner"), "Admin role should NOT have owner ability");
        ttrue(!webUserCan(user, "super"), "Admin role should NOT have super ability");
    }

    // Test charlie (super role - last in array)
    user = webLookupUser(host, "charlie");
    ttrue(user != NULL, "User charlie should exist");
    if (user) {
        ttrue(smatch(user->role, "super"), "Charlie's role should be super");
        ttrue(webUserCan(user, "public"), "Super role should have public ability");
        ttrue(webUserCan(user, "user"), "Super role should inherit user ability");
        ttrue(webUserCan(user, "admin"), "Super role should inherit admin ability");
        ttrue(webUserCan(user, "owner"), "Super role should inherit owner ability");
        ttrue(webUserCan(user, "super"), "Super role should have super ability");
    }

    // Cleanup
    webFreeHost(host);
    webTerm();

    tinfo("Legacy array format tests passed");
}

static void fiberMain(void *arg)
{
    testLegacyRolesArray();
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
