/* Runs actual 16-bit-int AVR code in simavr. Test transport/hardware fixtures
 * only; this does not access or validate a physical board. */
#define PS_TERMINAL_SOFT_DIV10 1
#include "ardush.h"
#include <avr/io.h>
#include <avr/sleep.h>
static ardush session;
static int led, changes;
static unsigned output_bytes;
static void sink(void *ctx, const unsigned char *p, size_t n) { (void)ctx; (void)p; output_bytes += n; }
static int get_led(void *ctx) { (void)ctx; return led; }
static void set_led(void *ctx, int value) { (void)ctx; led = value; ++changes; }
static void feed(const char *s) { while (*s) ardush_feed(&session, (unsigned char)*s++); }
static void uart(const char *text) {
    while (*text) { while (!(UCSR0A & _BV(UDRE0))) {} UDR0 = *text++; }
}
extern unsigned char __bss_end;
static unsigned char *watermark_start, *watermark_end;
int main(void) {
    ardush_port port = {sink, get_led, set_led, 0};
    UBRR0 = 103; UCSR0B = _BV(TXEN0); UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
    watermark_start = &__bss_end; watermark_end = (unsigned char *)(SP - 32);
    for (unsigned char *p = watermark_start; p < watermark_end; ++p) *p = 0xa5;
    ardush_init(&session, port, 0, 0, 1);
    feed("le\t\r\n");
    if (!led || changes != 1) goto fail;
    feed("\033[A\r"); if (led || changes != 2) goto fail;
    feed("getcur\r\n\033[123;240R");
    if (session.probe.row != 123 || session.probe.column != 240 || session.mode != ARDUSH_PROMPT) goto fail;
    ardush_tick(&session, 0xfffffff0UL); feed("getcur\r"); ardush_tick(&session, 484);
    if (session.mode != ARDUSH_PROMPT) goto fail;
    feed("led "); for (unsigned i = 0; i < 80; ++i) ardush_feed(&session, 'x');
    feed("\r"); if (changes != 2) goto fail;
    feed("keycodes\r\nA\003exit\r");
    if (session.mode != ARDUSH_CLOSED || !output_bytes) goto fail;
    {
        char digits[11]; unsigned spare = 0;
        while (watermark_start + spare < watermark_end && watermark_start[spare] == 0xa5) ++spare;
        digits[ps_decimal(digits, spare)] = 0;
        uart("ARDUSH_AVR_PASS untouched_stack_gap="); uart(digits); uart(" bytes\n");
    }
    for (;;) { sleep_mode(); }
fail:
    uart("ARDUSH_AVR_FAIL\n"); for (;;) { sleep_mode(); }
}
