/*
 * tscrt.h - TSCRT Common Definitions
 *
 * Cross-platform header. Platform-specific headers are guarded so the
 * same file can be used on Windows (MSYS2/UCRT64), macOS, and Linux.
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
  #define TSCRT_PATH_SEP "\\"
#else
  #include <unistd.h>
  #include <signal.h>
  #include <sys/ioctl.h>
  #include <pwd.h>
  #include <termios.h>
  #include <poll.h>
  #include <fcntl.h>
  #define tscrt_mkdir(p, m) mkdir((p), (m))
  #define TSCRT_PATH_SEP "/"
#endif

/* Suppress warn_unused_result for raw I/O writes (POSIX only) */
#define WRITE_TTY(fd, buf, len) \
    do { if (write((fd), (buf), (len)) < 0) { /* ignore */ } } while(0)

#define TSCRT_VERSION        "1.0.6"
#define TSCRT_DIR_NAME       "tscrt"
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

#define MAX_TAB_LINKS        64

#define MAX_SNAPSHOTS        16
#define MAX_SNAPSHOT_CMDS    16
#define MAX_SNAPSHOT_CMD_LEN 256
#define MAX_SNAPSHOT_RECIP   8
#define MAX_SNAPSHOT_RULES   32
#define MAX_REGEX_LEN        128
#define MAX_EMAIL_LEN        192
#define MAX_CRON_LEN         64

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
    int             show_cmdline;       /* 1 = show command line in fullscreen */
    int             show_buttons;       /* 1 = show button bar in fullscreen */
    /* Auto-reconnect (SSH + Serial common) */
    int             auto_reconnect;     /* 0 = off, 1 = on */
    int             reconnect_max;      /* 0 = unlimited, default 10 */
    int             reconnect_base_ms;  /* base backoff, default 500ms */
    /* Terminal type advertised during PTY request (empty = use global default) */
    char            terminal_type[64];
    /* SSH keepalive (ignored for serial) */
    int             ssh_keepalive_sec;  /* libssh2 keepalive interval, 0 = off */
    int             ssh_tcp_keepalive;  /* SO_KEEPALIVE on TCP socket, 0/1 */
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
    char command[MAX_SNAPSHOT_CMD_LEN];   /* ActionParser syntax (\r, \p, ...) */
    int  delay_ms;                        /* minimum wait after sending */
    char expect_prompt[MAX_REGEX_LEN];    /* regex; empty = no early exit */
    int  max_wait_ms;                     /* upper bound for expect_prompt (0 = delay_ms only) */
} snapshot_cmd_t;

typedef struct {
    char            name[MAX_NAME_LEN];
    char            description[MAX_NAME_LEN];
    snapshot_cmd_t  cmds[MAX_SNAPSHOT_CMDS];
    int             cmd_count;
    char            recipients[MAX_SNAPSHOT_RECIP][MAX_EMAIL_LEN];
    int             recipient_count;
    char            subject_tmpl[MAX_NAME_LEN];
    int             send_email;
    int             attach_file;
} snapshot_entry_t;

typedef enum {
    SNAPSHOT_TRIGGER_ON_CONNECT = 0,
    SNAPSHOT_TRIGGER_CRON       = 1,
    SNAPSHOT_TRIGGER_PATTERN    = 2
} snapshot_trigger_kind_t;

typedef struct {
    int  kind;                        /* snapshot_trigger_kind_t */
    char session[MAX_NAME_LEN];       /* empty = all sessions */
    char snapshot[MAX_NAME_LEN];
    char cron_expr[MAX_CRON_LEN];     /* "m h dom mon dow" */
    char pattern[MAX_PATTERN_LEN];
    int  cooldown_sec;
} snapshot_rule_t;

/* Cross-instance tab link. A pair_id ties two tab "endpoints" together:
 * activating the tab on one TSCRT instance switches the matching tab on
 * any peer instance. Each endpoint is identified by (session_name,
 * slot_index) where slot_index disambiguates duplicate tabs of the same
 * session (0 = first tab opened for that session in the owning window,
 * 1 = second, ...). Links are bidirectional — either endpoint can be on
 * either instance; the scheme doesn't hard-code sides. */
#define MAX_PAIR_ID_LEN 40
/* left_role/right_role are single chars: 'A' for the primary process, 'B'
 * for the auto-spawned sibling. They disambiguate which process owns
 * which endpoint so a same-named session tab in both processes doesn't
 * cause spurious self-binding. Legacy links with role == 0 fall back to
 * ('A','B') at load time. */
typedef struct {
    char pair_id[MAX_PAIR_ID_LEN];      /* UUID without braces */
    char left_role;                     /* 'A' or 'B' */
    char left_session[MAX_NAME_LEN];
    int  left_slot;
    char right_role;                    /* 'A' or 'B' */
    char right_session[MAX_NAME_LEN];
    int  right_slot;
} tab_link_t;

typedef struct {
    char host[MAX_HOST_LEN];
    int  port;
    int  security;                    /* 0=none, 1=STARTTLS, 2=SMTPS */
    char username[MAX_USER_LEN];
    char password[MAX_PASS_LEN];      /* encryptSecret()-wrapped */
    char from_addr[MAX_EMAIL_LEN];
    char from_name[MAX_NAME_LEN];
    int  timeout_sec;
} smtp_config_t;

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
    snapshot_entry_t snapshots[MAX_SNAPSHOTS];
    int             snapshot_count;
    snapshot_rule_t snapshot_rules[MAX_SNAPSHOT_RULES];
    int             snapshot_rule_count;
    smtp_config_t   smtp;
    tab_link_t      tab_links[MAX_TAB_LINKS];
    int             tab_link_count;
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
