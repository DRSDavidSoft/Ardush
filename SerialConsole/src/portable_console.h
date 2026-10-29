#ifndef PS_CONSOLE_H
#define PS_CONSOLE_H
/* MIT Copyright (c) 2018 David Refoua; 2026 contributors.
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
#ifndef PS_CONSOLE_PROMPT_LABEL
#define PS_CONSOLE_PROMPT_LABEL "Console"
#endif
#ifndef PS_CONSOLE_TITLE
#define PS_CONSOLE_TITLE "Console"
#endif
typedef char portable_console_prompt_fits[
    sizeof(PS_CONSOLE_PROMPT_LABEL) <= PS_EDITOR_PROMPT_BYTES ? 1 : -1];
#ifndef PS_CONSOLE_RAW_ECHO
#define PS_CONSOLE_RAW_ECHO 1
#endif
#ifndef PS_CONSOLE_LEGACY_EXIT_BYTES
#define PS_CONSOLE_LEGACY_EXIT_BYTES 0
#endif
#include "portable-shell/portable_editor.h"
#include "portable-shell/portable_parse.h"
#include "portable-shell/portable_probe.h"
typedef struct portable_console portable_console;
typedef void (*portable_console_callback)(portable_console *, unsigned, char *const *);
typedef struct { const char *name; portable_console_callback run; } portable_console_command;
typedef struct {
    ps_terminal_write write;
    int (*led_get)(void *); /* 0/1, or -1 unavailable. */
    void (*led_set)(void *, int);
    void *context;
} portable_console_port;
enum { PS_CONSOLE_PROMPT, PS_CONSOLE_KEYS, PS_CONSOLE_PROBE, PS_CONSOLE_CLOSED, PS_CONSOLE_VIEW };
struct portable_console {
    ps_editor editor;
    ps_terminal_probe probe;
    portable_console_port port;
    const portable_console_command *commands;
    size_t command_count;
    uint32_t now;
    unsigned char mode, interactive, quiet_input, skip_lf, status;
};
static inline void portable_console_write(void *ctx, const unsigned char *p, size_t n) {
    portable_console *s = (portable_console *)ctx;
    if (!s->quiet_input) s->port.write(s->port.context, p, n);
}
static inline void portable_console_put(portable_console *s, const char *p) { ps_term_emit(portable_console_write, s, p); }
static inline void portable_console_number(portable_console *s, unsigned value) {
    char digits[11]; digits[ps_decimal(digits, value)] = 0; portable_console_put(s, digits);
}
static inline int portable_console_equal(const char *a, const char *b) {
    while (*a && ps_ascii_fold((unsigned char)*a) == ps_ascii_fold((unsigned char)*b)) { ++a; ++b; }
    return *a == 0 && *b == 0;
}
static inline void portable_console_prompt(portable_console *s) {
    if (s->mode == PS_CONSOLE_PROMPT) ps_editor_prompt(&s->editor, portable_console_write, s);
}
static inline void portable_console_close(portable_console *s) {
#if PS_CONSOLE_LEGACY_EXIT_BYTES
    static const unsigned char old_exit[] = {3, 26, 0};
    if (s->interactive) portable_console_write(s, old_exit, sizeof old_exit);
#endif
    ps_editor_reset(&s->editor); s->mode = PS_CONSOLE_CLOSED;
    portable_console_put(s, "logout\r\n");
}
static inline void portable_console_builtin(portable_console *, unsigned, char *const *);
static const char *const portable_console_names[] = {
    "clear", "upper", "noupper", "echo", "led", "test", "getcur", "exit", "keycodes",
    "help", "status", "ansi", "plain", "history-clear"
};
#define PS_CONSOLE_BUILTINS (sizeof portable_console_names / sizeof portable_console_names[0])
static inline const char *portable_console_candidate(void *ctx, size_t i) {
    portable_console *s = (portable_console *)ctx;
    return i < PS_CONSOLE_BUILTINS ? portable_console_names[i] : s->commands[i - PS_CONSOLE_BUILTINS].name;
}
static inline int portable_console_terminal_only(portable_console *s) {
    if (s->interactive) return 1;
    s->status = 3; portable_console_put(s, "interactive only\r\n"); return 0;
}
static inline void portable_console_builtin(portable_console *s, unsigned argc, char *const *argv) {
    const char *cmd = argv[0]; size_t i;
    if (portable_console_equal(cmd, "echo")) {
        for (i = 1; i < argc; ++i) { if (i > 1) portable_console_put(s, " "); portable_console_put(s, argv[i]); }
        portable_console_put(s, "\r\n");
    } else if (portable_console_equal(cmd, "upper")) s->editor.upper = 1;
    else if (portable_console_equal(cmd, "noupper")) s->editor.upper = 0;
    else if (portable_console_equal(cmd, "exit")) portable_console_close(s);
    else if (portable_console_equal(cmd, "ansi")) s->editor.ansi = s->interactive;
    else if (portable_console_equal(cmd, "plain")) s->editor.ansi = 0;
    else if (portable_console_equal(cmd, "history-clear")) ps_editor_clear_history(&s->editor);
    else if (portable_console_equal(cmd, "help")) {
        for (i = 0; i < PS_CONSOLE_BUILTINS + s->command_count; ++i) {
            portable_console_put(s, portable_console_candidate(s, i)); portable_console_put(s, "\r\n");
        }
    } else if (portable_console_equal(cmd, "status")) {
        int led = s->port.led_get ? s->port.led_get(s->port.context) : -1;
        portable_console_put(s, "led="); portable_console_put(s, led < 0 ? "unknown" : led ? "on" : "off");
        portable_console_put(s, " upper="); portable_console_put(s, s->editor.upper ? "on" : "off");
        portable_console_put(s, " ansi="); portable_console_put(s, s->editor.ansi ? "on\r\n" : "off\r\n");
    } else if (portable_console_equal(cmd, "led")) {
        int led = s->port.led_get ? s->port.led_get(s->port.context) : -1;
        if (led < 0 || !s->port.led_set) { s->status = 3; portable_console_put(s, "led unavailable\r\n"); return; }
        if (argc == 1) led = !led;
        else if (argc == 2 && portable_console_equal(argv[1], "on")) led = 1;
        else if (argc == 2 && portable_console_equal(argv[1], "off")) led = 0;
        else { s->status = 2; portable_console_put(s, "led [on|off]\r\n"); return; }
        s->port.led_set(s->port.context, led);
    } else if (portable_console_terminal_only(s)) {
        if (portable_console_equal(cmd, "clear")) portable_console_put(s, s->editor.ansi ? "\033[H\033[2J" : "\r\n");
        else if (portable_console_equal(cmd, "test")) portable_console_put(s, "\005");
        else if (portable_console_equal(cmd, "keycodes")) s->mode = PS_CONSOLE_KEYS;
        else if (portable_console_equal(cmd, "getcur")) {
            if (!s->editor.ansi) { s->status = 3; portable_console_put(s, "ansi disabled\r\n"); return; }
            s->mode = PS_CONSOLE_PROBE; ps_probe_begin(&s->probe, s->now, portable_console_write, s);
        }
    }
}
static inline void portable_console_init(portable_console *s, portable_console_port port, const portable_console_command *commands,
                              size_t count, int interactive) {
    ps_secure_clear(s, sizeof *s); s->port = port; s->commands = commands; s->command_count = count;
    s->interactive = interactive != 0; ps_editor_init(&s->editor);
    s->editor.echo = s->editor.ansi = s->interactive; s->editor.upper = 1;
    ps_bytes_copy(s->editor.prompt, PS_CONSOLE_PROMPT_LABEL, sizeof(PS_CONSOLE_PROMPT_LABEL)); portable_console_prompt(s);
}
static inline void portable_console_execute(portable_console *s) {
    char *argv[8]; ps_parse_result result; size_t i;
    s->status = 0;
#if PS_CONSOLE_RAW_ECHO
    /* Preserve v1.2's exact raw tail, including spacing and literal quotes. */
    char *line = s->editor.command; int raw_echo = 1;
    for (i = 0; i < s->command_count; ++i)
        if (portable_console_equal(s->commands[i].name, "echo")) raw_echo = 0;
    if (raw_echo && ps_text_prefix(line, "echo", 4) && (!line[4] || line[4] == ' ')) {
        portable_console_put(s, line[4] ? line + 5 : ""); portable_console_put(s, "\r\n"); return;
    }
#endif
    if (ps_parse_tokens(s->editor.command, sizeof s->editor.command, s->editor.command,
                        sizeof s->editor.command, argv, 8, &result)) {
        s->status = 2; portable_console_put(s, result.error); portable_console_put(s, "\r\n"); return;
    }
    if (!result.argc) return;
    /* Application callbacks may deliberately override built-ins. */
    for (i = 0; i < s->command_count; ++i) if (portable_console_equal(argv[0], s->commands[i].name)) {
        s->commands[i].run(s, result.argc, argv); return;
    }
    for (i = 0; i < PS_CONSOLE_BUILTINS; ++i) if (portable_console_equal(argv[0], portable_console_names[i])) {
        portable_console_builtin(s, result.argc, argv); return;
    }
    s->status = 127; portable_console_put(s, "command not found\r\n");
}
static inline void portable_console_editor_feed(portable_console *s, unsigned char ch) {
    int event;
    s->quiet_input = !s->interactive;
    event = ps_editor_feed(&s->editor, ch, portable_console_write, s); s->quiet_input = 0;
    if (event == PS_EDIT_SUBMIT) {
        ps_editor_remember(&s->editor, 1); portable_console_execute(s); ps_editor_clear(&s->editor); portable_console_prompt(s);
    } else if (event == PS_EDIT_TOO_LONG) {
        s->status = 2; portable_console_put(s, "line too long\r\n"); ps_editor_clear(&s->editor); portable_console_prompt(s);
    } else if (event == PS_EDIT_CANCEL) portable_console_prompt(s);
    else if (event == PS_EDIT_CLOSE) portable_console_close(s);
    else if (event == PS_EDIT_COMPLETE)
        ps_editor_complete(&s->editor, 0, PS_CONSOLE_BUILTINS + s->command_count, portable_console_candidate, s, portable_console_write, s);
}
static inline void portable_console_feed(portable_console *s, unsigned char ch) {
    int result; unsigned i;
    if (s->mode == PS_CONSOLE_CLOSED || s->mode == PS_CONSOLE_VIEW) return;
    if (s->skip_lf && (ch == '\n' || !ch)) { s->skip_lf = 0; return; }
    if (s->mode == PS_CONSOLE_KEYS) {
        /* Only consume the command's pending CRLF above. Diagnostic bytes
         * are raw: a subsequent CR must not suppress LF or NUL. */
        s->skip_lf = 0;
        if (ch == 3) { s->mode = PS_CONSOLE_PROMPT; portable_console_prompt(s); }
        else { portable_console_number(s, ch); portable_console_put(s, "\r\n"); }
        return;
    }
    s->skip_lf = ch == '\r';
    if (s->mode == PS_CONSOLE_PROBE) {
        result = ps_probe_feed(&s->probe, ch);
        if (result == PS_PROBE_WAIT) return;
        ps_probe_restore(portable_console_write, s); s->mode = PS_CONSOLE_PROMPT;
        if (result == PS_PROBE_READY) {
            portable_console_number(s, s->probe.row); portable_console_put(s, "x"); portable_console_number(s, s->probe.column); portable_console_put(s, "\r\n");
        }
        portable_console_prompt(s);
        /* A typed character or navigation sequence cancels the probe and is
         * replayed. Malformed/oversized DSR replies are never command input. */
        if (result == PS_PROBE_INVALID && s->probe.length < sizeof s->probe.bytes &&
            (s->probe.length < 3 || ch != 'R'))
            for (i = 0; i < s->probe.length; ++i) portable_console_editor_feed(s, s->probe.bytes[i]);
        return;
    }
    portable_console_editor_feed(s, ch);
}
static inline void portable_console_tick(portable_console *s, uint32_t now) {
    s->now = now;
    if (s->mode == PS_CONSOLE_PROBE && ps_probe_tick(&s->probe, now) == PS_PROBE_TIMEOUT) {
        ps_probe_restore(portable_console_write, s); s->mode = PS_CONSOLE_PROMPT;
        s->status = 3; portable_console_put(s, "probe timeout\r\n"); portable_console_prompt(s);
    }
}
#endif
