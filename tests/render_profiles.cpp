/* Host replay fixture. Identical simulated observations in both feature builds. */
#include "portable-shell/portable_device_view.hpp"
#include <cstdio>
extern "C" void ps_replay_reset(void);
extern "C" void ps_replay_feed(const unsigned char *,unsigned);
extern "C" unsigned ps_replay_svg(char *);
static char svg[400000];
static int save(const char *path){
    unsigned n=ps_replay_svg(svg);FILE *f=std::fopen(path,"wb");if(!f)return 1;
    int bad=std::fwrite(svg,1,n,f)!=n;return std::fclose(f)||bad;
}
#ifndef PS_PREVIEW_REDUCED
#define PS_PREVIEW_REDUCED 0
#endif
int main(int argc,char **argv){
    using V=PortableShell::DeviceView;
    if(argc!=(PS_PREVIEW_REDUCED?2:5))return 2;
    V view;V::Snapshot s;V::Action a;
    V::Row rows[]={ {"Output","output","on",true},{"Brightness","brightness","160",true},
        {"Effect","effect","breathe",true},{"Hostname","hostname","workbench",true},
        {"Transport","transport","Serial / byte stream",false},{"Receive rate","rx","24",false} };
    const char *effects[]={"steady","breathe","rainbow"};
    rows[0].kind=V::Toggle;
    rows[1].kind=V::Level;rows[1].maximum=255;rows[1].meter=true;
    rows[2].kind=V::Choice;rows[2].choices=effects;rows[2].choiceCount=3;
    for(unsigned i=0;i<6;++i)rows[i].category=i<3?"Lighting":"Connection";
    rows[5].unit="bytes/s";
    s.title="Device workspace";s.rows=rows;s.rowCount=6;s.columns=96;s.lines=24;s.unicode=true;
    s.terminalSettings=true;s.canLeds=true;
    uint8_t states[80];for(unsigned i=0;i<80;++i)states[i]=uint8_t(i%4);
    s.cellStates=states;s.cellStaggered=true;
    view.begin();
    auto render=[&](){ps_replay_reset();view.render(s,[](const uint8_t *p,size_t n){ps_replay_feed(p,unsigned(n));},true);};
    render();if(save(argv[1]))return 3;
    std::printf("DeviceView state: %u bytes; retained_rows=%d widgets=%d dialogs=%d matrix=%d\n",unsigned(sizeof(view)),PS_DEVICE_VIEW_RETAIN_ROWS,PS_DEVICE_VIEW_WIDGETS,PS_DEVICE_VIEW_DIALOGS,PS_DEVICE_VIEW_MATRIX);
#if !PS_PREVIEW_REDUCED
    view.setTabLayout(V::VerticalTabs);render();if(save(argv[2]))return 4;
    view.feed('\t',s,a);view.feed('\t',s,a);render();if(save(argv[3]))return 5;
    view.feed(27,s,a);view.feed('[',s,a);view.feed('Z',s,a);
    view.feed(27,s,a);view.feed('[',s,a);view.feed('Z',s,a);
    view.feed(27,s,a);view.feed('[',s,a);view.feed('B',s,a);
    view.feed('\r',s,a);view.feed(21,s,a);view.feed('2',s,a);view.feed('0',s,a);view.feed('0',s,a);
    render();if(save(argv[4]))return 6;
#endif
    return 0;
}
