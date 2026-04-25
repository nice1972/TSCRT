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
    snprintf(p->base_dir, sizeof(p->base_dir),
             "%s" TSCRT_PATH_SEP "%s", home, TSCRT_DIR_NAME);
    snprintf(p->profile_path, sizeof(p->profile_path),
             "%s" TSCRT_PATH_SEP "%s", p->base_dir, TSCRT_PROFILE_NAME);

    if (tscrt_ensure_dir(p->base_dir) < 0) {
        fprintf(stderr, "Cannot create directory: %s\n", p->base_dir);
        return -1;
    }

    char pem_dir[MAX_PATH_LEN];
    snprintf(pem_dir, sizeof(pem_dir), "%s" TSCRT_PATH_SEP "pem", p->base_dir);
    tscrt_ensure_dir(pem_dir);

    /* Defaults — session logs land in "My Documents\TSCRT", not under
     * %APPDATA%. The directory is created lazily by SessionLogger when
     * the first log line is written. */
    const char *docs = tscrt_get_documents_dir();
    snprintf(p->common.log_dir, sizeof(p->common.log_dir),
             "%s" TSCRT_PATH_SEP "TSCRT", docs);
    snprintf(p->common.terminal_type, sizeof(p->common.terminal_type), "xterm-256color");
    snprintf(p->common.encoding, sizeof(p->common.encoding), "UTF-8");
    p->common.scrollback = 50000;

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
    s->log_enabled = 1;
    snprintf(s->name,         sizeof(s->name),         "demo (rebex.net)");
    snprintf(s->ssh.host,     sizeof(s->ssh.host),     "test.rebex.net");
    snprintf(s->ssh.username, sizeof(s->ssh.username), "demo");
    snprintf(s->ssh.password, sizeof(s->ssh.password), "password");
    s->ssh.port = 22;
}

