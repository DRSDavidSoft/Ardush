#ifndef ARDUSH_H
#define ARDUSH_H
/* MIT Copyright (c) 2018 David Refoua; 2026 Ardush contributors.
 * Application commands over PortableShell's canonical byte components.
 * C99, fixed storage, no Arduino/OS headers, heap, or blocking reads. */
#ifndef PS_EDITOR_CAPACITY
#define PS_EDITOR_CAPACITY 64
#endif
#ifndef PS_EDITOR_HISTORY
#define PS_EDITOR_HISTORY 2
#endif
#ifndef PS_EDITOR_PROMPT_BYTES
#define PS_EDITOR_PROMPT_BYTES 12
#endif
#ifndef PS_EDITOR_SUGGEST_BYTES
#define PS_EDITOR_SUGGEST_BYTES 24
#endif
#ifndef PS_EDITOR_PROMPT_STYLE
#define PS_EDITOR_PROMPT_STYLE "\033[1;35m"
#endif
#if PS_EDITOR_PROMPT_BYTES < 7
#error "Ardush prompt requires at least seven bytes"
#endif
#ifndef ARDUSH_RAW_ECHO
#define ARDUSH_RAW_ECHO 1
#endif
#ifndef ARDUSH_LEGACY_EXIT_BYTES
#define ARDUSH_LEGACY_EXIT_BYTES 0
#endif
#include "portable-shell/portable_editor.h"
#include "portable-shell/portable_parse.h"
#include "portable-shell/portable_probe.h"
typedef struct ardush ardush;
typedef void (*ardush_callback)(ardush *, unsigned, char *const *);
typedef struct { const char *name; ardush_callback run; } ardush_command;
typedef struct {
    ps_terminal_write write;
    int (*led_get)(void *); /* 0/1, or -1 unavailable. */
    void (*led_set)(void *, int);
    void *context;
} ardush_port;
enum { ARDUSH_PROMPT, ARDUSH_KEYS, ARDUSH_PROBE, ARDUSH_CLOSED, ARDUSH_VIEW };
struct ardush {
    ps_editor editor;
    ps_terminal_probe probe;
    ardush_port port;
    const ardush_command *commands;
    size_t command_count;
    uint32_t now;
    unsigned char mode, interactive, quiet_input, skip_lf, status;
};
static inline void ardush_write(void *ctx, const unsigned char *p, size_t n) {
    ardush *s = (ardush *)ctx;
    if (!s->quiet_input) s->port.write(s->port.context, p, n);
}
static inline void ardush_put(ardush *s, const char *p) { ps_term_emit(ardush_write, s, p); }
static inline void ardush_number(ardush *s, unsigned value) {
    char digits[11]; digits[ps_decimal(digits, value)] = 0; ardush_put(s, digits);
}
static inline int ardush_equal(const char *a, const char *b) {
    while (*a && ps_ascii_fold((unsigned char)*a) == ps_ascii_fold((unsigned char)*b)) { ++a; ++b; }
    return *a == 0 && *b == 0;
}
static inline void ardush_prompt(ardush *s) {
    if (s->mode == ARDUSH_PROMPT) ps_editor_prompt(&s->editor, ardush_write, s);
}
static inline void ardush_close(ardush *s) {
#if ARDUSH_LEGACY_EXIT_BYTES
    static const unsigned char old_exit[] = {3, 26, 0};
    if (s->interactive) ardush_write(s, old_exit, sizeof old_exit);
#endif
    ps_editor_reset(&s->editor); s->mode = ARDUSH_CLOSED;
    ardush_put(s, "logout\r\n");
}
static inline void ardush_builtin(ardush *, unsigned, char *const *);
static const char *const ardush_names[] = {
    "clear", "upper", "noupper", "echo", "led", "test", "getcur", "exit", "keycodes",
    "help", "status", "ansi", "plain", "history-clear"
};
#define ARDUSH_BUILTINS (sizeof ardush_names / sizeof ardush_names[0])
static inline const char *ardush_candidate(void *ctx, size_t i) {
    ardush *s = (ardush *)ctx;
    return i < ARDUSH_BUILTINS ? ardush_names[i] : s->commands[i - ARDUSH_BUILTINS].name;
}
static inline int ardush_terminal_only(ardush *s) {
    if (s->interactive) return 1;
    s->status = 3; ardush_put(s, "interactive only\r\n"); return 0;
}
static inline void ardush_builtin(ardush *s, unsigned argc, char *const *argv) {
    const char *cmd = argv[0]; size_t i;
    if (ardush_equal(cmd, "echo")) {
        for (i = 1; i < argc; ++i) { if (i > 1) ardush_put(s, " "); ardush_put(s, argv[i]); }
        ardush_put(s, "\r\n");
    } else if (ardush_equal(cmd, "upper")) s->editor.upper = 1;
    else if (ardush_equal(cmd, "noupper")) s->editor.upper = 0;
    else if (ardush_equal(cmd, "exit")) ardush_close(s);
    else if (ardush_equal(cmd, "ansi")) s->editor.ansi = s->interactive;
    else if (ardush_equal(cmd, "plain")) s->editor.ansi = 0;
    else if (ardush_equal(cmd, "history-clear")) ps_editor_clear_history(&s->editor);
    else if (ardush_equal(cmd, "help")) {
        for (i = 0; i < ARDUSH_BUILTINS + s->command_count; ++i) {
            ardush_put(s, ardush_candidate(s, i)); ardush_put(s, "\r\n");
        }
    } else if (ardush_equal(cmd, "status")) {
        int led = s->port.led_get ? s->port.led_get(s->port.context) : -1;
        ardush_put(s, "led="); ardush_put(s, led < 0 ? "unknown" : led ? "on" : "off");
        ardush_put(s, " upper="); ardush_put(s, s->editor.upper ? "on" : "off");
        ardush_put(s, " ansi="); ardush_put(s, s->editor.ansi ? "on\r\n" : "off\r\n");
    } else if (ardush_equal(cmd, "led")) {
        int led = s->port.led_get ? s->port.led_get(s->port.context) : -1;
        if (led < 0 || !s->port.led_set) { s->status = 3; ardush_put(s, "led unavailable\r\n"); return; }
        if (argc == 1) led = !led;
        else if (argc == 2 && ardush_equal(argv[1], "on")) led = 1;
        else if (argc == 2 && ardush_equal(argv[1], "off")) led = 0;
        else { s->status = 2; ardush_put(s, "led [on|off]\r\n"); return; }
        s->port.led_set(s->port.context, led);
    } else if (ardush_terminal_only(s)) {
        if (ardush_equal(cmd, "clear")) ardush_put(s, s->editor.ansi ? "\033[H\033[2J" : "\r\n");
        else if (ardush_equal(cmd, "test")) ardush_put(s, "\005");
        else if (ardush_equal(cmd, "keycodes")) s->mode = ARDUSH_KEYS;
        else if (ardush_equal(cmd, "getcur")) {
            if (!s->editor.ansi) { s->status = 3; ardush_put(s, "ansi disabled\r\n"); return; }
            s->mode = ARDUSH_PROBE; ps_probe_begin(&s->probe, s->now, ardush_write, s);
        }
    }
}
static inline void ardush_init(ardush *s, ardush_port port, const ardush_command *commands,
                              size_t count, int interactive) {
    ps_secure_clear(s, sizeof *s); s->port = port; s->commands = commands; s->command_count = count;
    s->interactive = interactive != 0; ps_editor_init(&s->editor);
    s->editor.echo = s->editor.ansi = s->interactive; s->editor.upper = 1;
    ps_bytes_copy(s->editor.prompt, "Ardush", 7); ardush_prompt(s);
}
static inline void ardush_execute(ardush *s) {
    char *argv[8]; ps_parse_result result; size_t i;
    s->status = 0;
#if ARDUSH_RAW_ECHO
    /* Preserve v1.2's exact raw tail, including spacing and literal quotes. */
    char *line = s->editor.command; int raw_echo = 1;
    for (i = 0; i < s->command_count; ++i)
        if (ardush_equal(s->commands[i].name, "echo")) raw_echo = 0;
    if (raw_echo && ps_text_prefix(line, "echo", 4) && (!line[4] || line[4] == ' ')) {
        ardush_put(s, line[4] ? line + 5 : ""); ardush_put(s, "\r\n"); return;
    }
#endif
    if (ps_parse_tokens(s->editor.command, sizeof s->editor.command, s->editor.command,
                        sizeof s->editor.command, argv, 8, &result)) {
        s->status = 2; ardush_put(s, result.error); ardush_put(s, "\r\n"); return;
    }
    if (!result.argc) return;
    /* Application callbacks may deliberately override built-ins. */
    for (i = 0; i < s->command_count; ++i) if (ardush_equal(argv[0], s->commands[i].name)) {
        s->commands[i].run(s, result.argc, argv); return;
    }
    for (i = 0; i < ARDUSH_BUILTINS; ++i) if (ardush_equal(argv[0], ardush_names[i])) {
        ardush_builtin(s, result.argc, argv); return;
    }
    s->status = 127; ardush_put(s, "command not found\r\n");
}
static inline void ardush_editor_feed(ardush *s, unsigned char ch) {
    int event;
    s->quiet_input = !s->interactive;
    event = ps_editor_feed(&s->editor, ch, ardush_write, s); s->quiet_input = 0;
    if (event == PS_EDIT_SUBMIT) {
        ps_editor_remember(&s->editor, 1); ardush_execute(s); ps_editor_clear(&s->editor); ardush_prompt(s);
    } else if (event == PS_EDIT_TOO_LONG) {
        s->status = 2; ardush_put(s, "line too long\r\n"); ps_editor_clear(&s->editor); ardush_prompt(s);
    } else if (event == PS_EDIT_CANCEL) ardush_prompt(s);
    else if (event == PS_EDIT_CLOSE) ardush_close(s);
    else if (event == PS_EDIT_COMPLETE)
        ps_editor_complete(&s->editor, 0, ARDUSH_BUILTINS + s->command_count, ardush_candidate, s, ardush_write, s);
}
static inline void ardush_feed(ardush *s, unsigned char ch) {
    int result; unsigned i;
    if (s->mode == ARDUSH_CLOSED || s->mode == ARDUSH_VIEW) return;
    if (s->skip_lf && (ch == '\n' || !ch)) { s->skip_lf = 0; return; }
    s->skip_lf = ch == '\r';
    if (s->mode == ARDUSH_KEYS) {
        if (ch == 3) { s->mode = ARDUSH_PROMPT; ardush_prompt(s); }
        else { ardush_number(s, ch); ardush_put(s, "\r\n"); }
        return;
    }
    if (s->mode == ARDUSH_PROBE) {
        result = ps_probe_feed(&s->probe, ch);
        if (result == PS_PROBE_WAIT) return;
        ps_probe_restore(ardush_write, s); s->mode = ARDUSH_PROMPT;
        if (result == PS_PROBE_READY) {
            ardush_number(s, s->probe.row); ardush_put(s, "x"); ardush_number(s, s->probe.column); ardush_put(s, "\r\n");
        }
        ardush_prompt(s);
        /* A typed character or navigation sequence cancels the probe and is
         * replayed. Malformed/oversized DSR replies are never command input. */
        if (result == PS_PROBE_INVALID && s->probe.length < sizeof s->probe.bytes &&
            (s->probe.length < 3 || ch != 'R'))
            for (i = 0; i < s->probe.length; ++i) ardush_editor_feed(s, s->probe.bytes[i]);
        return;
    }
    ardush_editor_feed(s, ch);
}
static inline void ardush_tick(ardush *s, uint32_t now) {
    s->now = now;
    if (s->mode == ARDUSH_PROBE && ps_probe_tick(&s->probe, now) == PS_PROBE_TIMEOUT) {
        ps_probe_restore(ardush_write, s); s->mode = ARDUSH_PROMPT;
        s->status = 3; ardush_put(s, "probe timeout\r\n"); ardush_prompt(s);
    }
}
#endif
