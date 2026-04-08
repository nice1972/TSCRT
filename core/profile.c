/*
 * profile.c - INI-style profile loader/saver.
 *
 * Ported from the Linux tscrt project; the parser/serializer are
 * platform-agnostic and reused as-is. Only directory creation and
 * permission helpers come from platform_win.c (or platform_posix.c).
 */
#include "profile.h"
#include <ctype.h>

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) e--;
    e[1] = '\0';
    return s;
}

int profile_init(profile_t *p)
{
    memset(p, 0, sizeof(*p));

    const char *home = tscrt_get_home();
    snprintf(p->base_dir, sizeof(p->base_dir), "%s\\%s", home, TSCRT_DIR_NAME);
    snprintf(p->profile_path, sizeof(p->profile_path),
             "%s\\%s", p->base_dir, TSCRT_PROFILE_NAME);

    if (tscrt_ensure_dir(p->base_dir) < 0) {
        fprintf(stderr, "Cannot create directory: %s\n", p->base_dir);
        return -1;
    }

    char log_dir[MAX_PATH_LEN];
    snprintf(log_dir, sizeof(log_dir), "%s\\%s", p->base_dir, TSCRT_LOG_DIR_NAME);
    if (tscrt_ensure_dir(log_dir) < 0) {
        fprintf(stderr, "Cannot create directory: %s\n", log_dir);
        return -1;
    }

    char pem_dir[MAX_PATH_LEN];
    snprintf(pem_dir, sizeof(pem_dir), "%s\\pem", p->base_dir);
    tscrt_ensure_dir(pem_dir);

    /* Defaults */
    snprintf(p->common.log_dir, sizeof(p->common.log_dir), "%s", log_dir);
    snprintf(p->common.terminal_type, sizeof(p->common.terminal_type), "xterm-256color");
    snprintf(p->common.encoding, sizeof(p->common.encoding), "UTF-8");
    p->common.scrollback = 10000;

    static const char *btn_defaults[][2] = {
        /* Quick keys */
        {"CR",       "\\r"},
        {"LF",       "\\n"},
        {"CR+LF",    "\\r\\n"},
        {"ESC",      "\\e"},
        {"Tab",      "\\t"},
        /* Common shell commands - clicking sends them with CR */
        {"ls",       "ls -al\\r"},
        {"pwd",      "pwd\\r"},
        {"who",      "who\\r"},
        {"uname",    "uname -a\\r"},
        {"date",     "date\\r"},
        {"df",       "df -h\\r"},
        {"top",      "top -bn1 | head -20\\r"},
        {"clear",    "clear\\r"},
    };
    int ndefaults = (int)(sizeof(btn_defaults) / sizeof(btn_defaults[0]));
    for (int i = 0; i < MAX_BUTTONS; i++) {
        if (i < ndefaults) {
            snprintf(p->buttons[i].label,  MAX_BTN_LABEL,  "%s", btn_defaults[i][0]);
            snprintf(p->buttons[i].action, MAX_BTN_ACTION, "%s", btn_defaults[i][1]);
        } else {
            p->buttons[i].label[0] = '\0';
            p->buttons[i].action[0] = '\0';
        }
    }

    return 0;
}

/* Populate a fresh profile with one sample SSH session so the user has
 * something to click after first launch. The host points at a public
 * read-only SSH demo server (Rebex.NET) - replace via Preferences. */
static void profile_seed_samples(profile_t *p)
{
    if (p->session_count > 0) return;
    session_entry_t *s = &p->sessions[p->session_count++];
    memset(s, 0, sizeof(*s));
    s->type = SESSION_TYPE_SSH;
    snprintf(s->name,         sizeof(s->name),         "demo (rebex.net)");
    snprintf(s->ssh.host,     sizeof(s->ssh.host),     "test.rebex.net");
    snprintf(s->ssh.username, sizeof(s->ssh.username), "demo");
    snprintf(s->ssh.password, sizeof(s->ssh.password), "password");
    s->ssh.port = 22;
}

