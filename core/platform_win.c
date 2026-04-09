/*
 * platform_win.c - Windows-specific platform helpers for tscrt_win.
 */
#include "tscrt.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <direct.h>
#include <windows.h>
#include <shlobj.h>

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
    /* Windows ACLs are out of scope for now; the user's APPDATA folder is
     * already per-user. Return success so callers behave the same. */
    (void)path;
    return 0;
}
