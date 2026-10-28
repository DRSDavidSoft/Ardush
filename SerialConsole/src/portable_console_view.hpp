#ifndef PS_CONSOLE_VIEW_HPP
#define PS_CONSOLE_VIEW_HPP
#include "portable_console.h"
#ifndef PS_DEVICE_VIEW_DIAGNOSTICS
#define PS_DEVICE_VIEW_DIAGNOSTICS 0
#endif
#include "portable-shell/portable_device_view.hpp"
/* Optional application mapping; the renderer is canonical PortableShell code.
 * Default Uno excludes this view. No hardware writes during render/navigation.
 * One convenience instance per translation unit; applications can own others. */
struct ConsoleView {
    PortableShell::DeviceView view;
    uint32_t last_input = 0;
    bool alternate = false;
    void snapshot(portable_console *s, PortableShell::DeviceView::Snapshot &snap, PortableShell::DeviceView::Row *rows) {
        const int led = s->port.led_get ? s->port.led_get(s->port.context) : -1;
        snap.title = PS_CONSOLE_TITLE; snap.terminalSettings = true;
#ifndef PS_CONSOLE_ASCII_ONLY
        snap.unicode = s->editor.ansi != 0;
#endif
        if (s->probe.column) snap.columns = (uint16_t)s->probe.column;
        if (s->probe.row) snap.lines = (uint16_t)s->probe.row;
        if (view.page() == PortableShell::DeviceView::Network) {
            rows[0] = {"Transport", "transport", "Byte stream", false}; snap.rowCount = 1;
        } else if (view.page() == PortableShell::DeviceView::TerminalPage) {
            rows[0] = {"Uppercase echo", "upper", s->editor.upper ? "on" : "off", true};
            rows[1] = {"ANSI", "ansi", s->editor.ansi ? "on" : "off", false}; snap.rowCount = 2;
        } else {
            rows[0] = {"LED", "led", led < 0 ? "unknown" : led ? "on" : "off", led >= 0 && s->port.led_set};
            rows[1] = {"Uppercase echo", "upper", s->editor.upper ? "on" : "off", true}; snap.rowCount = 2;
        }
        snap.rows = rows;
    }
    void render(portable_console *s) {
        PortableShell::DeviceView::Snapshot snap; PortableShell::DeviceView::Row rows[2];
        snapshot(s, snap, rows);
        view.render(snap, [s](const uint8_t *p, size_t n) { portable_console_write(s, p, n); });
    }
    void apply(portable_console *s, const PortableShell::DeviceView::Action &action) {
        using View = PortableShell::DeviceView;
        if (action.type == View::Action::Exit) {
            if (alternate) portable_console_put(s, "\033[?1049l");
            alternate = false; s->mode = PS_CONSOLE_PROMPT; portable_console_prompt(s); return;
        }
        if (action.type == View::Action::Set) {
            const bool on = portable_console_equal(action.value, "on");
            if (!on && !portable_console_equal(action.value, "off")) { view.feedback("Use on or off"); return; }
            if (portable_console_equal(action.key, "upper")) s->editor.upper = on;
            else if (portable_console_equal(action.key, "led") && s->port.led_get && s->port.led_set && s->port.led_get(s->port.context) >= 0)
                s->port.led_set(s->port.context, on);
            else { view.feedback("Unavailable"); return; }
            view.feedback("Updated");
        }
    }
    void begin(portable_console *s) {
        if (!portable_console_terminal_only(s)) return;
        s->mode = PS_CONSOLE_VIEW; alternate = s->editor.ansi != 0;
        if (alternate) portable_console_put(s, "\033[?1049h");
        view.begin(alternate); render(s);
    }
    void feed(portable_console *s, unsigned char byte) {
        if (s->mode != PS_CONSOLE_VIEW) { portable_console_feed(s, byte); return; }
        if (s->skip_lf && (byte == '\n' || !byte)) { s->skip_lf = 0; return; }
        s->skip_lf = 0;
        PortableShell::DeviceView::Snapshot snap; PortableShell::DeviceView::Row rows[2];
        PortableShell::DeviceView::Action action;
        snapshot(s, snap, rows); last_input = s->now;
        view.feed(byte, snap, action); apply(s, action);
        if (s->mode == PS_CONSOLE_VIEW) render(s);
    }
    void tick(portable_console *s) {
        if (s->mode != PS_CONSOLE_VIEW) return;
        if (view.escapePending() && (uint32_t)(s->now - last_input) >= 35) {
            PortableShell::DeviceView::Action action; view.flushEscape(action); apply(s, action);
        }
        if (s->mode == PS_CONSOLE_VIEW) render(s);
    }
};
static ConsoleView portable_console_ui;
static inline void portable_console_view_begin(portable_console *s, unsigned, char *const *) { portable_console_ui.begin(s); }
static inline void portable_console_view_feed(portable_console *s, unsigned char ch) { portable_console_ui.feed(s, ch); }
static inline void portable_console_view_tick(portable_console *s) { portable_console_ui.tick(s); }
#endif
