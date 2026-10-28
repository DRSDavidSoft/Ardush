#ifndef PS_CONSOLE_TELNET_H
#define PS_CONSOLE_TELNET_H
#include "portable_console.h"
/* Optional byte framing, NOT a socket server. The caller owns accepting,
 * authentication, connection lifetime and reliable ordered writes. One object
 * per connection. On serial links use portable_console directly, with no IAC framing. */
typedef struct { portable_console shell; ps_telnet protocol; portable_console_port transport; } portable_console_telnet;
static inline void portable_console_telnet_write(void *ctx, const unsigned char *p, size_t n) {
    portable_console_telnet *t = (portable_console_telnet *)ctx;
    /* Application IAC is doubled; negotiation goes directly to the wire. */
    while (n--) {
        t->transport.write(t->transport.context, p, 1);
        if (*p == 255) t->transport.write(t->transport.context, p, 1);
        ++p;
    }
}
static inline int portable_console_telnet_led_get(void *ctx) {
    portable_console_telnet *t = (portable_console_telnet *)ctx;
    return t->transport.led_get ? t->transport.led_get(t->transport.context) : -1;
}
static inline void portable_console_telnet_led_set(void *ctx, int on) {
    portable_console_telnet *t = (portable_console_telnet *)ctx;
    if (t->transport.led_set) t->transport.led_set(t->transport.context, on);
}
static inline void portable_console_telnet_init(portable_console_telnet *t, portable_console_port port,
                                      const portable_console_command *commands, size_t count) {
    portable_console_port application;
    ps_secure_clear(t, sizeof *t); t->transport = port;
    ps_telnet_init(&t->protocol, PS_TN_BINARY, 80, 24);
    ps_telnet_greet(&t->protocol, port.write, port.context);
    application.write = portable_console_telnet_write; application.context = t;
    application.led_get = portable_console_telnet_led_get;
    application.led_set = port.led_set ? portable_console_telnet_led_set : 0;
    portable_console_init(&t->shell, application, commands, count, 1);
}
/* A TUI owner can call decode and pass DATA bytes to ConsoleView::feed instead.
 * Always tick the underlying shell from the host's monotonic clock. */
static inline int portable_console_telnet_decode(portable_console_telnet *t, unsigned char ch, unsigned char *decoded) {
    int event = ps_telnet_feed(&t->protocol, ch, t->transport.write, t->transport.context, decoded);
    if (event == PS_TN_RESIZE) {
        t->shell.probe.column = t->protocol.columns; t->shell.probe.row = t->protocol.rows;
    } else if (event == PS_TN_TERMINAL && !ps_telnet_ansi(&t->protocol)) t->shell.editor.ansi = 0;
    return event;
}
static inline void portable_console_telnet_feed(portable_console_telnet *t, unsigned char ch) {
    unsigned char decoded;
    if (portable_console_telnet_decode(t, ch, &decoded) == PS_TN_DATA) portable_console_feed(&t->shell, decoded);
}
#endif
