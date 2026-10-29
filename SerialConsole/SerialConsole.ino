/* SerialConsole serial adapter. Hardware access lives only here. */
#include <Arduino.h>
#if defined(ARDUINO_ARCH_AVR) && !defined(PS_TERMINAL_SOFT_DIV10)
#define PS_TERMINAL_SOFT_DIV10 1
#endif
#ifndef PS_CONSOLE_SERIAL
#define PS_CONSOLE_SERIAL Serial
#endif
#ifndef PS_CONSOLE_BAUD
#define PS_CONSOLE_BAUD 9600
#endif
#ifndef PS_CONSOLE_INTERACTIVE
#define PS_CONSOLE_INTERACTIVE 1
#endif
#ifndef PS_CONSOLE_LED_PIN
#ifdef LED_BUILTIN
#define PS_CONSOLE_LED_PIN LED_BUILTIN
#else
#define PS_CONSOLE_LED_PIN -1
#endif
#endif
#ifndef PS_CONSOLE_LED_ACTIVE_LOW
#define PS_CONSOLE_LED_ACTIVE_LOW 0
#endif
#ifndef PS_CONSOLE_ENABLE_DEVICE_VIEW
#define PS_CONSOLE_ENABLE_DEVICE_VIEW 1
#endif
#include "src/portable_console.h"
#if PS_CONSOLE_ENABLE_DEVICE_VIEW
#include "src/portable_console_view.hpp"
#endif
static portable_console shell;
static void serial_write(void *, const unsigned char *bytes, size_t length) {
    PS_CONSOLE_SERIAL.write(bytes, length);
}
static int led_get(void *) {
#if PS_CONSOLE_LED_PIN >= 0
    return (digitalRead(PS_CONSOLE_LED_PIN) != LOW) != (PS_CONSOLE_LED_ACTIVE_LOW != 0);
#else
    return -1;
#endif
}
static void led_set(void *, int on) {
#if PS_CONSOLE_LED_PIN >= 0
    digitalWrite(PS_CONSOLE_LED_PIN, (on != (PS_CONSOLE_LED_ACTIVE_LOW != 0)) ? HIGH : LOW);
#else
    (void)on;
#endif
}
/* Add commands here; callbacks receive decoded arguments and caller state. */
static void version(portable_console *s, unsigned, char *const *) { portable_console_put(s, "Console 2.3\r\n"); }
static const portable_console_command commands[] = {
    {"version", version},
#if PS_CONSOLE_ENABLE_DEVICE_VIEW
    {"view", portable_console_view_begin},
#endif
};
void setup() {
    PS_CONSOLE_SERIAL.begin(PS_CONSOLE_BAUD);
#if PS_CONSOLE_LED_PIN >= 0
    pinMode(PS_CONSOLE_LED_PIN, OUTPUT); led_set(nullptr, 0);
#endif
    portable_console_port port = {serial_write, led_get, led_set, nullptr};
    portable_console_init(&shell, port, commands, sizeof commands / sizeof commands[0], PS_CONSOLE_INTERACTIVE);
}
void loop() {
    portable_console_tick(&shell, (uint32_t)millis());
#if PS_CONSOLE_ENABLE_DEVICE_VIEW
    portable_console_view_tick(&shell);
#endif
    for (unsigned n = 0; n < 32 && PS_CONSOLE_SERIAL.available() > 0; ++n) {
        unsigned char byte = (unsigned char)PS_CONSOLE_SERIAL.read();
#if PS_CONSOLE_ENABLE_DEVICE_VIEW
        portable_console_view_feed(&shell, byte);
#else
        portable_console_feed(&shell, byte);
#endif
    }
    // Application work here. exit closes input until an explicit portable_console_init(),
    // e.g. when your transport accepts a new connection. Never wait for USB.
}