int profile_load(profile_t *p)
{
    FILE *fp = fopen(p->profile_path, "r");
    if (!fp) {
        /* First run: seed a sample session before writing defaults. */
        profile_seed_samples(p);
        return profile_save(p);
    }

    char line[MAX_LINE_LEN];
    int section = 0;
    session_entry_t *cur = NULL;
    int btn_idx = 0;
    char auto_session[MAX_NAME_LEN] = {0};

    while (fgets(line, sizeof(line), fp)) {
        char *s = trim(line);
        if (*s == '#' || *s == ';' || *s == '\0')
            continue;

        if (*s == '[') {
            char *end = strchr(s, ']');
            if (!end) continue;
            *end = '\0';
            s++;

            if (strncmp(s, "session:", 8) == 0) {
                if (p->session_count < MAX_SESSIONS) {
                    cur = &p->sessions[p->session_count++];
                    memset(cur, 0, sizeof(*cur));
                    snprintf(cur->name, sizeof(cur->name), "%s", s + 8);
                    section = 2;
                }
            } else if (strcmp(s, "common") == 0) {
                section = 1;
            } else if (strcmp(s, "buttons") == 0) {
                section = 3;
                btn_idx = 0;
            } else if (strncmp(s, "triggers:", 9) == 0) {
                snprintf(auto_session, sizeof(auto_session), "%s", s + 9);
                section = 4;
            } else if (strncmp(s, "periodic:", 9) == 0) {
                snprintf(auto_session, sizeof(auto_session), "%s", s + 9);
                section = 5;
            } else if (strncmp(s, "startup:", 8) == 0) {
                snprintf(auto_session, sizeof(auto_session), "%s", s + 8);
                section = 6;
            }
            continue;
        }

        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim(s);
        char *val = trim(eq + 1);

        if (section == 1) {
            if (strcmp(key, "log_dir") == 0)
                snprintf(p->common.log_dir, sizeof(p->common.log_dir), "%s", val);
            else if (strcmp(key, "terminal_type") == 0)
                snprintf(p->common.terminal_type, sizeof(p->common.terminal_type), "%s", val);
            else if (strcmp(key, "encoding") == 0)
                snprintf(p->common.encoding, sizeof(p->common.encoding), "%s", val);
            else if (strcmp(key, "scrollback") == 0)
                p->common.scrollback = atoi(val);
        } else if (section == 2 && cur) {
            if (strcmp(key, "type") == 0) {
                cur->type = (strcmp(val, "serial") == 0)
                    ? SESSION_TYPE_SERIAL : SESSION_TYPE_SSH;
            }
            else if (strcmp(key, "host") == 0)
                snprintf(cur->ssh.host, sizeof(cur->ssh.host), "%s", val);
            else if (strcmp(key, "port") == 0)
                cur->ssh.port = atoi(val);
            else if (strcmp(key, "username") == 0)
                snprintf(cur->ssh.username, sizeof(cur->ssh.username), "%s", val);
            else if (strcmp(key, "password") == 0)
                snprintf(cur->ssh.password, sizeof(cur->ssh.password), "%s", val);
            else if (strcmp(key, "keyfile") == 0)
                snprintf(cur->ssh.keyfile, sizeof(cur->ssh.keyfile), "%s", val);
            else if (strcmp(key, "device") == 0)
                snprintf(cur->serial.device, sizeof(cur->serial.device), "%s", val);
            else if (strcmp(key, "baudrate") == 0)
                cur->serial.baudrate = atoi(val);
            else if (strcmp(key, "databits") == 0)
                cur->serial.databits = atoi(val);
            else if (strcmp(key, "stopbits") == 0)
                cur->serial.stopbits = atoi(val);
            else if (strcmp(key, "parity") == 0) {
                if (strcmp(val, "odd") == 0) cur->serial.parity = PARITY_ODD;
                else if (strcmp(val, "even") == 0) cur->serial.parity = PARITY_EVEN;
                else cur->serial.parity = PARITY_NONE;
            }
            else if (strcmp(key, "flowcontrol") == 0) {
                if (strcmp(val, "hardware") == 0) cur->serial.flowcontrol = FLOW_HARDWARE;
                else if (strcmp(val, "software") == 0) cur->serial.flowcontrol = FLOW_SOFTWARE;
                else cur->serial.flowcontrol = FLOW_NONE;
            }
        } else if (section == 3) {
            if (btn_idx < MAX_BUTTONS) {
                snprintf(p->buttons[btn_idx].label,  MAX_BTN_LABEL,  "%s", key);
                snprintf(p->buttons[btn_idx].action, MAX_BTN_ACTION, "%s", val);
                btn_idx++;
            }
        } else if (section == 4) {
            if (key[0] && p->trigger_count < MAX_TRIGGERS) {
                trigger_entry_t *t = &p->triggers[p->trigger_count++];
                snprintf(t->session, sizeof(t->session), "%s", auto_session);
                snprintf(t->pattern, sizeof(t->pattern), "%s", key);
                snprintf(t->action,  sizeof(t->action),  "%s", val);
            }
        } else if (section == 5) {
            if (key[0] && p->periodic_count < MAX_PERIODIC) {
                periodic_entry_t *pe = &p->periodics[p->periodic_count];
                snprintf(pe->session, sizeof(pe->session), "%s", auto_session);
                char *colon = strchr(key, ':');
                if (colon) {
                    *colon = '\0';
                    pe->interval = atoi(key);
                    snprintf(pe->name, sizeof(pe->name), "%s", colon + 1);
                } else {
                    pe->interval = atoi(key);
                    snprintf(pe->name, sizeof(pe->name), "task%d",
                             p->periodic_count);
                }
                snprintf(pe->action, sizeof(pe->action), "%s", val);
                if (pe->interval > 0)
                    p->periodic_count++;
            }
        } else if (section == 6) {
            if (key[0] && p->startup_count < MAX_STARTUP) {
                startup_entry_t *su = &p->startups[p->startup_count++];
                snprintf(su->session, sizeof(su->session), "%s", auto_session);
                snprintf(su->name, sizeof(su->name), "%s", key);
                snprintf(su->action, sizeof(su->action), "%s", val);
            }
        }
    }

    fclose(fp);
    return 0;
}

