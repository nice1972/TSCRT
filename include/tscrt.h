/*
 * tscrt.h - TSCRT Common Definitions (Windows port)
 *
 * Cross-platform header. POSIX-only headers are guarded so the same
 * file can be included from MSYS2/UCRT64 (Windows) and Linux builds.
 */
#ifndef TSCRT_H
#define TSCRT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <io.h>
  #include <direct.h>
  /* MSVCRT stat helpers */
  #ifndef S_ISDIR
  #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
  #endif
  /* mkdir(path, mode) compatibility */
  #define tscrt_mkdir(p, m) _mkdir(p)
#else
  #include <unistd.h>
  #include <signal.h>
  #include <sys/ioctl.h>
  #include <pwd.h>
  #include <termios.h>
  #include <poll.h>
  #include <fcntl.h>
  #define tscrt_mkdir(p, m) mkdir((p), (m))
#endif

/* Suppress warn_unused_result for raw I/O writes (POSIX only) */
#define WRITE_TTY(fd, buf, len) \
    do { if (write((fd), (buf), (len)) < 0) { /* ignore */ } } while(0)

#define TSCRT_VERSION        "1.0.0-win"
#define TSCRT_DIR_NAME       "tscrt"          /* under %APPDATA% on Windows */
#define TSCRT_PROFILE_NAME   "tscrt.profile"
#define TSCRT_LOG_DIR_NAME   "logs"

#define MAX_SESSIONS         64
#define MAX_NAME_LEN         128
#define MAX_PATH_LEN         512
#define MAX_HOST_LEN         256
#define MAX_USER_LEN         128
/* Plaintext SSH passwords are short, but we also store them in DPAPI
 * encrypted form as "dpapi:<base64>". DPAPI ciphertext for even an 8-byte
 * plaintext is ~220 bytes (~300 base64 chars), so this buffer must hold
 * the encrypted form, not just the plaintext. 256 truncates the base64
 * mid-string and breaks decryption. */
#define MAX_PASS_LEN         1024
#define MAX_DEVICE_LEN       128
/* Big enough for the longest "password = dpapi:<base64>" line. */
#define MAX_LINE_LEN         2048
#define IO_BUF_SIZE          65536

#define ESCAPE_KEY           0x01    /* Ctrl-A */

#define MAX_BUTTONS          20
#define MAX_BTN_LABEL        20
#define MAX_BTN_ACTION       256
#define MAX_TRIGGERS         32
#define MAX_PERIODIC         16
#define MAX_STARTUP          32
#define MAX_PATTERN_LEN      256
#define TRIGGER_BUF_SIZE     1024

#define TSCRT_HELP_TEXT \
    "\r\n--- TSCRT Help ---\r\n" \
    "  GUI menus and buttons replace tmux key chords on Windows.\r\n" \
    "  See Help -> Shortcuts for the full list.\r\n" \
    "------------------\r\n"

typedef struct {
    char label[MAX_BTN_LABEL];
    char action[MAX_BTN_ACTION];
} button_t;

typedef enum {
    SESSION_TYPE_SSH    = 0,
    SESSION_TYPE_SERIAL = 1
} session_type_t;

typedef enum {
    PARITY_NONE = 0,
    PARITY_ODD,
    PARITY_EVEN
} parity_t;

typedef enum {
    FLOW_NONE = 0,
    FLOW_HARDWARE,
    FLOW_SOFTWARE
} flowcontrol_t;

typedef struct {
    char host[MAX_HOST_LEN];
    int  port;
    char username[MAX_USER_LEN];
    char password[MAX_PASS_LEN];
    char keyfile[MAX_PATH_LEN];
} ssh_config_t;

typedef struct {
    char           device[MAX_DEVICE_LEN];   /* "COM3" on Windows, "/dev/ttyUSB0" on Linux */
    int            baudrate;
    int            databits;
    int            stopbits;
    parity_t       parity;
    flowcontrol_t  flowcontrol;
} serial_config_t;

typedef struct {
    char            name[MAX_NAME_LEN];
    session_type_t  type;
    int             log_enabled;        /* 1 = write per-session log file */
    union {
        ssh_config_t    ssh;
        serial_config_t serial;
    };
} session_entry_t;

typedef struct {
    char session[MAX_NAME_LEN];
    char pattern[MAX_PATTERN_LEN];
    char action[MAX_BTN_ACTION];
} trigger_entry_t;

typedef struct {
    char session[MAX_NAME_LEN];
    char name[MAX_NAME_LEN];
    char action[MAX_BTN_ACTION];
    int  interval;
} periodic_entry_t;

typedef struct {
    char session[MAX_NAME_LEN];
    char name[MAX_NAME_LEN];
    char action[MAX_BTN_ACTION];
} startup_entry_t;

typedef struct {
    char log_dir[MAX_PATH_LEN];
    char terminal_type[64];
    char encoding[32];
    int  scrollback;
} common_config_t;

typedef struct {
    char            base_dir[MAX_PATH_LEN];
    char            profile_path[MAX_PATH_LEN];
    common_config_t common;
    session_entry_t sessions[MAX_SESSIONS];
    int             session_count;
    button_t        buttons[MAX_BUTTONS];
    trigger_entry_t triggers[MAX_TRIGGERS];
    int             trigger_count;
    periodic_entry_t periodics[MAX_PERIODIC];
    int             periodic_count;
    startup_entry_t startups[MAX_STARTUP];
    int             startup_count;
} profile_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Implemented in core/platform_win.c (or platform_posix.c). */
const char *tscrt_get_home(void);
const char *tscrt_get_documents_dir(void);               /* My Documents on Windows */
int         tscrt_ensure_dir(const char *path);
int         tscrt_set_private_perms(const char *path);   /* no-op on Windows */

#ifdef __cplusplus
}
#endif

#endif /* TSCRT_H */
