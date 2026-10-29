#ifndef PORTABLE_STYLE_H
#define PORTABLE_STYLE_H
/* Shared desktop/device palette. No target detection, allocation or runtime. */
#ifndef PS_STYLE_THEMES
#define PS_STYLE_THEMES 1
#endif
static inline const char *ps_style_base(unsigned theme, int rgb) {
    if (!PS_STYLE_THEMES) return "\033[0m";
    if (!rgb) return "\033[0;38;5;255;48;5;234m";
    return theme==3 ? "\033[0;38;2;171;178;191;48;2;40;44;52m" :
           theme==2 ? "\033[0;38;2;220;228;238;48;2;12;18;29m" :
           theme==1 ? "\033[0;38;2;255;226;232;48;2;30;20;25m" :
                      "\033[0;38;2;241;231;255;48;2;23;19;31m";
}
static inline const char *ps_style_ink(unsigned theme, int rgb, int red_border, unsigned kind) {
    if (!PS_STYLE_THEMES) return "";
    static const char *const original[] = {"\033[38;5;245m", "\033[38;5;80m",    "\033[38;5;223m",
                                           "\033[38;5;203m", "\033[1;38;5;231m", "\033[38;5;151m",
                                           "\033[38;5;80m",  "\033[38;5;245m"};
    static const char *const violet[] = {"\033[38;2;162;150;181m", "\033[38;2;232;121;249m",
                                         "\033[38;2;255;201;157m", "\033[38;2;255;107;138m",
                                         "\033[38;2;241;231;255m", "\033[38;2;134;239;172m",
                                         "\033[38;2;190;153;255m", "\033[38;2;111;78;132m"};
    static const char *const red[] = {"\033[38;2;181;151;161m", "\033[38;2;255;107;138m",
                                      "\033[38;2;255;201;157m", "\033[38;2;255;87;113m",
                                      "\033[38;2;255;226;232m", "\033[38;2;134;239;172m",
                                      "\033[38;2;253;164;175m", "\033[38;2;125;68;87m"};
    static const char *const fallback[] = {"\033[38;5;246m", "\033[38;5;213m", "\033[38;5;223m",
                                           "\033[38;5;203m", "\033[38;5;255m", "\033[38;5;157m",
                                           "\033[38;5;183m", "\033[38;5;97m"};
    static const char *const one_dark[] = {"\033[38;2;120;129;149m", "\033[38;2;198;120;221m",
                                           "\033[38;2;229;192;123m", "\033[38;2;224;108;117m",
                                           "\033[38;2;220;223;228m", "\033[38;2;152;195;121m",
                                           "\033[38;2;198;120;221m", "\033[38;2;92;99;112m"};
    if (kind == 7 && theme != 2 && red_border) {
        return rgb ? "\033[38;2;255;95;115m" : "\033[38;5;203m";
    }
    return theme == 2                  ? original[kind % 8]
             : rgb ? (theme == 3   ? one_dark[kind % 8]
                                               : theme == 1 ? red[kind % 8]
                                                               : violet[kind % 8])
                                            : fallback[kind % 8];
}
static inline const char *ps_style_surface(unsigned theme, int rgb, unsigned active) {
    if (!PS_STYLE_THEMES) return "";
    if (theme == 2)
        return active == 1   ? "\033[48;5;23;38;5;159;1m"
                 : active == 2 ? "\033[48;5;236m"
                               : "\033[48;5;23;38;5;159m";
    else if (!rgb)
        return active == 1   ? "\033[48;5;171;38;5;16m"
                 : active == 2 ? "\033[48;5;53m"
                               : "\033[48;5;53;38;5;225m";
    else if (theme == 3)
        return active == 1   ? "\033[48;2;198;120;221m\033[38;2;33;37;43m"
                 : active == 2 ? "\033[48;2;62;68;81m"
                               : "\033[48;2;33;37;43m\033[38;2;171;178;191m";
    else if (theme == 1)
        return active == 1   ? "\033[48;2;251;113;133m\033[38;2;42;16;24m"
                 : active == 2 ? "\033[48;2;72;35;47m"
                               : "\033[48;2;55;29;39m\033[38;2;253;194;205m";
    else
        return active == 1   ? "\033[48;2;209;111;232m\033[38;2;30;16;40m"
                 : active == 2 ? "\033[48;2;62;37;78m"
                               : "\033[48;2;45;31;59m\033[38;2;225;194;247m";
}

#endif