static int profile_read_file(profile_t *p, const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[MAX_LINE_LEN];
    int section = 0;
    session_entry_t *cur = NULL;
    int btn_idx = 0;
    char auto_session[MAX_NAME_LEN] = {0};
    snapshot_entry_t *cur_snap = NULL;
    snapshot_rule_t  *cur_rule = NULL;

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
                    cur->log_enabled = 1;   /* default for old profiles */
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
            } else if (strcmp(s, "smtp") == 0) {
                section = 7;
            } else if (strncmp(s, "snapshot:", 9) == 0) {
                if (p->snapshot_count < MAX_SNAPSHOTS) {
                    cur_snap = &p->snapshots[p->snapshot_count++];
                    memset(cur_snap, 0, sizeof(*cur_snap));
                    snprintf(cur_snap->name, sizeof(cur_snap->name), "%s", s + 9);
                    snprintf(cur_snap->subject_tmpl, sizeof(cur_snap->subject_tmpl),
                             "TSCRT {session} {snapshot} {timestamp}");
                    cur_snap->send_email = 1;
                    cur_snap->attach_file = 1;
                    section = 8;
                } else {
                    section = 0;
                }
            } else if (strncmp(s, "snapshot_rule:", 14) == 0) {
                if (p->snapshot_rule_count < MAX_SNAPSHOT_RULES) {
                    cur_rule = &p->snapshot_rules[p->snapshot_rule_count++];
                    memset(cur_rule, 0, sizeof(*cur_rule));
                    section = 9;
                } else {
                    section = 0;
                }
            } else if (strncmp(s, "tab_link:", 9) == 0) {
                if (p->tab_link_count < MAX_TAB_LINKS) {
                    tab_link_t *lk = &p->tab_links[p->tab_link_count++];
                    memset(lk, 0, sizeof(*lk));
                    snprintf(lk->pair_id, sizeof(lk->pair_id), "%s", s + 9);
                    section = 10;
                } else {
                    section = 0;
                }
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
            else if (strcmp(key, "log_enabled") == 0) {
                cur->log_enabled = (atoi(val) != 0) ? 1 : 0;
            }
            else if (strcmp(key, "show_cmdline") == 0) {
                cur->show_cmdline = (atoi(val) != 0) ? 1 : 0;
            }
            else if (strcmp(key, "show_buttons") == 0) {
                cur->show_buttons = (atoi(val) != 0) ? 1 : 0;
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
            else if (strcmp(key, "terminal_type") == 0)
                snprintf(cur->terminal_type, sizeof(cur->terminal_type), "%s", val);
            else if (strcmp(key, "auto_reconnect") == 0)
                cur->auto_reconnect = (atoi(val) != 0) ? 1 : 0;
            else if (strcmp(key, "reconnect_max") == 0)
                cur->reconnect_max = atoi(val);
            else if (strcmp(key, "reconnect_base_ms") == 0)
                cur->reconnect_base_ms = atoi(val);
            else if (strcmp(key, "ssh_keepalive_sec") == 0)
                cur->ssh_keepalive_sec = atoi(val);
            else if (strcmp(key, "ssh_tcp_keepalive") == 0)
                cur->ssh_tcp_keepalive = (atoi(val) != 0) ? 1 : 0;
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
        } else if (section == 7) {
            /* [smtp] */
            if (strcmp(key, "host") == 0)
                snprintf(p->smtp.host, sizeof(p->smtp.host), "%s", val);
            else if (strcmp(key, "port") == 0)
                p->smtp.port = atoi(val);
            else if (strcmp(key, "security") == 0) {
                if (strcmp(val, "starttls") == 0) p->smtp.security = 1;
                else if (strcmp(val, "smtps") == 0) p->smtp.security = 2;
                else p->smtp.security = 0;
            }
            else if (strcmp(key, "username") == 0)
                snprintf(p->smtp.username, sizeof(p->smtp.username), "%s", val);
            else if (strcmp(key, "password") == 0)
                snprintf(p->smtp.password, sizeof(p->smtp.password), "%s", val);
            else if (strcmp(key, "from_addr") == 0)
                snprintf(p->smtp.from_addr, sizeof(p->smtp.from_addr), "%s", val);
            else if (strcmp(key, "from_name") == 0)
                snprintf(p->smtp.from_name, sizeof(p->smtp.from_name), "%s", val);
            else if (strcmp(key, "timeout_sec") == 0)
                p->smtp.timeout_sec = atoi(val);
        } else if (section == 8 && cur_snap) {
            /* [snapshot:<name>]
             *   description = ...
             *   subject_tmpl = ...
             *   send_email = 0/1
             *   attach_file = 0/1
             *   recipient = user@host   (repeated)
             *   cmd = <delay_ms>|<max_wait_ms>|<expect_prompt>|<command>
             */
            if (strcmp(key, "description") == 0)
                snprintf(cur_snap->description, sizeof(cur_snap->description), "%s", val);
            else if (strcmp(key, "subject_tmpl") == 0)
                snprintf(cur_snap->subject_tmpl, sizeof(cur_snap->subject_tmpl), "%s", val);
            else if (strcmp(key, "send_email") == 0)
                cur_snap->send_email = (atoi(val) != 0) ? 1 : 0;
            else if (strcmp(key, "attach_file") == 0)
                cur_snap->attach_file = (atoi(val) != 0) ? 1 : 0;
            else if (strcmp(key, "recipient") == 0) {
                if (cur_snap->recipient_count < MAX_SNAPSHOT_RECIP) {
                    snprintf(cur_snap->recipients[cur_snap->recipient_count],
                             MAX_EMAIL_LEN, "%s", val);
                    cur_snap->recipient_count++;
                }
            }
            else if (strcmp(key, "cmd") == 0) {
                if (cur_snap->cmd_count < MAX_SNAPSHOT_CMDS) {
                    snapshot_cmd_t *cmd = &cur_snap->cmds[cur_snap->cmd_count++];
                    /* Parse three '|' separators (delay|max_wait|expect|command).
                     * Command is the rest of the line so it may contain '|'. */
                    char *p1 = strchr(val, '|');
                    if (p1) {
                        *p1 = '\0';
                        cmd->delay_ms = atoi(val);
                        char *p2 = strchr(p1 + 1, '|');
                        if (p2) {
                            *p2 = '\0';
                            cmd->max_wait_ms = atoi(p1 + 1);
                            char *p3 = strchr(p2 + 1, '|');
                            if (p3) {
                                *p3 = '\0';
                                snprintf(cmd->expect_prompt,
                                         sizeof(cmd->expect_prompt), "%s", p2 + 1);
                                snprintf(cmd->command,
                                         sizeof(cmd->command), "%s", p3 + 1);
                            } else {
                                snprintf(cmd->expect_prompt,
                                         sizeof(cmd->expect_prompt), "%s", p2 + 1);
                            }
                        }
                    } else {
                        /* bare command only, no delay/regex */
                        snprintf(cmd->command, sizeof(cmd->command), "%s", val);
                    }
                }
            }
        } else if (section == 9 && cur_rule) {
            if (strcmp(key, "kind") == 0) {
                if (strcmp(val, "on_connect") == 0) cur_rule->kind = 0;
                else if (strcmp(val, "cron") == 0)  cur_rule->kind = 1;
                else if (strcmp(val, "pattern") == 0) cur_rule->kind = 2;
            }
            else if (strcmp(key, "session") == 0)
                snprintf(cur_rule->session, sizeof(cur_rule->session), "%s", val);
            else if (strcmp(key, "snapshot") == 0)
                snprintf(cur_rule->snapshot, sizeof(cur_rule->snapshot), "%s", val);
            else if (strcmp(key, "cron_expr") == 0)
                snprintf(cur_rule->cron_expr, sizeof(cur_rule->cron_expr), "%s", val);
            else if (strcmp(key, "pattern") == 0)
                snprintf(cur_rule->pattern, sizeof(cur_rule->pattern), "%s", val);
            else if (strcmp(key, "cooldown_sec") == 0)
                cur_rule->cooldown_sec = atoi(val);
        } else if (section == 10 && p->tab_link_count > 0) {
            /* [tab_link:<pair_id>]
             *   left_role     = A
             *   left_session  = ...
             *   left_slot     = 0
             *   right_role    = B
             *   right_session = ...
             *   right_slot    = 0
             */
            tab_link_t *lk = &p->tab_links[p->tab_link_count - 1];
            if (strcmp(key, "left_role") == 0)
                lk->left_role = val[0] ? val[0] : 'A';
            else if (strcmp(key, "left_session") == 0)
                snprintf(lk->left_session, sizeof(lk->left_session), "%s", val);
            else if (strcmp(key, "left_slot") == 0)
                lk->left_slot = atoi(val);
            else if (strcmp(key, "left_window_id") == 0)
                lk->left_window_id = atoi(val);
            else if (strcmp(key, "right_role") == 0)
                lk->right_role = val[0] ? val[0] : 'B';
            else if (strcmp(key, "right_session") == 0)
                snprintf(lk->right_session, sizeof(lk->right_session), "%s", val);
            else if (strcmp(key, "right_slot") == 0)
                lk->right_slot = atoi(val);
            else if (strcmp(key, "right_window_id") == 0)
                lk->right_window_id = atoi(val);
        }
    }

    fclose(fp);
    return 0;
}

