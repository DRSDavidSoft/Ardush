#include "ardush.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char out[32768]; static size_t used; static int led, writes, calls;
static void capture(void *ctx, const unsigned char *p, size_t n) {
    (void)ctx; assert(used + n < sizeof out); memcpy(out + used, p, n); used += n; out[used] = 0;
}
static int get_led(void *ctx) { (void)ctx; return led; }
static void set_led(void *ctx, int value) { (void)ctx; led = value; ++writes; }
static void custom(ardush *s, unsigned argc, char *const *argv) {
    (void)s; assert(argc == 4 && !strcmp(argv[1], "a b") && !strcmp(argv[2], "") && !strcmp(argv[3], "c d")); ++calls;
}
static void feed(ardush *s, const char *text) { while (*text) ardush_feed(s, (unsigned char)*text++); }
static void clear_output(void) { used = 0; out[0] = 0; }
int main(void) {
    ardush s; const ardush_command commands[] = {{"custom", custom}};
    const ardush_port port = {capture, get_led, set_led, 0};
    ardush_init(&s, port, commands, 1, 1);
    assert(strstr(out, "Ardush> ") && s.editor.upper);
    feed(&s, "noupper\r\ncustom 'a b' \"\" c\\ d\r\n"); assert(calls == 1 && !s.editor.upper);
    feed(&s, "custom 'bad\r"); assert(calls == 1 && strstr(out, "unclosed quote"));
    clear_output(); feed(&s, "echo  \"raw\"\r"); assert(strstr(out, " \"raw\"\r\n"));
    feed(&s, "leX\b\t\r\n"); assert(led && writes == 1);
    feed(&s, "\033[A\r"); assert(!led && writes == 2);
    feed(&s, "led on\rled bad\r"); assert(led && writes == 3);
    feed(&s, "led"); for (unsigned i = 0; i < 90; ++i) ardush_feed(&s, ' ');
    feed(&s, "\r"); assert(writes == 3);
    feed(&s, "getcur\r\n"); assert(s.mode == ARDUSH_PROBE);
    feed(&s, "\033[123;240R"); assert(s.mode == ARDUSH_PROMPT && s.probe.column == 240 && s.probe.row == 123);
    feed(&s, "getcur\rx"); assert(!strcmp(s.editor.command, "x")); feed(&s, "\003");
    ardush_tick(&s, 0xfffffff0u); feed(&s, "getcur\r"); ardush_tick(&s, 484);
    assert(s.mode == ARDUSH_PROMPT && strstr(out, "probe timeout"));
    feed(&s, "getcur\r\033[99999;1R"); assert(!s.editor.command[0]);
    clear_output(); feed(&s, "keycodes\r\nAz\003"); assert(strstr(out, "65\r\n122\r\n"));
    assert(s.mode == ARDUSH_PROMPT); feed(&s, "upper\r"); assert(s.editor.upper);
    clear_output(); feed(&s, "clear\rtest\r"); assert(strstr(out, "\033[H\033[2J") && strchr(out, 5));
    feed(&s, "help\rstatus\rhistory-clear\r"); assert(!s.editor.count && strstr(out, "custom\r\n"));
    clear_output(); feed(&s, "plain\r"); clear_output(); feed(&s, "echo hello\r"); assert(!strchr(out, 27));
    feed(&s, "ansi\r"); assert(s.editor.ansi);
    feed(&s, "exit\rled\r"); assert(s.mode == ARDUSH_CLOSED && writes == 3);
    clear_output(); ardush_init(&s, port, commands, 1, 0); feed(&s, "echo ok\nstatus\ngetcur\n");
    assert(!strchr(out, 27) && !strstr(out, "Ardush>") && !strncmp(out, "ok\r\n", 4));
    assert(strstr(out, "interactive only") && s.status == 3);
    feed(&s, "missing\n"); assert(s.status == 127);
    feed(&s, "echo success\n"); assert(s.status == 0);
    printf("Ardush command/editor/probe/batch checks passed; session=%u bytes\n", (unsigned)sizeof s);
    return 0;
}
