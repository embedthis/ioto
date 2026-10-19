/* upload-dir.tst.c -- Upload directory defaults and startup validation. */
#include "test.h"

#if ME_WIN_LIKE
    #include <aclapi.h>
#endif

static void expectResult(int result, int expected)
{
    teqi(result, expected);
}

static WebHost *makeHost(cchar *dir)
{
    Json *config;
    WebHost *host;

    config = jsonParse("{web:{documents:'./site'}}", 0);
    if (dir) {
        jsonSet(config, 0, "web.upload.dir", dir, JSON_STRING);
    }
    host = webAllocHost(config, 0);
    if (host) host->freeConfig = 1;
    return host;
}

#if ME_WIN_LIKE
/*
    Inspect the DACL independently: write permissions must name the process user, LocalSystem, or Administrators.
 */
static bool onlyPrivateWriters(cchar *path)
{
    PSECURITY_DESCRIPTOR security;
    ACCESS_ALLOWED_ACE   *allow;
    ACE_HEADER           *ace;
    TOKEN_USER           *user;
    HANDLE               token;
    PACL                 acl;
    DWORD                i, len;
    bool                 result;

    security = NULL;
    token = NULL;
    user = NULL;
    acl = NULL;
    result = 0;
    if (GetNamedSecurityInfoA((LPSTR) path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &acl, NULL,
                              &security) != ERROR_SUCCESS || !acl) {
        goto done;
    }
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        goto done;
    }
    len = 0;
    GetTokenInformation(token, TokenUser, NULL, 0, &len);
    user = rAlloc(len);
    if (!GetTokenInformation(token, TokenUser, user, len, &len)) {
        goto done;
    }
    for (i = 0; i < acl->AceCount; i++) {
        if (!GetAce(acl, i, (void**) &ace)) {
            goto done;
        }
        if (ace->AceType != ACCESS_ALLOWED_ACE_TYPE || (ace->AceFlags & INHERIT_ONLY_ACE)) {
            continue;
        }
        allow = (ACCESS_ALLOWED_ACE*) ace;
        if ((allow->Mask & (GENERIC_ALL | GENERIC_WRITE | FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY |
                            FILE_DELETE_CHILD | DELETE | WRITE_DAC | WRITE_OWNER)) &&
            !EqualSid(&allow->SidStart, user->User.Sid) &&
            !IsWellKnownSid(&allow->SidStart, WinLocalSystemSid) &&
            !IsWellKnownSid(&allow->SidStart, WinBuiltinAdministratorsSid)) {
            goto done;
        }
    }
    result = 1;
done:
    if (token) CloseHandle(token);
    rFree(user);
    if (security) LocalFree(security);
    return result;
}
#endif

#if ME_WIN_LIKE
/*
    Add an inheritable ACE granting BUILTIN\Users modify access, as a permissive parent would carry.
 */
static bool grantUsersWrite(cchar *path)
{
    PSECURITY_DESCRIPTOR security;
    EXPLICIT_ACCESS_A    ea;
    PACL                 acl, updated;
    BYTE                 sid[SECURITY_MAX_SID_SIZE];
    DWORD                sidLen;
    bool                 result;

    security = NULL;
    updated = NULL;
    acl = NULL;
    result = 0;
    sidLen = sizeof(sid);
    if (!CreateWellKnownSid(WinBuiltinUsersSid, NULL, sid, &sidLen)) {
        return 0;
    }
    if (GetNamedSecurityInfoA((LPSTR) path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &acl, NULL,
                              &security) != ERROR_SUCCESS) {
        return 0;
    }
    memset(&ea, 0, sizeof(ea));
    ea.grfAccessPermissions = FILE_GENERIC_READ | FILE_GENERIC_WRITE | FILE_GENERIC_EXECUTE | DELETE;
    ea.grfAccessMode = GRANT_ACCESS;
    ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea.Trustee.ptstrName = (LPSTR) sid;
    if (SetEntriesInAclA(1, &ea, acl, &updated) == ERROR_SUCCESS &&
        SetNamedSecurityInfoA((LPSTR) path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, updated,
                              NULL) == ERROR_SUCCESS) {
        result = 1;
    }
    if (updated) LocalFree(updated);
    if (security) LocalFree(security);
    return result;
}
#endif

static void fiberMain(void *arg)
{
    WebHost *host;
    char dir[128], uploads[160];
#if ME_UNIX_LIKE
    char link[160];
#endif
#if ME_WIN_LIKE
    char state[160];
#endif
    struct stat info;

    host = makeHost(NULL);
    tnotnull(host);
    tmatch(host->uploadDir, "state/uploads");
    webFreeHost(host);

    mkdir(".test", 0700);
    sfmtbuf(dir, sizeof(dir), ".test/upload-dir-%d", getpid());
    expectResult(mkdir(dir, 0700), 0);
    rAddDirectory("state", dir);
    sfmtbuf(uploads, sizeof(uploads), "%s/uploads", dir);
    host = makeHost(NULL);
    tnotnull(host);
    tmatch(host->uploadDir, uploads);
    expectResult(webStartHost(host), 0);
    expectResult(stat(uploads, &info), 0);
#if ME_UNIX_LIKE
    teqi(info.st_mode & 0777, 0700);
#endif
    webFreeHost(host);
    rmdir(uploads);
    host = makeHost(dir);
    tnotnull(host);
    expectResult(webStartHost(host), 0);
    expectResult(stat(dir, &info), 0);
    webFreeHost(host);
#if ME_UNIX_LIKE
    expectResult(chmod(dir, 0777), 0);
    host = makeHost(dir);
    ttrue(webStartHost(host) < 0, "A world-writable upload directory must prevent startup");
    webFreeHost(host);
    expectResult(chmod(dir, 0700), 0);
    sfmtbuf(link, sizeof(link), "%s-link", dir);
    expectResult(symlink(rBasename(dir), link), 0);
    host = makeHost(link);
    ttrue(webStartHost(host) < 0, "A symlink upload directory must prevent startup");
    webFreeHost(host);
    unlink(link);
#endif
    rmdir(dir);
    host = makeHost(dir);
    ttrue(webStartHost(host) < 0, "A missing configured directory must prevent startup");
    webFreeHost(host);

#if ME_WIN_LIKE
    /*
        Use a writable parent to test that newly created upload directories remain private.
     */
    sfmtbuf(dir, sizeof(dir), ".test/upload-open-%d", getpid());
    expectResult(mkdir(dir, 0700), 0);
    ttrue(grantUsersWrite(dir), "Cannot grant Users write on the parent");
    ttrue(!onlyPrivateWriters(dir), "The parent must grant Users write");

    sfmtbuf(state, sizeof(state), "%s/state", dir);
    rAddDirectory("state", state);
    sfmtbuf(uploads, sizeof(uploads), "%s/uploads", state);
    host = makeHost(NULL);
    tnotnull(host);
    expectResult(webStartHost(host), 0);
    ttrue(onlyPrivateWriters(state), "The created state directory must be writable only by private principals");
    ttrue(onlyPrivateWriters(uploads), "The created upload directory must be writable only by private principals");
    webFreeHost(host);

    host = makeHost(dir);
    ttrue(webStartHost(host) < 0, "A configured directory granting Users write must prevent startup");
    webFreeHost(host);
    rmdir(uploads);
    rmdir(state);
    rmdir(dir);
#endif
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}
