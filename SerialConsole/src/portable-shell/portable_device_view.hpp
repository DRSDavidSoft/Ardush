#ifndef PORTABLE_DEVICE_VIEW_HPP
#define PORTABLE_DEVICE_VIEW_HPP
#include "portable_shell_editor.hpp"
#include "portable_style.h"
/* MIT Copyright (c) 2026 AtomicDeploy contributors. Canonical snapshot/action
 * device view. Source lineage is in the donor inventory. Hardware calls and routing
 * stay with the caller. Include only in products requiring this optional view. */
#ifndef PS_DEVICE_VIEW_DIAGNOSTICS
#define PS_DEVICE_VIEW_DIAGNOSTICS 1
#endif
#ifndef PS_DEVICE_VIEW_INLINE_EDIT
#define PS_DEVICE_VIEW_INLINE_EDIT 1
#endif
#ifndef PS_DEVICE_VIEW_TABLE
#define PS_DEVICE_VIEW_TABLE 1
#endif
#ifndef PS_DEVICE_VIEW_WIDGETS
#define PS_DEVICE_VIEW_WIDGETS 1
#endif
#ifndef PS_DEVICE_VIEW_DIALOGS
#define PS_DEVICE_VIEW_DIALOGS 1
#endif
#ifndef PS_DEVICE_VIEW_MAX_ROWS
#define PS_DEVICE_VIEW_MAX_ROWS 64
#endif
#ifndef PS_DEVICE_VIEW_FRAME_BYTES
#define PS_DEVICE_VIEW_FRAME_BYTES 32768
#endif
#if PS_DEVICE_VIEW_TABLE && PS_DEVICE_VIEW_FRAME_BYTES < 16384
#error Rich tables require a frame budget of at least 16384 bytes
#endif
#if PS_DEVICE_VIEW_MAX_ROWS < 1 || PS_DEVICE_VIEW_MAX_ROWS > 255
#error PS_DEVICE_VIEW_MAX_ROWS must be in 1..255
#endif
// Allocation-free device presentation with filtering, deterministic state,
// bounded rendering and sanitized text. The caller owns application behavior.
// Snapshots are observations; only the caller's common router may authorize
// and execute an explicit Action. Entering/rendering never produces an action.
namespace PortableShell {
class DeviceView {
  public:
    enum Theme : uint8_t { Violet, Red, Lumen, OneDark };
    enum Kind : uint8_t { Text, Toggle, Level, Choice };
    enum Page : uint8_t {
        Device,
        Network,
        Led,
        Pixel,
        Game,
        TerminalPage,
        Cloud,
        Notifications,
        Clock,
        PageCount
    };
    struct Row {
        const char *label = nullptr;
        const char *key = nullptr;
        const char *value = nullptr;
        bool editable = false;
        bool secret = false;
        Kind kind = Text;
        const char *category = "";
        uint32_t minimum = 0, maximum = 65535, step = 1;
        const char *const *choices = nullptr;
        uint8_t choiceCount = 0;
        bool pending = false;
        bool meter = false; // Meaningful bounded levels/PWM, not ports or timestamps.
        constexpr Row(const char *label_ = nullptr, const char *key_ = nullptr,
                      const char *value_ = nullptr, bool editable_ = false, bool secret_ = false)
            : label(label_), key(key_), value(value_), editable(editable_), secret(secret_) {}
    };
    struct Snapshot {
        const char *title = "Device";
        const Row *rows = nullptr;               // Current page only, retained through feed/render.
        uint8_t rowCount = 0;                    // Bounded by PS_DEVICE_VIEW_MAX_ROWS.
        const uint8_t *cellStates = nullptr;     // Exactly 80: 0 white, 1 red, 2 off.
        uint16_t pixelCount = 0, usedPixels = 0; // Wire capacity and configured prefix.
        const uint8_t *groupCounts = nullptr;
        const uint16_t *groupStarts = nullptr; // Exactly groupCount prefix spans.
        uint8_t groupCount = 0;
        uint32_t groupLayoutRevision = 0;
        bool canLeds = false, canPixels = false; // Independently ACKed, default denied.
        bool canGame = false, gameReady = false, terminalSettings = false, unicode = false;
        bool cloudSettings = false, notificationSettings = false, clockSettings = false;
        uint16_t enabledKeys = 0, columns = 80, lines = 24;
        uint8_t winner = 0;
    };
    struct Action {
        enum Type : uint8_t { None, Set, Cell, Pixel, Refresh, Exit, Press, Reset } type = None;
        char key[32]{}, value[385]{};
        // Cell: logical1..80; Pixel: ZERO-based physical span from this snapshot.
        uint16_t index = 0;
        uint8_t count = 0;
        uint32_t groupLayoutRevision = 0;
        bool group = false;
        Action() = default;
        Action(const Action &) = default;
        Action &operator=(const Action &) = default;
        ~Action() {
            volatile char *p = value;
            for (size_t i = 0; i < sizeof(value); ++i)
                p[i] = 0;
        }
    };
    ~DeviceView() {
        clearDraft();
    } // No terminal output, SDK call, or allocation on teardown.
    void begin(bool ansi = true) {
        clearDraft();
        active_ = true;
        ansi_ = ansi;
        page_ = Device;
        selected_ = 0;
        grid_ = false;
        dirty_ = full_ = true;
        lastHash_ = 0;
        escape_ = escapeBytes_ = attribute_ = 0;
        crPending_ = false;
        cell_ = 1;
        pixel_ = 0;
        group_ = false;
        pixelColor_ = 1;
        notice_[0] = 0;
    }
    bool active() const {
        return active_;
    }
    bool editing() const {
        return editing_;
    }
    bool escapePending() const {
        return escape_ == 1;
    }
    Page page() const {
        return page_;
    }
    uint8_t selectedCell() const {
        return cell_;
    }
    uint16_t selectedPixel() const {
        return pixel_;
    }
    bool group() const {
        return group_;
    }
    const char *draft() const {
        return draft_;
    } // Secret draft is NEVER rendered.
    void invalidate() {
        if (!editing_)
            dirty_ = true;
    }
    void setAnsi(bool enabled) {
        if (ansi_ != enabled) {
            ansi_ = enabled;
            dirty_ = full_ = true;
        }
    }
    void setTheme(Theme theme) { if (theme <= OneDark && theme_ != theme) {theme_=theme;dirty_=full_=true;} }
    Theme theme() const { return theme_; }
    void setTrueColor(bool enabled) { if (trueColor_!=enabled) {trueColor_=enabled;dirty_=full_=true;} }
    bool trueColor() const { return trueColor_; }
    void feedback(const char *text) {
        setNotice(text);
    }
    // A standalone ESC needs an idle delimiter (~35ms); CSI may span callbacks.
    // Only the transport owner decides that delimiter. Never flush each chunk.
    bool flushEscape(Action &action) {
        action = Action{};
        if (escape_ != 1)
            return false;
        escape_ = escapeBytes_ = 0;
        if (editing_) {
            clearDraft();
            dirty_ = full_ = true;
            return false;
        }
        if (!active_) {
            return false;
        }
        active_ = false;
        action.type = Action::Exit;
        return true;
    }
    bool feed(uint8_t ch, const Snapshot &snapshot, Action &action) {
        action = Action{};
        if (!active_)
            return false;
        if (crPending_ && (ch == '\n' || ch == 0)) {
            crPending_ = false;
            return false;
        }
        if (ch != '\n' && ch != '\r')
            crPending_ = false;
        if (escape_) {
            if (escape_ == 1) {
                if (ch == '[' || ch == 'O') {
                    escape_ = 2;
                    return false;
                }
                escape_ = escapeBytes_ = 0;
                if (ch >= 'A' && ch <= 'D')
                    return arrow(ch, snapshot, action);
                return false;
            }
            if (++escapeBytes_ > 16) {
                escape_ = escapeBytes_ = 0;
                return false;
            }
            if (ch >= '0' && ch <= '9')
                attribute_ = attribute_ > 24 ? 255 : uint8_t(attribute_ * 10 + ch - '0');
            if (ch >= 0x40 && ch <= 0x7e) {
                const uint8_t attribute = attribute_;
                escape_ = escapeBytes_ = attribute_ = 0;
                if (snapshot.lines < 16 || snapshot.columns < 40)
                    return false;
                if (PS_DEVICE_VIEW_DIALOGS && PS_DEVICE_VIEW_TABLE && ch == 'Q')
                    return startDialog(snapshot, action);
                if (ch == '~') {
                    if (PS_DEVICE_VIEW_DIALOGS && PS_DEVICE_VIEW_TABLE && attribute == 12)
                        return startDialog(snapshot,action);
                    if (attribute == 1 || attribute == 7)
                        ch = 'H';
                    else if (attribute == 4 || attribute == 8)
                        ch = 'F';
                    else if (attribute == 5)
                        ch = 'P';
                    else if (attribute == 6)
                        ch = 'N';
                    else if (attribute == 3 && editing_)
                        return edit(4, snapshot, action);
                    else
                        return false;
                }
                if (ch == 'Z' && !editing_) {
                    nextPage(snapshot, -1);
                    return false;
                }
                return arrow(ch, snapshot, action);
            }
            return false;
        }
        if (ch == 27) {
            escape_ = 1;
            escapeBytes_ = attribute_ = 0;
            return false;
        }
        if (ch == 3) {
            if (editing_) {
                clearDraft();
                dirty_ = full_ = true;
                return false;
            }
            active_ = false;
            action.type = Action::Exit;
            return true;
        }
        if (ch == 12) {
            dirty_ = full_ = true;
            return false;
        }
        if (snapshot.lines < 16 || snapshot.columns < 40)
            return false;
        if (editing_)
            return edit(ch, snapshot, action);
        if (PS_DEVICE_VIEW_WIDGETS && !grid_ && (ch == ' ' || ch == '+' || ch == '-'))
            return adjust(ch, snapshot, action);
        if (ch == '\t') {
            nextPage(snapshot);
            return false;
        }
        if (ch == 18) {
            action.type = Action::Refresh;
            return true;
        }
        if (page_ == Game) {
            if (ch >= '1' && ch <= '9') {
                if (!snapshot.canGame || !snapshot.gameReady ||
                    !(snapshot.enabledKeys & (1U << (ch - '1')))) {
                    setNotice("Box unavailable: check AVR, enabled mask or maintenance");
                    return false;
                }
                action.type = Action::Press;
                action.index = ch - '0';
                return true;
            }
            if (ch == 'r') {
                if (!snapshot.canGame || !snapshot.gameReady) {
                    setNotice("Game unavailable");
                    return false;
                }
                action.type = Action::Reset;
                return true;
            }
        }
        if ((page_ == Led && !snapshot.canLeds) || (page_ == Pixel && !pixels(snapshot)))
            return false;
        if (page_ == Pixel)
            syncPixel(snapshot);
        if (ch == 'f' && (page_ == Led || page_ == Pixel)) {
            grid_ = !grid_;
            dirty_ = true;
            return false;
        }
        if (page_ == Led && snapshot.canLeds && (ch == 'w' || ch == 'r' || ch == 'o')) {
            action.type = Action::Cell;
            action.index = cell_;
            copy(action.key, sizeof(action.key), "cell");
            copy(action.value, sizeof(action.value),
                 ch == 'w'   ? "white"
                 : ch == 'r' ? "red"
                             : "off");
            return true;
        }
        if (page_ == Pixel && pixels(snapshot)) {
            if (ch == 'g') {
                group_ = !group_;
                syncPixel(snapshot);
                dirty_ = true;
                return false;
            }
            if (ch == 'w' || ch == 'r' || ch == 'o') {
                pixelColor_ = ch == 'w' ? 0 : ch == 'r' ? 1 : 2;
                dirty_ = true;
                return false;
            }
            if (grid_ && (ch == '+' || ch == '-'))
                return stepPixel(ch == '+' ? 1 : -1, snapshot, action);
        }
        if (ch == '\r' || ch == '\n') {
            crPending_ = ch == '\r';
            if (grid_ && page_ == Pixel && pixels(snapshot))
                return pixelAction(snapshot, action);
            if (grid_ && page_ == Led) {
                grid_ = false;
                dirty_ = true;
                return false;
            }
            const Row *row = currentRow(snapshot);
            if (row && row->editable && !row->pending && keySafe(row->key)) {
                if (!row->secret && !plain(row->value, 384)) {
                    setNotice("Value is not editable ASCII");
                    return false;
                }
                copy(editKey_, sizeof(editKey_), row->key);
                copy(editLabel_, sizeof(editLabel_), row->label);
                secret_ = row->secret;
                observed_ = rowHash(*row);
                copy(draft_, sizeof(draft_), secret_ ? "" : row->value);
                cursor_ = ps_text_length(draft_);
                editing_ = true;
                draftChanged_ = false;
                dirty_ = full_ = true;
                notice_[0] = 0;
            } else
                setNotice("Read only");
        }
        return false;
    }
    template <typename Write>
    bool render(const Snapshot &snapshot, Write write, bool force = false) {
        if (!active_)
            return false;
        if (columns_ != snapshot.columns || lines_ != snapshot.lines) {
            columns_ = snapshot.columns;
            lines_ = snapshot.lines;
            dirty_ = full_ = true;
        }
        if (!editing_) {
            const uint32_t next = hash(snapshot);
            if (next != lastHash_)
                dirty_ = true;
            lastHash_ = next;
        }
        if (!dirty_ && !force)
            return false;
        dirty_ = false;
        if (force) full_=true;
        if (editing_ && ansi_ && columns_ >= 40 && lines_ >= 16 && !full_ && !force) {
            editLine(write);
            return true;
        }
        if (PS_DEVICE_VIEW_TABLE)
            return renderTable(snapshot, write);
        Writer<Write> out(write, ansi_);
        if (ansi_)
            out.raw(full_ ? "\x1b[H\x1b[2J\x1b[1;31m" : "\x1b[H\x1b[1;31m");
        full_ = false;
        const size_t width = snapshot.columns < 40    ? 40
                             : snapshot.columns > 100 ? 100
                                                      : snapshot.columns;
        // UTF-8 wire bytes are char-based under both C++17 and C++23.
        out.raw(snapshot.unicode ? "\xe2\x95\xad\xe2\x94\x80 " : "+- ");
        out.safe(snapshot.title, width - 10);
        char titleMeasure[101];
        const size_t titleSize = terminalText(titleMeasure, width - 9, snapshot.title);
        for (size_t k = titleSize + 4; k < width - 1; ++k)
            out.raw(snapshot.unicode ? "\xe2\x94\x80" : "-");
        out.raw(snapshot.unicode ? "\xe2\x95\xae" : "+");
        if (ansi_) {
            out.raw("\x1b[0m");
        }
        out.raw("\r\n");
        if (snapshot.lines < 16 || snapshot.columns < 40) {
            out.raw("Resize terminal to 40 x 16\r\n");
            return true;
        }
        for (uint8_t i = 0; i < PageCount; ++i) {
            if ((i == Led && (!PS_DEVICE_VIEW_DIAGNOSTICS || !snapshot.canLeds)) ||
                (i == Pixel && !pixels(snapshot)))
                continue;
            if ((i == Game && (!PS_DEVICE_VIEW_DIAGNOSTICS || !snapshot.canGame)) ||
                (i == TerminalPage && !snapshot.terminalSettings))
                continue;
            if ((i == Cloud && !snapshot.cloudSettings) ||
                (i == Notifications && !snapshot.notificationSettings))
                continue;
            if (i == Clock && !snapshot.clockSettings)
                continue;
            if (width < 80 && i != page_)
                continue;
            if (ansi_)
                out.raw(i == page_ ? "\x1b[1;35m" : "\x1b[90m");
            out.raw(i == Device         ? " Device "
                    : i == Network      ? " Network "
                    : i == Led          ? " LEDs "
                    : i == Pixel        ? " Pixels "
                    : i == Game         ? " Game "
                    : i == TerminalPage ? " Term "
                    : i == Cloud        ? " Cloud "
                    : i == Clock        ? " Time "
                                        : " Notify ");
            if (ansi_)
                out.raw("\x1b[0m");
        }
        out.raw("\r\n");
        if (ansi_)
            out.raw("\x1b[31m");
        out.raw(snapshot.unicode ? "\xe2\x94\x9c" : "+");
        for (size_t i = 0; i < width - 3; ++i)
            out.raw(snapshot.unicode ? "\xe2\x94\x80" : "-");
        out.raw(snapshot.unicode ? "\xe2\x94\xa4" : "+");
        if (ansi_) {
            out.raw("\x1b[0m");
        }
        out.raw("\r\n");
        const bool allowed = page_ != Led && page_ != Pixel ? true
                             : page_ == Led                 ? snapshot.canLeds
                                                            : pixels(snapshot);
        if (!allowed) {
            out.raw("Unavailable\r\n");
        } else {
            const uint8_t count = rowCount(snapshot);
            if (selected_ >= count)
                selected_ = count ? count - 1 : 0;
            const uint8_t visible = page_ == Led          ? (snapshot.lines < 24 ? 1 : 4)
                                    : snapshot.lines < 24 ? 4
                                                          : 8;
            const uint8_t start = selected_ >= visible ? selected_ - visible + 1 : 0;
            for (uint8_t i = 0; i < visible; ++i) {
                const uint8_t at = start + i;
                if (ansi_)
                    out.raw("\x1b[31m");
                out.raw(snapshot.unicode ? "\xe2\x94\x82 " : "| ");
                if (at < count) {
                    const Row &row = snapshot.rows[at];
                    const bool selected = at == selected_ && !grid_;
                    if (ansi_) {
                        out.raw(selected ? "\x1b[1;35m" : "\x1b[0m");
                    }
                    out.raw(selected ? "> " : "  ");
                    const size_t labelWidth = width < 60 ? 12 : 22;
                    out.field(row.label, labelWidth);
                    if (ansi_)
                        out.raw("\x1b[90m");
                    out.raw(row.editable ? "  " : "- ");
                    if (ansi_)
                        out.raw(selected ? "\x1b[1;35m" : "\x1b[0m");
                    if (PS_DEVICE_VIEW_INLINE_EDIT && editing_ && selected)
                        editValue(out, width - labelWidth - 9);
                    else
                        out.field(row.secret ? "[hidden]" : row.value, width - labelWidth - 9);
                    if (ansi_)
                        out.raw("\x1b[0m");
                } else
                    out.field("", width - 5);
                if (ansi_)
                    out.raw("\x1b[31m");
                out.raw(snapshot.unicode ? " \xe2\x94\x82" : " |");
                if (ansi_)
                    out.raw("\x1b[0m");
                out.raw("\r\n");
            }
            if (ansi_)
                out.raw("\x1b[31m");
            out.raw(snapshot.unicode ? "\xe2\x95\xb0" : "+");
            for (size_t i = 0; i < width - 3; ++i)
                out.raw(snapshot.unicode ? "\xe2\x94\x80" : "-");
            out.raw(snapshot.unicode ? "\xe2\x95\xaf" : "+");
            if (ansi_)
                out.raw("\x1b[0m");
            out.raw("\r\n");
            renderDiagnostics(out,snapshot,unsigned(width));
        }
        if (ansi_)
            out.raw("\x1b[90m");
        if (editing_)
            out.raw("Enter save  Esc cancel\r\n");
        else if (width < 60)
            out.raw(snapshot.unicode ? "\xe2\x86\x91/\xe2\x86\x93 Tab Enter Esc\r\n"
                                     : "Up/Down Tab Enter Esc\r\n");
        else
            out.raw(snapshot.unicode ? "\xe2\x86\x91/\xe2\x86\x93 select  Tab pages  Enter edit  Esc back\r\n"
                                     : "Up/Down select  Tab pages  Enter edit  Esc back\r\n");
        if (ansi_)
            out.raw("\x1b[0m");
        out.safe(notice_, width - 1);
        out.raw("\r\n");
        if (PS_DEVICE_VIEW_INLINE_EDIT) {
            if (ansi_) {
                out.raw("\x1b[J");
                if (editing_)
                    editLine(write);
            }
            return true;
        }
        if (ansi_) {
            out.raw("\x1b[J");
            char position[24];
            ps_terminal_format(position, sizeof(position), "\x1b[%u;1H\x1b[2K", editorRow());
            out.raw(position);
        }
        if (editing_)
            editText(out);
        else
            out.raw(" ");
        return true;
    }

