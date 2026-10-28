#include "portable_console_view.hpp"
#include <cassert>
#include <cstdio>
#include <string>
static std::string out; static int led, writes;
static void capture(void *, const unsigned char *p, size_t n) { out.append(reinterpret_cast<const char *>(p), n); }
static int get_led(void *) { return led; }
static void set_led(void *, int value) { led = value; ++writes; }
static void feed(portable_console *s, const char *text) { while (*text) portable_console_view_feed(s, (unsigned char)*text++); }
int main() {
    portable_console s; portable_console_port port = {capture, get_led, set_led, nullptr};
    portable_console_command commands[] = {{"view", portable_console_view_begin}};
    portable_console_init(&s, port, commands, 1, 1); feed(&s, "view\r\n");
    assert(s.mode == PS_CONSOLE_VIEW && writes == 0 && !portable_console_ui.view.editing());
    assert(out.find("\033[?1049h") != std::string::npos && out.find("LED") != std::string::npos);
#ifdef PS_CONSOLE_ASCII_ONLY
    for (unsigned char byte : out) assert(byte < 128);
#else
    assert(out.find("\xe2\x86\x91/\xe2\x86\x93") != std::string::npos);
#endif
    feed(&s, "\r\025on\r"); assert(led && writes == 1);
    feed(&s, "\t"); assert(portable_console_ui.view.page() == PortableShell::DeviceView::Network);
    feed(&s, "\r"); assert(writes == 1 && !portable_console_ui.view.editing());
    feed(&s, "\t"); assert(portable_console_ui.view.page() == PortableShell::DeviceView::TerminalPage);
    feed(&s, "\r\025off\r"); assert(!s.editor.upper && writes == 1);
    feed(&s, "\033"); portable_console_tick(&s, 35); portable_console_view_tick(&s);
    assert(s.mode == PS_CONSOLE_PROMPT && out.find("\033[?1049l") != std::string::npos);
    feed(&s, "plain\r"); out.clear(); feed(&s, "view\r");
    assert(out.find('\033') == std::string::npos);
    std::printf("SerialConsole view tabs/edit/exit checks passed; view=%u bytes\n", (unsigned)sizeof portable_console_ui);
}
