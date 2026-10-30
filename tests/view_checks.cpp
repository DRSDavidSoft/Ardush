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
#if PS_MOUSE && PS_DEVICE_VIEW_TABLE
    assert(out.find("\033[?1006h")!=std::string::npos);
    feed(&s,"\033[<0;50;7M\033[<0;50;7m");assert(led&&writes==1);
    feed(&s,"\033[<0;50;7M\033[<0;50;7m");assert(!led&&writes==2);
    writes=0; // Start the existing keyboard/edit scenario from its original baseline.
#endif
    out.clear(); feed(&s, "\r\025on");
    assert(!led && writes == 0 && portable_console_ui.view.editing());
#if PS_DEVICE_VIEW_TABLE
    assert(out.find("Edit LED:") == std::string::npos);
#elif PS_DEVICE_VIEW_INLINE_EDIT
    assert(out.find("\033[4;29H") != std::string::npos && out.find("Edit LED:") == std::string::npos);
#else
    assert(out.find("Edit LED:") != std::string::npos);
#endif
    feed(&s, "\033"); portable_console_tick(&s, 35); portable_console_view_tick(&s);
    assert(!led && writes == 0 && !portable_console_ui.view.editing());
    feed(&s, "\r\025on\r"); assert(led && writes == 1);
    feed(&s, "\t"); assert(portable_console_ui.view.page() == PortableShell::DeviceView::Network);
    feed(&s, "\r"); assert(writes == 1 && !portable_console_ui.view.editing());
    feed(&s, "\t"); assert(portable_console_ui.view.page() == PortableShell::DeviceView::TerminalPage);
    feed(&s, "\r\025off\r"); assert(!s.editor.upper && writes == 1);
    feed(&s, "\033"); portable_console_tick(&s, 70); portable_console_view_tick(&s);
    assert(s.mode == PS_CONSOLE_PROMPT && out.find("\033[?1049l") != std::string::npos);
#if PS_MOUSE
    assert(out.find("\033[?1006l")!=std::string::npos);
#endif
    out.clear();feed(&s,"view vertical\r");
    assert(s.mode==PS_CONSOLE_VIEW&&writes==1);
    assert(portable_console_ui.view.tabLayout()==PortableShell::DeviceView::VerticalTabs);
    feed(&s,"\033");portable_console_tick(&s,105);portable_console_view_tick(&s);
    out.clear();feed(&s,"view invalid\r");assert(s.mode==PS_CONSOLE_PROMPT);
    assert(out.find("view [horizontal|vertical]")!=std::string::npos);
    feed(&s, "plain\r"); out.clear(); feed(&s, "view\r");
    assert(portable_console_ui.view.tabLayout()==PortableShell::DeviceView::HorizontalTabs);
    assert(out.find('\033') == std::string::npos);
    out.clear(); feed(&s, "\r\025off");
    assert(out.find('\033') == std::string::npos);
#if PS_DEVICE_VIEW_TABLE
    assert(out.find("Edit LED:") == std::string::npos);
#elif PS_DEVICE_VIEW_INLINE_EDIT
    assert(out.find("Edit LED:") == std::string::npos);
#endif
    assert(out.find("off") != std::string::npos && led && writes == 1);
    std::printf("SerialConsole view tabs/edit/exit checks passed; view=%u bytes\n", (unsigned)sizeof portable_console_ui);
}