static const char *parity_str(parity_t p)
{
    switch (p) {
    case PARITY_ODD:  return "odd";
    case PARITY_EVEN: return "even";
    default:          return "none";
    }
}

static const char *flow_str(flowcontrol_t f)
{
    switch (f) {
    case FLOW_HARDWARE: return "hardware";
    case FLOW_SOFTWARE: return "software";
    default:            return "none";
    }
}

int profile_save(const profile_t *p)
{
    FILE *fp = fopen(p->profile_path, "w");
    if (!fp) return -1;

    tscrt_set_private_perms(p->profile_path);

    fprintf(fp, "# TSCRT Configuration Profile\n");
    fprintf(fp, "# Generated by tscrt v%s\n\n", TSCRT_VERSION);

    fprintf(fp, "[common]\n");
    fprintf(fp, "log_dir = %s\n", p->common.log_dir);
    fprintf(fp, "terminal_type = %s\n", p->common.terminal_type);
    fprintf(fp, "encoding = %s\n", p->common.encoding);
    fprintf(fp, "scrollback = %d\n\n", p->common.scrollback);

    for (int i = 0; i < p->session_count; i++) {
        const session_entry_t *s = &p->sessions[i];
        fprintf(fp, "[session:%s]\n", s->name);

        if (s->type == SESSION_TYPE_SSH) {
            fprintf(fp, "type = ssh\n");
            fprintf(fp, "host = %s\n", s->ssh.host);
            fprintf(fp, "port = %d\n", s->ssh.port);
            fprintf(fp, "username = %s\n", s->ssh.username);
            fprintf(fp, "password = %s\n", s->ssh.password);
            if (s->ssh.keyfile[0])
                fprintf(fp, "keyfile = %s\n", s->ssh.keyfile);
        } else {
            fprintf(fp, "type = serial\n");
            fprintf(fp, "device = %s\n", s->serial.device);
            fprintf(fp, "baudrate = %d\n", s->serial.baudrate);
            fprintf(fp, "databits = %d\n", s->serial.databits);
            fprintf(fp, "stopbits = %d\n", s->serial.stopbits);
            fprintf(fp, "parity = %s\n", parity_str(s->serial.parity));
            fprintf(fp, "flowcontrol = %s\n", flow_str(s->serial.flowcontrol));
        }
        fprintf(fp, "\n");
    }

    fprintf(fp, "[buttons]\n");
    for (int i = 0; i < MAX_BUTTONS; i++) {
        if (p->buttons[i].label[0])
            fprintf(fp, "%s = %s\n", p->buttons[i].label, p->buttons[i].action);
        else
            fprintf(fp, "=\n");
    }
    fprintf(fp, "\n");

    /* Collect unique session names from triggers/periodic/startup */
    char seen[MAX_SESSIONS][MAX_NAME_LEN];
    int seen_count = 0;

    #define ADD_SEEN(nm) do { \
        int _found = 0; \
        for (int _k = 0; _k < seen_count; _k++) \
            if (strcmp(seen[_k], (nm)) == 0) { _found = 1; break; } \
        if (!_found && seen_count < MAX_SESSIONS) \
            snprintf(seen[seen_count++], MAX_NAME_LEN, "%s", (nm)); \
    } while(0)

    for (int i = 0; i < p->trigger_count; i++)
        ADD_SEEN(p->triggers[i].session);
    for (int i = 0; i < p->periodic_count; i++)
        ADD_SEEN(p->periodics[i].session);
    for (int i = 0; i < p->startup_count; i++)
        ADD_SEEN(p->startups[i].session);
    for (int i = 0; i < p->session_count; i++)
        ADD_SEEN(p->sessions[i].name);
    #undef ADD_SEEN

    for (int si = 0; si < seen_count; si++) {
        const char *sn = seen[si];

        fprintf(fp, "[startup:%s]\n", sn);
        for (int i = 0; i < p->startup_count; i++)
            if (strcmp(p->startups[i].session, sn) == 0)
                fprintf(fp, "%s = %s\n", p->startups[i].name,
                        p->startups[i].action);
        fprintf(fp, "\n");

        fprintf(fp, "[triggers:%s]\n", sn);
        for (int i = 0; i < p->trigger_count; i++)
            if (strcmp(p->triggers[i].session, sn) == 0)
                fprintf(fp, "%s = %s\n", p->triggers[i].pattern,
                        p->triggers[i].action);
        fprintf(fp, "\n");

        fprintf(fp, "[periodic:%s]\n", sn);
        for (int i = 0; i < p->periodic_count; i++)
            if (strcmp(p->periodics[i].session, sn) == 0)
                fprintf(fp, "%d:%s = %s\n", p->periodics[i].interval,
                        p->periodics[i].name, p->periodics[i].action);
        fprintf(fp, "\n");
    }

    fclose(fp);
    return 0;
}

int profile_add_session(profile_t *p, const session_entry_t *s)
{
    if (p->session_count >= MAX_SESSIONS) return -1;
    memcpy(&p->sessions[p->session_count++], s, sizeof(session_entry_t));
    return profile_save(p);
}

int profile_delete_session(profile_t *p, int index)
{
    if (index < 0 || index >= p->session_count) return -1;
    if (index < p->session_count - 1) {
        memmove(&p->sessions[index], &p->sessions[index + 1],
                (p->session_count - index - 1) * sizeof(session_entry_t));
    }
    p->session_count--;
    return profile_save(p);
}
