#ifndef ARDUSH_TELNET_H
#define ARDUSH_TELNET_H
#include "ardush.h"
/* Optional byte framing, NOT a socket server. The caller owns accepting,
 * authentication, connection lifetime and reliable ordered writes. One object
 * per connection. On serial links use ardush directly, with no IAC framing. */
typedef struct { ardush shell; ps_telnet protocol; ardush_port transport; } ardush_telnet;
static inline void ardush_telnet_write(void *ctx, const unsigned char *p, size_t n) {
    ardush_telnet *t = (ardush_telnet *)ctx;
    /* Application IAC is doubled; negotiation goes directly to the wire. */
    while (n--) {
        t->transport.write(t->transport.context, p, 1);
        if (*p == 255) t->transport.write(t->transport.context, p, 1);
        ++p;
    }
}
static inline int ardush_telnet_led_get(void *ctx) {
    ardush_telnet *t = (ardush_telnet *)ctx;
    return t->transport.led_get ? t->transport.led_get(t->transport.context) : -1;
}
static inline void ardush_telnet_led_set(void *ctx, int on) {
    ardush_telnet *t = (ardush_telnet *)ctx;
    if (t->transport.led_set) t->transport.led_set(t->transport.context, on);
}
static inline void ardush_telnet_init(ardush_telnet *t, ardush_port port,
                                      const ardush_command *commands, size_t count) {
    ardush_port application;
    ps_secure_clear(t, sizeof *t); t->transport = port;
    ps_telnet_init(&t->protocol, PS_TN_BINARY, 80, 24);
    ps_telnet_greet(&t->protocol, port.write, port.context);
    application.write = ardush_telnet_write; application.context = t;
    application.led_get = ardush_telnet_led_get;
    application.led_set = port.led_set ? ardush_telnet_led_set : 0;
    ardush_init(&t->shell, application, commands, count, 1);
}
/* A TUI owner can call decode and pass DATA bytes to ArdushView::feed instead.
 * Always tick the underlying shell from the host's monotonic clock. */
static inline int ardush_telnet_decode(ardush_telnet *t, unsigned char ch, unsigned char *decoded) {
    int event = ps_telnet_feed(&t->protocol, ch, t->transport.write, t->transport.context, decoded);
    if (event == PS_TN_RESIZE) {
        t->shell.probe.column = t->protocol.columns; t->shell.probe.row = t->protocol.rows;
    } else if (event == PS_TN_TERMINAL && !ps_telnet_ansi(&t->protocol)) t->shell.editor.ansi = 0;
    return event;
}
static inline void ardush_telnet_feed(ardush_telnet *t, unsigned char ch) {
    unsigned char decoded;
    if (ardush_telnet_decode(t, ch, &decoded) == PS_TN_DATA) ardush_feed(&t->shell, decoded);
}
#endif
