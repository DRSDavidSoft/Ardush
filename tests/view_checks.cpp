#include "ardush_view.hpp"
#include <cassert>
#include <cstdio>
#include <string>
static std::string out; static int led, writes;
static void capture(void *, const unsigned char *p, size_t n) { out.append(reinterpret_cast<const char *>(p), n); }
static int get_led(void *) { return led; }
static void set_led(void *, int value) { led = value; ++writes; }
static void feed(ardush *s, const char *text) { while (*text) ardush_view_feed(s, (unsigned char)*text++); }
int main() {
    ardush s; ardush_port port = {capture, get_led, set_led, nullptr};
    ardush_command commands[] = {{"view", ardush_view_begin}};
    ardush_init(&s, port, commands, 1, 1); feed(&s, "view\r\n");
    assert(s.mode == ARDUSH_VIEW && writes == 0 && !ardush_ui.view.editing());
    assert(out.find("\033[?1049h") != std::string::npos && out.find("LED") != std::string::npos);
    feed(&s, "\r\025on\r"); assert(led && writes == 1);
    feed(&s, "\t"); assert(ardush_ui.view.page() == PortableShell::DeviceView::Network);
    feed(&s, "\r"); assert(writes == 1 && !ardush_ui.view.editing());
    feed(&s, "\t"); assert(ardush_ui.view.page() == PortableShell::DeviceView::TerminalPage);
    feed(&s, "\r\025off\r"); assert(!s.editor.upper && writes == 1);
    feed(&s, "\033"); ardush_tick(&s, 35); ardush_view_tick(&s);
    assert(s.mode == ARDUSH_PROMPT && out.find("\033[?1049l") != std::string::npos);
    feed(&s, "plain\r"); out.clear(); feed(&s, "view\r");
    assert(out.find('\033') == std::string::npos);
    std::printf("Ardush view tabs/edit/exit checks passed; view=%u bytes\n", (unsigned)sizeof ardush_ui);
}