int profile_load(profile_t *p)
{
    if (profile_read_file(p, p->profile_path) != 0) {
        /* First run: seed a sample session before writing defaults. */
        profile_seed_samples(p);
        return profile_save(p);
    }

    /* Migrate the legacy default log_dir (%APPDATA%\tscrt\logs) to the
     * new default under My Documents\TSCRT. Custom paths are left alone. */
    {
        char legacy[MAX_PATH_LEN];
        snprintf(legacy, sizeof(legacy), "%s" TSCRT_PATH_SEP "%s",
                 p->base_dir, TSCRT_LOG_DIR_NAME);
        if (p->common.log_dir[0] == '\0' ||
            strcmp(p->common.log_dir, legacy) == 0) {
            const char *docs = tscrt_get_documents_dir();
            snprintf(p->common.log_dir, sizeof(p->common.log_dir),
                     "%s" TSCRT_PATH_SEP "TSCRT", docs);
        }
    }

    return 0;
}

int profile_load_from(profile_t *p, const char *path)
{
    return profile_read_file(p, path);
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

static int profile_write_file(const profile_t *p, const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    tscrt_set_private_perms(path);

    fprintf(fp, "# TSCRT Configuration Profile\n");
    fprintf(fp, "# Generated by TSCRT v%s\n\n", TSCRT_VERSION);

    fprintf(fp, "[common]\n");
    fprintf(fp, "log_dir = %s\n", p->common.log_dir);
    fprintf(fp, "terminal_type = %s\n", p->common.terminal_type);
    fprintf(fp, "encoding = %s\n", p->common.encoding);
    fprintf(fp, "scrollback = %d\n\n", p->common.scrollback);

    for (int i = 0; i < p->session_count; i++) {
        const session_entry_t *s = &p->sessions[i];
        fprintf(fp, "[session:%s]\n", s->name);
        fprintf(fp, "log_enabled = %d\n", s->log_enabled ? 1 : 0);
        fprintf(fp, "show_cmdline = %d\n", s->show_cmdline ? 1 : 0);
        fprintf(fp, "show_buttons = %d\n", s->show_buttons ? 1 : 0);

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
        if (s->terminal_type[0])
            fprintf(fp, "terminal_type = %s\n", s->terminal_type);
        fprintf(fp, "auto_reconnect = %d\n",    s->auto_reconnect ? 1 : 0);
        fprintf(fp, "reconnect_max = %d\n",     s->reconnect_max);
        fprintf(fp, "reconnect_base_ms = %d\n", s->reconnect_base_ms);
        if (s->type == SESSION_TYPE_SSH) {
            fprintf(fp, "ssh_keepalive_sec = %d\n", s->ssh_keepalive_sec);
            fprintf(fp, "ssh_tcp_keepalive = %d\n", s->ssh_tcp_keepalive ? 1 : 0);
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

    /* SMTP */
    if (p->smtp.host[0] || p->smtp.from_addr[0]) {
        const char *sec = (p->smtp.security == 1) ? "starttls"
                        : (p->smtp.security == 2) ? "smtps" : "none";
        fprintf(fp, "[smtp]\n");
        fprintf(fp, "host = %s\n", p->smtp.host);
        fprintf(fp, "port = %d\n", p->smtp.port);
        fprintf(fp, "security = %s\n", sec);
        fprintf(fp, "username = %s\n", p->smtp.username);
        fprintf(fp, "password = %s\n", p->smtp.password);
        fprintf(fp, "from_addr = %s\n", p->smtp.from_addr);
        fprintf(fp, "from_name = %s\n", p->smtp.from_name);
        fprintf(fp, "timeout_sec = %d\n\n", p->smtp.timeout_sec);
    }

    /* Snapshots */
    for (int i = 0; i < p->snapshot_count; i++) {
        const snapshot_entry_t *sn = &p->snapshots[i];
        fprintf(fp, "[snapshot:%s]\n", sn->name);
        if (sn->description[0])
            fprintf(fp, "description = %s\n", sn->description);
        if (sn->subject_tmpl[0])
            fprintf(fp, "subject_tmpl = %s\n", sn->subject_tmpl);
        fprintf(fp, "send_email = %d\n", sn->send_email ? 1 : 0);
        fprintf(fp, "attach_file = %d\n", sn->attach_file ? 1 : 0);
        for (int r = 0; r < sn->recipient_count; r++)
            fprintf(fp, "recipient = %s\n", sn->recipients[r]);
        for (int c = 0; c < sn->cmd_count; c++) {
            const snapshot_cmd_t *cmd = &sn->cmds[c];
            fprintf(fp, "cmd = %d|%d|%s|%s\n",
                    cmd->delay_ms, cmd->max_wait_ms,
                    cmd->expect_prompt, cmd->command);
        }
        fprintf(fp, "\n");
    }

    /* Snapshot rules */
    for (int i = 0; i < p->snapshot_rule_count; i++) {
        const snapshot_rule_t *r = &p->snapshot_rules[i];
        const char *k = (r->kind == 1) ? "cron"
                      : (r->kind == 2) ? "pattern" : "on_connect";
        fprintf(fp, "[snapshot_rule:%d]\n", i);
        fprintf(fp, "kind = %s\n", k);
        fprintf(fp, "session = %s\n", r->session);
        fprintf(fp, "snapshot = %s\n", r->snapshot);
        if (r->cron_expr[0])
            fprintf(fp, "cron_expr = %s\n", r->cron_expr);
        if (r->pattern[0])
            fprintf(fp, "pattern = %s\n", r->pattern);
        fprintf(fp, "cooldown_sec = %d\n\n", r->cooldown_sec);
    }

    /* Tab links (cross-instance). One section per pair so we can keep
     * endpoint metadata (role + session name + slot_index) readable.
     * left_window_id / right_window_id are intentionally NOT serialized:
     * window IDs are assigned per-process at runtime and would be stale
     * on the next launch. The load-side parser still accepts those keys
     * for forward compatibility, but on restart links default to 0 (any
     * window) and same-process multi-window pairs must be re-established. */
    for (int i = 0; i < p->tab_link_count; i++) {
        const tab_link_t *lk = &p->tab_links[i];
        if (!lk->pair_id[0]) continue;
        const char lr = lk->left_role  ? lk->left_role  : 'A';
        const char rr = lk->right_role ? lk->right_role : 'B';
        fprintf(fp, "[tab_link:%s]\n", lk->pair_id);
        fprintf(fp, "left_role = %c\n",     lr);
        fprintf(fp, "left_session = %s\n",  lk->left_session);
        fprintf(fp, "left_slot = %d\n",     lk->left_slot);
        fprintf(fp, "right_role = %c\n",    rr);
        fprintf(fp, "right_session = %s\n", lk->right_session);
        fprintf(fp, "right_slot = %d\n\n",  lk->right_slot);
    }

    fclose(fp);
    return 0;
}

int profile_save(const profile_t *p)
{
    return profile_write_file(p, p->profile_path);
}

int profile_save_to(const profile_t *p, const char *path)
{
    return profile_write_file(p, path);
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