  private:
    template <typename Write> class Writer {
      public:
        explicit Writer(Write write, bool ansi = false) : write_(write), ansi_(ansi) {}
        void raw(const char *text) {
            while (*text) {
                const char *end = ansi_ ? ps_text_find(text, "\r\n") : nullptr;
                const size_t n = end ? size_t(end - text) : ps_text_length(text);
                if (n + (end ? 5 : 0) > remaining_)
                    return;
                write_(reinterpret_cast<const uint8_t *>(text), n);
                remaining_ -= n;
                if (!end)
                    return;
                write_(reinterpret_cast<const uint8_t *>("\x1b[K\r\n"), 5);
                remaining_ -= 5;
                text = end + 2;
            }
        }
        void safe(const char *text, size_t width) {
            char clean[241];
            terminalText(clean, width + 1 < sizeof(clean) ? width + 1 : sizeof(clean), text);
            raw(clean);
        }
        void field(const char *text, size_t width) {
            char clean[241];
            const size_t n =
                terminalText(clean, width + 1 < sizeof(clean) ? width + 1 : sizeof(clean), text);
            raw(clean);
            for (size_t i = n; i < width; ++i)
                raw(" ");
        }

      private:
        Write write_;
        size_t remaining_ = PS_DEVICE_VIEW_TABLE ? PS_DEVICE_VIEW_FRAME_BYTES : 4096;
        bool ansi_;
    };
    static void scrub(char *p, size_t n) {
        volatile char *out = p;
        while (n--)
            *out++ = 0;
    }
    static void copy(char *out, size_t size, const char *in) {
        size_t i = 0;
        if (!size)
            return;
        while (in && in[i] && i + 1 < size) {
            out[i] = in[i];
            ++i;
        }
        out[i] = 0;
    }
    static bool plain(const char *text, size_t maximum) {
        if (!text)
            return true;
        size_t n = 0;
        while (text[n] && n <= maximum) {
            if (text[n] < 32 || text[n] > 126)
                return false;
            ++n;
        }
        return n <= maximum;
    }
    static bool keySafe(const char *key) {
        if (!key || !key[0])
            return false;
        size_t i = 0;
        while (key[i] && i < 32) {
            const char c = key[i++];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '_' || c == '.' || c == '-'))
                return false;
        }
        return i < 32;
    }
    static uint8_t rowCount(const Snapshot &s) {
        return s.rows ? (s.rowCount > PS_DEVICE_VIEW_MAX_ROWS ? PS_DEVICE_VIEW_MAX_ROWS : s.rowCount) : 0;
    }
    const Row *currentRow(const Snapshot &s) const {
        return selected_ < rowCount(s) ? s.rows + selected_ : nullptr;
    }
    static bool pixels(const Snapshot &s) {
        if (!PS_DEVICE_VIEW_DIAGNOSTICS || !s.canPixels || !s.pixelCount || s.pixelCount > 480 ||
            !s.usedPixels || s.usedPixels > s.pixelCount || !s.groupCount || s.groupCount > 80 ||
            !s.groupCounts || !s.groupStarts)
            return false;
        uint32_t prefix = 0;
        for (uint8_t i = 0; i < s.groupCount; ++i) {
            if (!s.groupCounts[i] || s.groupStarts[i] != prefix)
                return false;
            prefix += s.groupCounts[i];
            if (prefix > s.pixelCount)
                return false;
        }
        return prefix == s.usedPixels;
    }
    // Call only after pixels(s) succeeds. Never synthesize six/twelve-pixel spans.
    static uint8_t groupAt(const Snapshot &s, uint16_t pixel) {
        for (uint8_t i = 0; i < s.groupCount; ++i)
            if (pixel < s.groupStarts[i] + s.groupCounts[i])
                return i;
        return s.groupCount - 1;
    }
    void syncPixel(const Snapshot &s) {
        if (pixel_ >= s.usedPixels)
            pixel_ = s.usedPixels - 1;
        if (group_)
            pixel_ = s.groupStarts[groupAt(s, pixel_)];
    }
    const char *color() const {
        return pixelColor_ == 0 ? "white" : pixelColor_ == 1 ? "red" : "off";
    }
    void clearDraft() {
        scrub(draft_, sizeof(draft_));
        scrub(editKey_, sizeof(editKey_));
        scrub(editLabel_, sizeof(editLabel_));
        cursor_ = 0;
        editing_ = secret_ = draftChanged_ = false;
        dialog_ = false;
    }
    void setNotice(const char *text) {
        terminalText(notice_, sizeof(notice_), text);
        dirty_ = true;
    }
    void nextPage(const Snapshot &s, int direction = 1) {
        for (uint8_t i = 0; i < PageCount; ++i) {
            page_ = static_cast<Page>((page_ + PageCount + direction) % PageCount);
            if ((page_ != Led || (PS_DEVICE_VIEW_DIAGNOSTICS && s.canLeds)) &&
                (page_ != Pixel || pixels(s)) &&
                (page_ != Game || (PS_DEVICE_VIEW_DIAGNOSTICS && s.canGame)) &&
                (page_ != TerminalPage || s.terminalSettings) &&
                (page_ != Cloud || s.cloudSettings) &&
                (page_ != Notifications || s.notificationSettings) &&
                (page_ != Clock || s.clockSettings))
                break;
        }
        selected_ = 0;
        grid_ = page_ == Led || page_ == Pixel;
        dirty_ = full_ = true;
        notice_[0] = 0;
    }
    bool pixelAction(const Snapshot &s, Action &a) {
        if (!pixels(s))
            return false;
        syncPixel(s);
        a.type = Action::Pixel;
        a.index = pixel_;
        a.group = group_;
        a.count = group_ ? s.groupCounts[groupAt(s, pixel_)] : 1;
        a.groupLayoutRevision = s.groupLayoutRevision;
        copy(a.key, sizeof(a.key), "pixel");
        copy(a.value, sizeof(a.value), color());
        return true;
    }
    bool stepPixel(int direction, const Snapshot &s, Action &a) {
        if (!pixels(s))
            return false;
        syncPixel(s);
        int next = static_cast<int>(pixel_) + direction;
        if (group_) {
            const int group = static_cast<int>(groupAt(s, pixel_)) + direction;
            if (group < 0 || group >= s.groupCount) {
                setNotice("Pixel boundary");
                return false;
            }
            next = s.groupStarts[group];
        }
        if (next < 0 || next >= s.usedPixels) {
            setNotice("Pixel boundary");
            return false;
        }
        pixel_ = static_cast<uint16_t>(next);
        dirty_ = true;
        return pixelAction(s, a);
    }
    bool arrow(uint8_t key, const Snapshot &s, Action &a) {
        if (editing_) {
            if (key == 'D' && cursor_)
                --cursor_;
            else if (key == 'C' && cursor_ < ps_text_length(draft_))
                ++cursor_;
            else if (key == 'H')
                cursor_ = 0;
            else if (key == 'F')
                cursor_ = ps_text_length(draft_);
            dirty_ = true;
            return false;
        }
        if (grid_ && page_ == Led && s.canLeds) {
            int next = cell_ + (key == 'D'   ? -1
                                : key == 'C' ? 1
                                : key == 'A' ? -10
                                : key == 'B' ? 10
                                             : 0);
            if (key == 'H')
                next = 1;
            else if (key == 'F')
                next = 80;
            else if (key == 'P')
                next = cell_ > 40 ? cell_ - 40 : 1;
            else if (key == 'N')
                next = cell_ < 40 ? cell_ + 40 : 80;
            if (next >= 1 && next <= 80) {
                cell_ = static_cast<uint8_t>(next);
            }
            dirty_ = true;
            return false;
        }
        if (grid_ && page_ == Pixel && pixels(s) &&
            (key == 'D' || key == 'C' || key == 'A' || key == 'B'))
            return stepPixel(key == 'D' || key == 'A' ? -1 : 1, s, a);
        if (grid_ && page_ == Pixel && pixels(s) &&
            (key == 'H' || key == 'F' || key == 'P' || key == 'N')) {
            syncPixel(s);
            const uint16_t old = pixel_;
            if (key == 'H')
                pixel_ = 0;
            else if (key == 'F')
                pixel_ = s.usedPixels - 1;
            else if (group_) {
                const int next = int(groupAt(s, pixel_)) + (key == 'P' ? -4 : 4);
                pixel_ = s.groupStarts[next < 0               ? 0
                                       : next >= s.groupCount ? s.groupCount - 1
                                                              : next];
            } else if (key == 'P')
                pixel_ = pixel_ > 8 ? pixel_ - 8 : 0;
            else
                pixel_ = pixel_ + 8 < s.usedPixels ? pixel_ + 8 : s.usedPixels - 1;
            syncPixel(s);
            dirty_ = true;
            return pixel_ != old && pixelAction(s, a);
        }
        const uint8_t count = rowCount(s);
        if (key == 'C' || key == 'D') {
            const Row *row=currentRow(s);
            if(PS_DEVICE_VIEW_WIDGETS&&row&&(row->kind==Level||row->kind==Choice))
                return adjust(key=='C'?'+':'-',s,a);
            nextPage(s, key == 'C' ? 1 : -1);
            return false;
        }
        if (key == 'A' && selected_)
            --selected_;
        else if (key == 'B' && selected_ + 1 < count)
            ++selected_;
        else if (key == 'H')
            selected_ = 0;
        else if (key == 'F')
            selected_ = count ? count - 1 : 0;
        else if (key == 'P')
            selected_ = selected_ > 4 ? selected_ - 4 : 0;
        else if (key == 'N')
            selected_ = count ? uint8_t(selected_ + 4 < count ? selected_ + 4 : count - 1) : 0;
        dirty_ = true;
        return false;
    }
    bool edit(uint8_t ch, const Snapshot &s, Action &action) {
        if (ch == '\r' || ch == '\n') {
            crPending_ = ch == '\r';
            const Row *row = currentRow(s);
            if ((page_ == Led && !s.canLeds) || (page_ == Pixel && !pixels(s)) ||
                (page_ == Game && !s.canGame) || !row || !row->editable || row->pending || !keySafe(row->key) ||
                !ps_text_equal(row->key, editKey_) || row->secret != secret_) {
                clearDraft();
                setNotice("Field changed; edit cancelled");
                full_ = true;
                return false;
            }
            if (!draftChanged_) {
                clearDraft();
                dirty_ = full_ = true;
                return false;
            }
            if (observed_ != rowHash(*row)) { setNotice("State changed; reopen editor"); full_=true; return false; }
            if (!validValue(*row, draft_)) { setNotice("Invalid value or outside range"); full_=true; return false; }
            action.type = Action::Set;
            copy(action.key, sizeof(action.key), editKey_);
            copy(action.value, sizeof(action.value), draft_);
            clearDraft();
            dirty_ = full_ = true;
            return true;
        }
        if (ch == 1)
            cursor_ = 0;
        else if (ch == 5)
            cursor_ = ps_text_length(draft_);
        else if (ch == 21) {
            if (cursor_) {
                ps_bytes_move(draft_, draft_ + cursor_, ps_text_length(draft_ + cursor_) + 1);
                cursor_ = 0;
                draftChanged_ = true;
            }
        } else if (ch == 23) {
            size_t target = cursor_;
            while (target && draft_[target - 1] == ' ')
                --target;
            while (target && draft_[target - 1] != ' ')
                --target;
            if (target < cursor_) {
                ps_bytes_move(draft_ + target, draft_ + cursor_,
                              ps_text_length(draft_ + cursor_) + 1);
                cursor_ = target;
                draftChanged_ = true;
            }
        } else if (ch == 8 || ch == 127) {
            if (cursor_) {
                --cursor_;
                ps_bytes_move(draft_ + cursor_, draft_ + cursor_ + 1,
                              ps_text_length(draft_ + cursor_));
                draftChanged_ = true;
            }
        } else if (ch == 4) {
            if (cursor_ < ps_text_length(draft_)) {
                ps_bytes_move(draft_ + cursor_, draft_ + cursor_ + 1,
                              ps_text_length(draft_ + cursor_));
                draftChanged_ = true;
            }
        } else if (ch >= 32 && ch < 127) {
            const size_t size = ps_text_length(draft_);
            if (size < 384) {
                ps_bytes_move(draft_ + cursor_ + 1, draft_ + cursor_, size - cursor_ + 1);
                draft_[cursor_++] = static_cast<char>(ch);
                draftChanged_ = true;
            }
        } else
            return false; // Form owns keys: 'r'/Tab cannot invoke global shortcuts.
        dirty_ = true;
        return false;
    }
    size_t editCapacity() const {
        if (PS_DEVICE_VIEW_TABLE) return editWidth_;
        const size_t width = columns_ < 40 ? 40 : columns_ > 100 ? 100 : columns_;
        return width - (width < 60 ? 12 : 22) - 9;
    }
    size_t editStart(size_t capacity) const {
        return cursor_ >= capacity ? cursor_ - capacity + 1 : 0;
    }
    template <typename Write> void editValue(Writer<Write> &out, size_t capacity) const {
        const size_t length = ps_text_length(draft_), start = editStart(capacity);
        char visible[241]{};
        for (size_t i = 0; i < capacity && start + i < length; ++i)
            visible[i] = secret_ ? '*' : draft_[start + i];
        if (ansi_)
            out.raw(PS_DEVICE_VIEW_TABLE ? ps_style_surface(theme_,trueColor_,1) : "\x1b[7m");
        out.field(visible, capacity);
        if (ansi_)
            out.raw(PS_DEVICE_VIEW_TABLE ? ps_style_base(theme_,trueColor_) : "\x1b[0m");
    }
    template <typename Write> void editText(Writer<Write> &out) const {
        char label[19];
        terminalText(label, sizeof(label), editLabel_);
        out.raw("Edit ");
        out.raw(label);
        out.raw(": ");
        const size_t width = columns_ < 40 ? 40 : columns_ > 100 ? 100 : columns_;
        const size_t capacity = width - ps_text_length(label) - 9;
        const size_t length = ps_text_length(draft_),
                     start = cursor_ >= capacity ? cursor_ - capacity + 1 : 0;
        char visible[101]{};
        for (size_t i = 0; i < capacity && start + i < length; ++i)
            visible[i] = secret_ ? '*' : draft_[start + i];
        out.raw(visible);
        if (ansi_) {
            char cursor[24];
            ps_terminal_format(cursor, sizeof(cursor), "\x1b[%u;%uH", editorRow(),
                               static_cast<unsigned>(8 + ps_text_length(label) + cursor_ - start));
            out.raw(cursor);
        }
    }
    unsigned editorRow() const {
        if (PS_DEVICE_VIEW_TABLE) return editRow_;
        if (PS_DEVICE_VIEW_INLINE_EDIT) {
            const unsigned visible = page_ == Led ? (lines_ < 24 ? 1 : 4)
                                      : lines_ < 24 ? 4 : 8;
            return 4 + (selected_ < visible ? selected_ : visible - 1);
        }
        return lines_ > 24 ? 23 : lines_ - 1;
    }
    template <typename Write> void editLine(Write write) const {
        Writer<Write> out(write, ansi_);
        if (PS_DEVICE_VIEW_TABLE) {
            char position[24];
            ps_terminal_format(position,sizeof(position),"\x1b[%u;%uH",editRow_,editColumn_);
            out.raw(position);editValue(out,editWidth_);
            ps_terminal_format(position,sizeof(position),"\x1b[%u;%uH",editRow_,editColumn_+unsigned(cursor_-editStart(editWidth_)));
            out.raw(position);out.raw("\x1b[?25h");return;
        }
        if (PS_DEVICE_VIEW_INLINE_EDIT) {
            const unsigned column = (columns_ < 60 ? 12 : 22) + 7;
            char position[24];
            ps_terminal_format(position, sizeof(position), "\x1b[%u;%uH", editorRow(), column);
            out.raw(position);
            const size_t capacity = editCapacity();
            editValue(out, capacity);
            ps_terminal_format(position, sizeof(position), "\x1b[%u;%uH", editorRow(),
                               column + static_cast<unsigned>(cursor_ - editStart(capacity)));
            out.raw(position);
            out.raw("\x1b[?25h");
            return;
        }
        if (ansi_) {
            char position[24];
            ps_terminal_format(position, sizeof(position), "\x1b[%u;1H\x1b[2K", editorRow());
            out.raw(position);
        } else
            out.raw("\r");
        editText(out);
    }
    static uint32_t hash(const Snapshot &s) {
        uint32_t value = 2166136261U;
        auto add = [&](const char *text) {
            for (size_t i = 0; text && text[i] && i < 384; ++i) {
                value ^= static_cast<uint8_t>(text[i]);
                value *= 16777619U;
            }
        };
        add(s.title);
        value ^= (s.terminalSettings?1U:0U)|(s.cloudSettings?2U:0U)|(s.notificationSettings?4U:0U)|(s.clockSettings?8U:0U);
        value *= 16777619U;
        value ^= s.canLeds ? 1 : 0;
        value ^= s.canPixels ? 2 : 0;
        value ^= s.pixelCount;
        value ^= s.enabledKeys;
        value *= 16777619U;
        value ^= s.winner;
        value *= 16777619U;
        value ^= s.columns;
        value *= 16777619U;
        value ^= s.lines;
        value *= 16777619U;
        value ^= (s.canGame ? 16 : 0) | (s.gameReady ? 32 : 0) | (s.unicode ? 64 : 0);
        value ^= s.usedPixels;
        value *= 16777619U;
        value ^= s.groupLayoutRevision;
        value *= 16777619U;
        if (pixels(s))
            for (uint8_t i = 0; i < s.groupCount; ++i) {
                value ^= s.groupCounts[i];
                value *= 16777619U;
                value ^= s.groupStarts[i];
                value *= 16777619U;
            }
        for (uint8_t i = 0; i < rowCount(s); ++i) {
            add(s.rows[i].label);
            add(s.rows[i].key);
            add(s.rows[i].category);
            if (!s.rows[i].secret)
                add(s.rows[i].value);
            value ^= (s.rows[i].editable ? 4 : 0) | (s.rows[i].secret ? 8 : 0);
            value ^= (unsigned(s.rows[i].kind)<<8)|(s.rows[i].pending?16:0);
            value ^= s.rows[i].meter?32:0;
            value *= 16777619U;
            value ^= s.rows[i].minimum;value *= 16777619U;
            value ^= s.rows[i].maximum;value *= 16777619U;
        }
        if (s.canLeds && s.cellStates)
            for (uint8_t i = 0; i < 80; ++i) {
                value ^= s.cellStates[i];
                value *= 16777619U;
            }
        return value;
    }
    Page page_ = Device;
#include "portable_device_table.hpp"
    Theme theme_ = Violet;
    bool trueColor_ = true, dialog_ = false;
    unsigned editRow_ = 6, editColumn_ = 30, editWidth_ = 40;
    uint32_t observed_ = 0;
    uint32_t painted_[PS_DEVICE_VIEW_TABLE ? PS_DEVICE_VIEW_MAX_ROWS : 1]{};
    unsigned paintedSelection_ = 255;
    unsigned paintedRows_ = 0;
    bool paintedGrid_ = false;
    uint8_t selected_ = 0, cell_ = 1, pixelColor_ = 1;
    uint16_t pixel_ = 0, columns_ = 80, lines_ = 24;
    size_t cursor_ = 0;
    bool active_ = false, ansi_ = true, dirty_ = true, full_ = true, editing_ = false,
         secret_ = false, draftChanged_ = false, grid_ = false, group_ = false, crPending_ = false;
    uint8_t escape_ = 0, escapeBytes_ = 0, attribute_ = 0;
    uint32_t lastHash_ = 0;
    char draft_[385]{}, editKey_[32]{}, editLabel_[49]{}, notice_[65]{};
};
} // namespace PortableShell

#endif
