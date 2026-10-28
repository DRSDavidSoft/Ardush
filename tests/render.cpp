/* Host-only fixture: actual emitted bytes, simulated LED, One Dark replay.
 * Link to PortableShell tests/render.c; no copied terminal renderer here. */
#include "portable_console_view.hpp"
#include <cstdio>
extern "C" void ps_replay_reset(void);
extern "C" void ps_replay_feed(const unsigned char *, unsigned);
extern "C" unsigned ps_replay_svg(char *);
static char svg[400000];
static void write_bytes(void *, const unsigned char *p, size_t n) { ps_replay_feed(p, (unsigned)n); }
static int get_led(void *) { return 0; }
static void set_led(void *, int) {}
static int save(const char *path) {
    unsigned n = ps_replay_svg(svg); FILE *f = std::fopen(path, "wb");
    if (!f) return 1;
    int bad = std::fwrite(svg, 1, n, f) != n; return std::fclose(f) || bad;
}
static void feed(portable_console *s, const char *text) { while (*text) portable_console_view_feed(s, (unsigned char)*text++); }
int main(int argc, char **argv) {
    if (argc != 5) return 2;
    portable_console s; portable_console_port port = {write_bytes, get_led, set_led, nullptr};
    ps_replay_reset(); portable_console_init(&s, port, nullptr, 0, 1); portable_console_ui.begin(&s);
    if (save(argv[1])) return 3;
    feed(&s, "\r\025on"); if (save(argv[2])) return 4;
    feed(&s, "\033"); portable_console_tick(&s, 35); portable_console_view_tick(&s);
    feed(&s, "\t"); if (save(argv[3])) return 5;
    feed(&s, "\t"); return save(argv[4]) ? 6 : 0;
}
