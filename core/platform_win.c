/*
 * platform_win.c - Windows-specific platform helpers for tscrt_win.
 */
#include "tscrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <direct.h>
#include <windows.h>
#include <shlobj.h>
#include <aclapi.h>

static char g_home_buf[MAX_PATH_LEN];
static char g_docs_buf[MAX_PATH_LEN];

const char *tscrt_get_documents_dir(void)
{
    if (g_docs_buf[0])
        return g_docs_buf;

    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, path))) {
        snprintf(g_docs_buf, sizeof(g_docs_buf), "%s", path);
        return g_docs_buf;
    }

    const char *userprofile = getenv("USERPROFILE");
    if (userprofile && *userprofile)
        snprintf(g_docs_buf, sizeof(g_docs_buf), "%s\\Documents", userprofile);
    else
        snprintf(g_docs_buf, sizeof(g_docs_buf), "C:\\");
    return g_docs_buf;
}

const char *tscrt_get_home(void)
{
    if (g_home_buf[0])
        return g_home_buf;

    /* Prefer %APPDATA% (Roaming) — that's where per-user app config lives. */
    const char *appdata = getenv("APPDATA");
    if (appdata && *appdata) {
        snprintf(g_home_buf, sizeof(g_home_buf), "%s", appdata);
        return g_home_buf;
    }

    /* Fall back to SHGetFolderPath for older runtimes. */
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        snprintf(g_home_buf, sizeof(g_home_buf), "%s", path);
        return g_home_buf;
    }

    /* Last resort. */
    const char *userprofile = getenv("USERPROFILE");
    snprintf(g_home_buf, sizeof(g_home_buf), "%s",
             userprofile && *userprofile ? userprofile : "C:\\");
    return g_home_buf;
}

int tscrt_ensure_dir(const char *path)
{
    struct _stat st;
    if (_stat(path, &st) == 0)
        return S_ISDIR(st.st_mode) ? 0 : -1;
    return _mkdir(path);
}

int tscrt_set_private_perms(const char *path)
{
    /* Replace the file's DACL with a single entry granting the current
     * user full control, then mark the DACL as protected to block any
     * parent-directory inheritance that would re-broaden access.
     *
     * This hardens tscrt.profile against sibling-user reads/writes when
     * %APPDATA% lives on a network drive or inside a cloud-synced
     * folder (OneDrive, Dropbox) where default ACLs can be looser than
     * the usual per-user Roaming directory. */
    if (!path || !*path)
        return -1;

    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return -1;

    DWORD tokenInfoLen = 0;
    GetTokenInformation(token, TokenUser, NULL, 0, &tokenInfoLen);
    if (tokenInfoLen == 0) {
        CloseHandle(token);
        return -1;
    }

    TOKEN_USER *tokenUser = (TOKEN_USER *)malloc(tokenInfoLen);
    if (!tokenUser) {
        CloseHandle(token);
        return -1;
    }

    int rc = -1;
    PACL dacl = NULL;

    if (GetTokenInformation(token, TokenUser, tokenUser,
                            tokenInfoLen, &tokenInfoLen)) {
        EXPLICIT_ACCESSA ea;
        ZeroMemory(&ea, sizeof(ea));
        ea.grfAccessPermissions = GENERIC_ALL;
        ea.grfAccessMode        = SET_ACCESS;
        ea.grfInheritance       = NO_INHERITANCE;
        ea.Trustee.TrusteeForm  = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType  = TRUSTEE_IS_USER;
        ea.Trustee.ptstrName    = (LPSTR)tokenUser->User.Sid;

        if (SetEntriesInAclA(1, &ea, NULL, &dacl) == ERROR_SUCCESS) {
            DWORD r = SetNamedSecurityInfoA(
                (LPSTR)path, SE_FILE_OBJECT,
                DACL_SECURITY_INFORMATION |
                    PROTECTED_DACL_SECURITY_INFORMATION,
                NULL, NULL, dacl, NULL);
            if (r == ERROR_SUCCESS)
                rc = 0;
        }
    }

    if (dacl) LocalFree(dacl);
    free(tokenUser);
    CloseHandle(token);
    return rc;
}
