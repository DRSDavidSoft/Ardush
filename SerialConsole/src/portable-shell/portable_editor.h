#ifndef PORTABLE_EDITOR_H
#define PORTABLE_EDITOR_H
#include "portable_terminal.h"
#include "portable_style.h"
/* Bounded event editor profile; source lineage is in the donor inventory.
 * No command execution, allocation or application state. Hidden by default.
 * Compile separately from the full session when a firmware already owns its
 * dispatcher and TUI. Capacity macros must agree in all consuming units. */
#ifndef PS_EDITOR_CAPACITY
#define PS_EDITOR_CAPACITY 512
#endif
#ifndef PS_EDITOR_HISTORY
#define PS_EDITOR_HISTORY 8
#endif
#ifndef PS_EDITOR_PROMPT_BYTES
#define PS_EDITOR_PROMPT_BYTES 49
#endif
#ifndef PS_EDITOR_SUGGEST_BYTES
#define PS_EDITOR_SUGGEST_BYTES 80
#endif
#ifndef PS_EDITOR_PROMPT_STYLE
#define PS_EDITOR_PROMPT_STYLE "\x1b[1;35m"
#endif
#if PS_EDITOR_PROMPT_BYTES < 6 || PS_EDITOR_SUGGEST_BYTES < 2
#error "Prompt needs >=6 bytes and suggestion storage >=2"
#endif
#if PS_EDITOR_CAPACITY < 32 || PS_EDITOR_HISTORY < 1
#error "Event editor requires capacity >=32 and history >=1"
#endif
enum {
    PS_EDIT_NONE,
    PS_EDIT_SUBMIT,
    PS_EDIT_CANCEL,
    PS_EDIT_TOO_LONG,
    PS_EDIT_CLOSE,
    PS_EDIT_COMPLETE
};
typedef const char *(*ps_editor_candidate)(void *, size_t);
typedef struct {
    char command[PS_EDITOR_CAPACITY + 1], history[PS_EDITOR_HISTORY][PS_EDITOR_CAPACITY + 1];
    char draft[PS_EDITOR_CAPACITY + 1], prompt[PS_EDITOR_PROMPT_BYTES], suggestion[PS_EDITOR_SUGGEST_BYTES];
    size_t next, count, offset, draft_cursor, cursor;
    unsigned char escape, attribute, escape_bytes, echo, cr_pending, overflow, ansi, upper;
    unsigned char theme, true_color;
} ps_editor;
static inline void ps_term_emit(ps_terminal_write write, void *ctx, const char *text) {
    write(ctx, (const unsigned char *)text, ps_text_length(text));
}
static inline void ps_term_left(ps_terminal_write write, void *ctx, size_t n, int ansi) {
    char b[16];
    size_t length;
    if (!n || n > PS_EDITOR_CAPACITY)
        return;
    if (!ansi) {
        while (n--)
            ps_term_emit(write, ctx, "\b");
        return;
    }
    b[0] = 27;
    b[1] = '[';
    length = ps_decimal(b + 2, (unsigned)n);
    b[length + 2] = 'D';
    b[length + 3] = 0;
    ps_term_emit(write, ctx, b);
}
static inline void ps_editor_init(ps_editor *s) {
    ps_secure_clear(s, sizeof(*s));
    s->ansi = 1;
    s->theme = 255;
    s->true_color = 1;
    ps_bytes_copy(s->prompt, "shell", 6);
}
static inline void ps_editor_clear(ps_editor *s) {
    ps_secure_clear(s->command, sizeof(s->command));
    ps_secure_clear(s->draft, sizeof(s->draft));
    ps_secure_clear(s->suggestion, sizeof(s->suggestion));
    s->cursor = s->draft_cursor = s->offset = 0;
    s->escape = s->attribute = s->escape_bytes = s->overflow = 0;
}
static inline void ps_editor_clear_history(ps_editor *s) {
    ps_secure_clear(s->history, sizeof(s->history));
    ps_secure_clear(s->draft, sizeof(s->draft));
    s->next = s->count = s->offset = s->draft_cursor = 0;
}
static inline void ps_editor_reset(ps_editor *s) {
    ps_editor_clear(s);
    ps_editor_clear_history(s);
    s->cr_pending = s->echo = s->upper = 0;
}
static inline void ps_editor_prompt(ps_editor *s, ps_terminal_write write, void *ctx) {
    if (!s->echo)
        return;
    if (s->ansi)
        ps_term_emit(write, ctx, s->theme <= 3 ? ps_style_ink(s->theme, s->true_color, 0, 1) : PS_EDITOR_PROMPT_STYLE);
    ps_term_emit(write, ctx, s->prompt);
    ps_term_emit(write, ctx, "> ");
    if (s->ansi)
        ps_term_emit(write, ctx, "\x1b[0m");
}
static inline void ps_editor_emit_input(ps_editor *s, ps_terminal_write write, void *ctx,
                                        const char *text) {
    if (!s->upper) {
        ps_term_emit(write, ctx, text);
        return;
    }
    while (*text) {
        unsigned char c = (unsigned char)*text++;
        if (c >= 'a' && c <= 'z')
            c -= 32;
        write(ctx, &c, 1);
    }
}
static inline void ps_editor_bell(ps_editor *s, ps_terminal_write write, void *ctx) {
    if (s->echo)
        ps_term_emit(write, ctx, "\a");
}
static inline void ps_editor_left(ps_editor *s, ps_terminal_write write, void *ctx, size_t n) {
    if (s->echo)
        ps_term_left(write, ctx, n, s->ansi);
}
static inline void ps_editor_redraw(ps_editor *s, ps_terminal_write write, void *ctx) {
    if (!s->echo)
        return;
    ps_term_emit(write, ctx, s->ansi ? "\r\x1b[2K" : "\r");
    ps_editor_prompt(s, write, ctx);
    ps_editor_emit_input(s, write, ctx, s->command);
    if (s->ansi && s->suggestion[0]) {
        ps_term_emit(write, ctx, "\x1b[90m");
        ps_editor_emit_input(s, write, ctx, s->suggestion);
        ps_term_emit(write, ctx, "\x1b[0m");
        ps_editor_left(s, write, ctx, ps_text_length(s->suggestion));
    }
    ps_editor_left(s, write, ctx, ps_text_length(s->command) - s->cursor);
}
static inline void ps_editor_erase_suggestion(ps_editor *s, ps_terminal_write write, void *ctx) {
    if (!s->suggestion[0])
        return;
    if (s->echo && s->ansi)
        ps_term_emit(write, ctx, "\x1b[K");
    ps_secure_clear(s->suggestion, sizeof(s->suggestion));
}
static inline int ps_editor_remember(ps_editor *s, int allowed) {
    size_t latest = (s->next + PS_EDITOR_HISTORY - 1) % PS_EDITOR_HISTORY;
    if (!allowed || !s->echo || s->overflow || !ps_history_safe(s->command, sizeof(s->command)))
        return 0;
    if (s->count && ps_text_equal(s->history[latest], s->command))
        return 0;
    ps_secure_clear(s->history[s->next], sizeof(s->history[s->next]));
    ps_bytes_copy(s->history[s->next], s->command, ps_text_length(s->command) + 1);
    s->next = (s->next + 1) % PS_EDITOR_HISTORY;
    if (s->count < PS_EDITOR_HISTORY)
        ++s->count;
    s->offset = 0;
    return 1;
}
static inline void ps_editor_history(ps_editor *s, int older, ps_terminal_write write, void *ctx) {
    if (!s->echo || !s->count) {
        ps_editor_bell(s, write, ctx);
        return;
    }
    if (older) {
        if (s->offset == s->count) {
            ps_editor_bell(s, write, ctx);
            return;
        }
        if (!s->offset) {
            ps_secure_clear(s->draft, sizeof(s->draft));
            ps_bytes_copy(s->draft, s->command, ps_text_length(s->command) + 1);
            s->draft_cursor = s->cursor;
        }
        ++s->offset;
    } else {
        if (!s->offset) {
            ps_editor_bell(s, write, ctx);
            return;
        }
        --s->offset;
    }
    if (s->offset) {
        ps_bytes_copy(s->command,
                      s->history[(s->next + PS_EDITOR_HISTORY - s->offset) % PS_EDITOR_HISTORY],
                      sizeof(s->command));
        s->cursor = ps_text_length(s->command);
    } else {
        ps_bytes_copy(s->command, s->draft, sizeof(s->command));
        s->cursor = s->draft_cursor;
    }
    ps_editor_redraw(s, write, ctx);
}
static inline void ps_editor_shift_left(ps_editor *s) {
    size_t i;
    for (i = s->cursor; i < PS_EDITOR_CAPACITY; ++i)
        s->command[i] = s->command[i + 1];
    s->command[PS_EDITOR_CAPACITY] = 0;
}
static inline void ps_editor_fix_cursor(ps_editor *s, int clear, ps_terminal_write write,
                                        void *ctx) {
    size_t length = ps_text_length(s->command);
    if (s->cursor != length) {
        if (s->ansi) {
            ps_term_emit(write, ctx, "\x1b[s");
            ps_editor_emit_input(s, write, ctx, s->command + s->cursor);
            ps_term_emit(write, ctx, "\x1b[K\x1b[u");
        } else {
            ps_editor_emit_input(s, write, ctx, s->command + s->cursor);
            if (clear)
                ps_term_emit(write, ctx, " ");
            ps_editor_left(s, write, ctx, length - s->cursor + (clear ? 1 : 0));
        }
    } else if (clear)
        ps_term_emit(write, ctx, s->ansi ? "\x1b[K" : " \b");
}
static inline void ps_editor_move(ps_editor *s, unsigned direction, ps_terminal_write write,
                                  void *ctx) {
    size_t length = ps_text_length(s->command), size, target;
    if ((direction == 'C' || direction == 'F') && s->cursor == length && s->suggestion[0]) {
        size = ps_text_length(s->suggestion);
        ps_bytes_copy(s->command + s->cursor, s->suggestion, size);
        s->cursor += size;
        s->command[s->cursor++] = ' ';
        s->command[s->cursor] = 0;
        ps_editor_emit_input(s, write, ctx, s->suggestion);
        ps_term_emit(write, ctx, " ");
        ps_secure_clear(s->suggestion, sizeof(s->suggestion));
        s->offset = 0;
        return;
    }
    ps_editor_erase_suggestion(s, write, ctx);
    if (direction == 'D') {
        if (!s->cursor) {
            ps_editor_bell(s, write, ctx);
            return;
        }
        --s->cursor;
        ps_editor_left(s, write, ctx, 1);
    } else if (direction == 'C') {
        if (s->cursor == length) {
            ps_editor_bell(s, write, ctx);
            return;
        }
        if (s->echo) {
            if (s->ansi)
                ps_term_emit(write, ctx, "\x1b[C");
            else {
                char c[2] = {s->command[s->cursor], 0};
                ps_editor_emit_input(s, write, ctx, c);
            }
        }
        ++s->cursor;
    } else if (direction == 'H' || direction == 'F') {
        target = direction == 'H' ? 0 : length;
        while (s->cursor != target)
            ps_editor_move(s, s->cursor > target ? 'D' : 'C', write, ctx);
    } else if (direction == 'A' || direction == 'B')
        ps_editor_history(s, direction == 'A', write, ctx);
    else
        ps_editor_bell(s, write, ctx);
}
static inline void ps_editor_escape(ps_editor *s, unsigned ch, ps_terminal_write write, void *ctx) {
    if (s->escape == 1) {
        if (ch == '[' || ch == 'O') {
            s->escape = 2;
            return;
        }
        s->escape = 0;
        ps_editor_move(s, ch, write, ctx);
        return;
    }
    if (++s->escape_bytes > 16) {
        s->escape = s->attribute = s->escape_bytes = 0;
        ps_editor_bell(s, write, ctx);
        return;
    }
    if ((s->escape == 2 || s->escape == 3) && ((ch >= '0' && ch <= '9') || ch == ';')) {
        if (!s->attribute)
            s->attribute = (unsigned char)ch;
        s->escape = 3;
        return;
    }
    if (s->escape == 3) {
        if (ch == '~') {
            if (s->attribute == '3') {
                ps_editor_erase_suggestion(s, write, ctx);
                if (s->cursor < ps_text_length(s->command)) {
                    ps_editor_shift_left(s);
                    if (s->echo)
                        ps_editor_fix_cursor(s, 1, write, ctx);
                } else
                    ps_editor_bell(s, write, ctx);
            } else if (s->attribute == '1' || s->attribute == '4' || s->attribute == '7' ||
                       s->attribute == '8')
                ps_editor_move(s, s->attribute == '1' || s->attribute == '7' ? 'H' : 'F', write,
                               ctx);
            else if (s->attribute != '2')
                ps_editor_bell(s, write, ctx);
        } else
            ps_editor_move(s, ch, write, ctx);
    } else
        ps_editor_move(s, ch, write, ctx);
    s->escape = s->attribute = s->escape_bytes = 0;
}
static inline int ps_editor_feed(ps_editor *s, unsigned char ch, ps_terminal_write write,
                                 void *ctx) {
    size_t target, i;
    if (!s->escape && ch != 27)
        ps_editor_erase_suggestion(s, write, ctx);
    if (s->cr_pending && (ch == '\n' || !ch)) {
        s->cr_pending = 0;
        return PS_EDIT_NONE;
    }
    if (ch == '\r' || ch == '\n') {
        s->cr_pending = ch == '\r';
        ps_term_emit(write, ctx, "\r\n");
        return s->overflow ? PS_EDIT_TOO_LONG : PS_EDIT_SUBMIT;
    }
    s->cr_pending = 0;
    if (ch == 3) {
        ps_editor_clear(s);
        ps_term_emit(write, ctx, "^C\r\n");
        return PS_EDIT_CANCEL;
    }
    if (s->overflow)
        return PS_EDIT_NONE;
    if (s->escape) {
        ps_editor_escape(s, ch, write, ctx);
        return PS_EDIT_NONE;
    }
    if (ch == 27) {
        s->escape = 1;
        return PS_EDIT_NONE;
    }
    if (ch == '\t')
        return s->echo ? PS_EDIT_COMPLETE : PS_EDIT_NONE;
    if (ch == 1 || ch == 5) {
        ps_editor_move(s, ch == 1 ? 'H' : 'F', write, ctx);
        return PS_EDIT_NONE;
    }
    if (ch == 21 || ch == 23) {
        target = s->cursor;
        if (ch == 21)
            target = 0;
        else {
            while (target && s->command[target - 1] == ' ')
                --target;
            while (target && s->command[target - 1] != ' ')
                --target;
        }
        if (target < s->cursor) {
            ps_bytes_move(s->command + target, s->command + s->cursor,
                          ps_text_length(s->command + s->cursor) + 1);
            s->cursor = target;
            ps_editor_redraw(s, write, ctx);
        }
        return PS_EDIT_NONE;
    }
    if (ch == 4) {
        if (!s->command[0])
            return PS_EDIT_CLOSE;
        if (s->cursor < ps_text_length(s->command)) {
            ps_editor_shift_left(s);
            if (s->echo)
                ps_editor_fix_cursor(s, 1, write, ctx);
        } else
            ps_editor_bell(s, write, ctx);
        return PS_EDIT_NONE;
    }
    if (ch == 12) {
        if (s->echo) {
            ps_term_emit(write, ctx, s->ansi ? "\x1b[H\x1b[2J" : "\r\n");
            ps_editor_redraw(s, write, ctx);
        }
        return PS_EDIT_NONE;
    }
    if (ch == 8 || ch == 127) {
        if (!s->cursor)
            ps_editor_bell(s, write, ctx);
        else {
            --s->cursor;
            ps_editor_shift_left(s);
            if (s->echo) {
                ps_editor_left(s, write, ctx, 1);
                ps_editor_fix_cursor(s, 1, write, ctx);
            }
        }
        return PS_EDIT_NONE;
    }
    if (ch < 32 || ch >= 127) {
        ps_editor_bell(s, write, ctx);
        return PS_EDIT_NONE;
    }
    if (ps_text_length(s->command) >= PS_EDITOR_CAPACITY) {
        s->overflow = 1;
        ps_editor_bell(s, write, ctx);
        return PS_EDIT_NONE;
    }
    for (i = PS_EDITOR_CAPACITY; i > s->cursor; --i)
        s->command[i] = s->command[i - 1];
    s->command[s->cursor++] = (char)ch;
    if (s->echo) {
        char text[2] = {(char)ch, 0};
        ps_editor_emit_input(s, write, ctx, text);
        ps_editor_fix_cursor(s, 0, write, ctx);
    }
    return PS_EDIT_NONE;
}
static inline void ps_editor_complete(ps_editor *s, size_t start, size_t count,
                                      ps_editor_candidate candidate, void *cctx,
                                      ps_terminal_write write, void *ctx) {
    size_t typed, matches = 0, common = 0, i, n, extra;
    const char *first = 0, *name;
    if (!s->echo || s->overflow || s->escape || s->cursor != ps_text_length(s->command) ||
        start > s->cursor)
        return;
    ps_editor_erase_suggestion(s, write, ctx);
    typed = s->cursor - start;
    for (i = 0; i < count; ++i) {
        name = candidate(cctx, i);
        if (!name || !ps_text_prefix(name, s->command + start, typed))
            continue;
        if (!matches) {
            first = name;
            common = ps_text_length(name);
        } else {
            n = 0;
            while (n < common && name[n] &&
                   ps_ascii_fold((unsigned char)name[n]) == ps_ascii_fold((unsigned char)first[n]))
                ++n;
            common = n;
        }
        ++matches;
    }
    if (!matches) {
        ps_editor_bell(s, write, ctx);
        return;
    }
    extra = matches == 1 ? 1 : 0;
    if (start + common + extra > PS_EDITOR_CAPACITY) {
        ps_editor_bell(s, write, ctx);
        return;
    }
    if (common > typed || matches == 1) {
        ps_bytes_copy(s->command + start, first, common);
        s->cursor = start + common;
        if (extra)
            s->command[s->cursor++] = ' ';
        s->command[s->cursor] = 0;
        s->offset = 0;
        ps_editor_redraw(s, write, ctx);
        return;
    }
    ps_term_emit(write, ctx, "\r\n");
    for (i = 0; i < count; ++i) {
        name = candidate(cctx, i);
        if (name && ps_text_prefix(name, s->command + start, typed)) {
            ps_term_emit(write, ctx, name);
            ps_term_emit(write, ctx, "  ");
        }
    }
    ps_term_emit(write, ctx, "\r\n");
    ps_editor_redraw(s, write, ctx);
}
static inline void ps_editor_suggest(ps_editor *s, size_t start, size_t count,
                                     ps_editor_candidate candidate, void *cctx,
                                     ps_terminal_write write, void *ctx) {
    size_t i, typed, size;
    const char *name;
    if (!s->echo || !s->ansi || s->overflow || s->escape ||
        s->cursor != ps_text_length(s->command) || start >= s->cursor)
        return;
    ps_editor_erase_suggestion(s, write, ctx);
    typed = s->cursor - start;
    for (i = 0; i < count; ++i) {
        name = candidate(cctx, i);
        if (!name || !ps_text_prefix(name, s->command + start, typed) ||
            ps_text_length(name) <= typed)
            continue;
        size = ps_text_length(name) - typed;
        if (size >= sizeof(s->suggestion) || s->cursor + size + 1 > PS_EDITOR_CAPACITY)
            return;
        ps_bytes_copy(s->suggestion, name + typed, size + 1);
        ps_term_emit(write, ctx, "\x1b[90m");
        ps_editor_emit_input(s, write, ctx, s->suggestion);
        ps_term_emit(write, ctx, "\x1b[0m");
        ps_editor_left(s, write, ctx, size);
        return;
    }
}
#endif
