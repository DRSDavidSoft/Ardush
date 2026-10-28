/* Optional hosted C adapter; stdio stays outside the byte-only application.
 * No fake hardware: LED observations report unavailable. */
#include "ardush.h"
#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static void output(void *ctx, const unsigned char *p, size_t n) { (void)ctx; fwrite(p, 1, n, stdout); }
int main(int argc, char **argv) {
    ardush s; int ch; ardush_port port = {output, 0, 0, 0};
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY); /* Shell already emits CRLF. */
#endif
    ardush_init(&s, port, 0, 0, 0);
    if (argc > 1) {
        /* Each program argument is one complete command, not rejoined tokens. */
        for (int i = 1; i < argc && s.mode != ARDUSH_CLOSED; ++i) {
            const unsigned char *p = (const unsigned char *)argv[i];
            while (*p) ardush_feed(&s, *p++);
            ardush_feed(&s, '\n');
        }
    } else {
        while (s.mode != ARDUSH_CLOSED && (ch = getchar()) != EOF) ardush_feed(&s, (unsigned char)ch);
        if (s.mode != ARDUSH_CLOSED && s.editor.command[0]) ardush_feed(&s, '\n');
    }
    return ferror(stdout) ? 1 : s.status;
}
