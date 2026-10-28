/* Ardush serial adapter. Hardware access lives only here. */
#include <Arduino.h>
#if defined(ARDUINO_ARCH_AVR) && !defined(PS_TERMINAL_SOFT_DIV10)
#define PS_TERMINAL_SOFT_DIV10 1
#endif
#ifndef ARDUSH_SERIAL
#define ARDUSH_SERIAL Serial
#endif
#ifndef ARDUSH_BAUD
#define ARDUSH_BAUD 9600
#endif
#ifndef ARDUSH_INTERACTIVE
#define ARDUSH_INTERACTIVE 1
#endif
#ifndef ARDUSH_LED_PIN
#ifdef LED_BUILTIN
#define ARDUSH_LED_PIN LED_BUILTIN
#else
#define ARDUSH_LED_PIN -1
#endif
#endif
#ifndef ARDUSH_LED_ACTIVE_LOW
#define ARDUSH_LED_ACTIVE_LOW 0
#endif
#ifndef ARDUSH_ENABLE_DEVICE_VIEW
#if defined(ARDUINO_ARCH_AVR)
#define ARDUSH_ENABLE_DEVICE_VIEW 0
#else
#define ARDUSH_ENABLE_DEVICE_VIEW 1
#endif
#endif
#include "src/ardush.h"
#if ARDUSH_ENABLE_DEVICE_VIEW
#include "src/ardush_view.hpp"
#endif
static ardush shell;
static void serial_write(void *, const unsigned char *bytes, size_t length) {
    ARDUSH_SERIAL.write(bytes, length);
}
static int led_get(void *) {
#if ARDUSH_LED_PIN >= 0
    return (digitalRead(ARDUSH_LED_PIN) != LOW) != (ARDUSH_LED_ACTIVE_LOW != 0);
#else
    return -1;
#endif
}
static void led_set(void *, int on) {
#if ARDUSH_LED_PIN >= 0
    digitalWrite(ARDUSH_LED_PIN, (on != (ARDUSH_LED_ACTIVE_LOW != 0)) ? HIGH : LOW);
#else
    (void)on;
#endif
}
/* Add commands here; callbacks receive decoded arguments and caller state. */
static void version(ardush *s, unsigned, char *const *) { ardush_put(s, "Ardush 2.0\r\n"); }
static const ardush_command commands[] = {
    {"version", version},
#if ARDUSH_ENABLE_DEVICE_VIEW
    {"view", ardush_view_begin},
#endif
};
void setup() {
    ARDUSH_SERIAL.begin(ARDUSH_BAUD);
#if ARDUSH_LED_PIN >= 0
    pinMode(ARDUSH_LED_PIN, OUTPUT); led_set(nullptr, 0);
#endif
    ardush_port port = {serial_write, led_get, led_set, nullptr};
    ardush_init(&shell, port, commands, sizeof commands / sizeof commands[0], ARDUSH_INTERACTIVE);
}
void loop() {
    ardush_tick(&shell, (uint32_t)millis());
#if ARDUSH_ENABLE_DEVICE_VIEW
    ardush_view_tick(&shell);
#endif
    for (unsigned n = 0; n < 32 && ARDUSH_SERIAL.available() > 0; ++n) {
        unsigned char byte = (unsigned char)ARDUSH_SERIAL.read();
#if ARDUSH_ENABLE_DEVICE_VIEW
        ardush_view_feed(&shell, byte);
#else
        ardush_feed(&shell, byte);
#endif
    }
    // Application work here. exit closes input until an explicit ardush_init(),
    // e.g. when your transport accepts a new connection. Never wait for USB.
}
