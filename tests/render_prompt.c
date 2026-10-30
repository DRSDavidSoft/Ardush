/* Prompt-only build: no C++ device view or simulated GPIO callbacks. */
#include "portable_console.h"
#include <stdio.h>
void ps_replay_reset(void);
void ps_replay_feed(const unsigned char *,unsigned);
unsigned ps_replay_svg(char *);
static char svg[400000];
static void write_bytes(void *context,const unsigned char *p,size_t n){(void)context;ps_replay_feed(p,(unsigned)n);}
int main(int argc,char **argv){
    portable_console s;portable_console_port port={write_bytes,0,0,0};
    const char *p="help\r";unsigned n;FILE *f;int bad;
    if(argc!=2)return 2;
    ps_replay_reset();portable_console_init(&s,port,0,0,1);
    while(*p)portable_console_feed(&s,(unsigned char)*p++);
    n=ps_replay_svg(svg);f=fopen(argv[1],"wb");if(!f)return 3;
    bad=fwrite(svg,1,n,f)!=n;
    printf("Prompt-only console state: %u bytes\n",(unsigned)sizeof(s));
    return fclose(f)||bad;
}
