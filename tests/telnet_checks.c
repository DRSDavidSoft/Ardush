#include "ardush_telnet.h"
#include <assert.h>
#include <string.h>
static unsigned char out[2048]; static size_t used;
static void capture(void *ctx, const unsigned char *p, size_t n) { (void)ctx; assert(used+n<sizeof out); memcpy(out+used,p,n);used+=n; }
int main(void) {
    ardush_telnet t; ardush_port port = {capture, 0, 0, 0};
    unsigned char resize[] = {255,251,31,255,250,31,0,100,0,30,255,240};
    unsigned char terminal[] = {255,251,24,255,250,24,0,'d','u','m','b',255,240};
    ardush_telnet_init(&t, port, 0, 0); assert(used > 21 && out[0] == 255 && out[1] == 251);
    for(size_t i=0;i<sizeof resize;++i) ardush_telnet_feed(&t,resize[i]);
    assert(t.shell.probe.column == 100 && t.shell.probe.row == 30);
    for(size_t i=0;i<sizeof terminal;++i) ardush_telnet_feed(&t,terminal[i]);
    assert(!t.shell.editor.ansi);
    used=0; { unsigned char iac=255; ardush_telnet_write(&t,&iac,1); }
    assert(used==2 && out[0]==255 && out[1]==255);
    used=0; { const char *p="echo framed\r\n"; while(*p)ardush_telnet_feed(&t,(unsigned char)*p++); }
    assert(t.shell.editor.command[0]==0 && t.shell.editor.count==1);
    return 0;
}
