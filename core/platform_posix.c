/*
 * platform_posix.c - macOS / Linux platform helpers for tscrt.
 */
#include "tscrt.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>

static char g_home_buf[MAX_PATH_LEN];
static char g_docs_buf[MAX_PATH_LEN];

static const char *posix_home(void)
{
    const char *h = getenv("HOME");
    if (h && *h) return h;
    struct passwd *pw = getpwuid(getuid());
    return pw ? pw->pw_dir : "/tmp";
}

const char *tscrt_get_documents_dir(void)
{
    if (g_docs_buf[0])
        return g_docs_buf;
    snprintf(g_docs_buf, sizeof(g_docs_buf), "%s/Documents", posix_home());
    return g_docs_buf;
}

const char *tscrt_get_home(void)
{
    if (g_home_buf[0])
        return g_home_buf;
#ifdef __APPLE__
    snprintf(g_home_buf, sizeof(g_home_buf),
             "%s/Library/Application Support", posix_home());
#else
    /* XDG_CONFIG_HOME or ~/.config on Linux */
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg)
        snprintf(g_home_buf, sizeof(g_home_buf), "%s", xdg);
    else
        snprintf(g_home_buf, sizeof(g_home_buf), "%s/.config", posix_home());
#endif
    return g_home_buf;
}

int tscrt_ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0)
        return S_ISDIR(st.st_mode) ? 0 : -1;
    return mkdir(path, 0700);
}

int tscrt_set_private_perms(const char *path)
{
    return chmod(path, 0600);
}
