/* DeviceView members. Shared device presentation, independent of CPU/transport.
 * Typed controls and palettes follow portable_controls.inc; applications retain
 * validators, authorization, pending state and all physical side effects. */
static bool decimal(const char *s, uint32_t &n) {
    n=0;if(!s||!*s)return false;
    for(;*s;++s){if(*s<'0'||*s>'9'||n>(UINT32_MAX-unsigned(*s-'0'))/10U)return false;
        n=n*10U+unsigned(*s-'0');}return true;
}
static int toggle(const char *s) {
    if(ps_text_equal(s,"true")||ps_text_equal(s,"on")||ps_text_equal(s,"1"))return 1;
    if(ps_text_equal(s,"false")||ps_text_equal(s,"off")||ps_text_equal(s,"0"))return 0;
    return -1;
}
static void numberText(char *out,uint32_t n) {
    char reverse[10];unsigned count=0;
    do {reverse[count++]=char('0'+n%10U);n/=10U;}while(n);
    unsigned at=0;while(count)out[at++]=reverse[--count];out[at]=0;
}
static uint32_t rowHash(const Row &r) {
    uint32_t h=2166136261U;
    // Secret values are deliberately absent; providers must revalidate writes.
    for(const char *p=r.secret?"":r.value;p&&*p;++p){h^=uint8_t(*p);h*=16777619U;}
    return h;
}
static bool validValue(const Row &r,const char *value) {
    if(!PS_DEVICE_VIEW_WIDGETS)return true;
    if(r.kind==Toggle)return toggle(value)>=0;
    if(r.kind==Level){uint32_t n;return decimal(value,n)&&n>=r.minimum&&n<=r.maximum;}
    if(r.kind==Choice){for(unsigned i=0;r.choices&&i<r.choiceCount;++i)if(ps_text_equal(value,r.choices[i]))return true;return false;}
    return true;
}
bool adjust(uint8_t key,const Snapshot &s,Action &a) {
    const Row *r=currentRow(s);
    if(!r||!r->editable||r->secret||r->pending||!keySafe(r->key))return false;
    if(r->kind==Toggle&&key==' '){
        const int v=toggle(r->value);if(v<0){setNotice("State unknown");return false;}
        copy(a.value,sizeof(a.value),ps_text_equal(r->value,"on")||ps_text_equal(r->value,"off")?(v?"off":"on"):(v?"false":"true"));
    }else if(r->kind==Level&&(key=='+'||key=='-')){
        uint32_t n;if(!decimal(r->value,n)||n<r->minimum||n>r->maximum||!r->step)return false;
        const uint32_t step=r->step;
        if(key=='+')n=r->maximum-n<step?r->maximum:n+step;
        else n=n-r->minimum<step?r->minimum:n-step;
        numberText(a.value,n);
    }else if(r->kind==Choice&&(key==' '||key=='+'||key=='-')){
        unsigned at=0;while(r->choices&&at<r->choiceCount&&!ps_text_equal(r->value,r->choices[at]))++at;
        if(!r->choices||at==r->choiceCount)return false;
        at=key=='-'?(at?at-1:r->choiceCount-1):(at+1)%r->choiceCount;
        copy(a.value,sizeof(a.value),r->choices[at]);
    }else return false;
    if(ps_text_equal(a.value,r->value))return false;
    copy(a.key,sizeof(a.key),r->key);a.type=Action::Set;dirty_=true;return true;
}
bool startDialog(const Snapshot &s,Action &a) {
    if(editing_||grid_)return false;
    feed('\r',s,a);crPending_=false;
    if(editing_){dialog_=true;full_=dirty_=true;}return false;
}
static const char *pageName(unsigned p) {
    static const char *const names[]={"Device","Network","LEDs","Pixels","Game","Terminal","Cloud","Notifications","Clock"};
    return p<PageCount?names[p]:"";
}
static bool pageAvailable(unsigned p,const Snapshot &s) {
    return (p!=Led||(PS_DEVICE_VIEW_DIAGNOSTICS&&s.canLeds))&&(p!=Pixel||pixels(s))&&
        (p!=Game||(PS_DEVICE_VIEW_DIAGNOSTICS&&s.canGame))&&(p!=TerminalPage||s.terminalSettings)&&
        (p!=Cloud||s.cloudSettings)&&(p!=Notifications||s.notificationSettings)&&(p!=Clock||s.clockSettings);
}
template<class W> void base(Writer<W> &out) const {if(ansi_)out.raw(ps_style_base(theme_,trueColor_));}
template<class W> void ink(Writer<W> &out,unsigned role) const {if(ansi_)out.raw(ps_style_ink(theme_,trueColor_,true,role));}
template<class W> void edge(Writer<W> &out,const Snapshot &s,const char *unicode,const char *ascii) const {
    base(out);ink(out,7);out.raw(s.unicode?unicode:ascii);base(out);
}
template<class W> void rule(Writer<W> &out,const Snapshot &s,unsigned width,const char *left,const char *right,bool newline=true) const {
    base(out);ink(out,7);out.raw(s.unicode?left:"+");
    for(unsigned i=0;i<width-2;++i)out.raw(s.unicode?"\xe2\x94\x80":"-");
    out.raw(s.unicode?right:"+");base(out);if(newline)out.raw("\r\n");
}
template<class W> void gridRule(Writer<W> &out,const Snapshot &s,unsigned w,unsigned group,unsigned label,const char *junction,const char *left,const char *right) const {
    base(out);ink(out,7);out.raw(s.unicode?left:"+");
    for(unsigned i=1;i<w-1;++i){const bool join=i==label+3+(group?group+3:0)||(group&&i==group+3);
        out.raw(s.unicode?(join?junction:"\xe2\x94\x80"):(join?"+":"-"));}
    out.raw(s.unicode?right:"+");base(out);out.raw("\r\n");
}
template<class W> void value(Writer<W> &out,const Snapshot &s,const Row &r,unsigned width) const {
    if(r.secret){ink(out,0);out.field("[hidden]",width);return;}
    if(r.pending){ink(out,2);out.field("Pending readback",width);return;}
    if(PS_DEVICE_VIEW_WIDGETS&&r.kind==Toggle){
        const int on=toggle(r.value);ink(out,on<0?0:on?5:3);
        out.field(on<0?"Unknown":on?"[x] On":"[ ] Off",width);return;
    }
    uint32_t n;
    if(PS_DEVICE_VIEW_WIDGETS&&r.kind==Level&&r.meter&&width>=22&&decimal(r.value,n)&&r.maximum>r.minimum&&n>=r.minimum&&n<=r.maximum){
        // At most ten cells, no multiplication overflow for uint32 ranges.
        const unsigned bar=10;const uint32_t span=r.maximum-r.minimum,offset=n-r.minimum;
        const uint32_t quantum=span/bar+(span%bar!=0);
        const unsigned filled=n==r.maximum?bar:unsigned(offset/quantum);
        for(unsigned i=0;i<bar;++i){ink(out,i<filled?1:0);out.raw(s.unicode?(i<filled?"\xe2\x94\x81":"\xe2\x94\x80"):(i<filled?"#":"."));}
        ink(out,1);out.raw(" ");char text[32];numberText(text,n);out.field(text,width-bar-1);return;
    }
    ink(out,r.editable?1:4);out.field(r.value,width);
}
template<class Write> bool renderTable(const Snapshot &s,Write write) {
    Writer<Write> out(write,ansi_);
    const unsigned width=s.columns>240?240:s.columns;
    bool paintAll=full_||!ansi_;
    if(ansi_){base(out);out.raw(full_?"\x1b[H\x1b[2J":"\x1b[H");}
    full_=false;
    if(width<40||s.lines<16){out.raw("Resize terminal to 40 x 16\r\n");return true;}
    // Leave the physical terminal's last column free to avoid delayed autowrap.
    const unsigned w=width-1;
    base(out);ink(out,7);out.raw(s.unicode?"\xe2\x95\xad\xe2\x94\x80 ":"+- ");
    ink(out,4);char title[241];const unsigned titleSize=unsigned(terminalText(title,w-8,s.title));out.raw(title);
    ink(out,7);out.raw(" ");for(unsigned i=titleSize+4;i<w-1;++i)out.raw(s.unicode?"\xe2\x94\x80":"-");out.raw(s.unicode?"\xe2\x95\xae":"+");base(out);out.raw("\r\n");
    edge(out,s,"\xe2\x94\x82 ","| ");
    unsigned used=0,total=0;for(unsigned i=0;i<PageCount;++i)if(pageAvailable(i,s))total+=unsigned(ps_text_length(pageName(i)))+3;
    for(unsigned i=0;i<PageCount;++i){
        if(!pageAvailable(i,s)||(total>w-4&&i!=page_))continue;
        if(i==page_&&ansi_)out.raw(ps_style_surface(theme_,trueColor_,1));else ink(out,0);
        out.raw(i==page_?"[":" ");out.raw(pageName(i));out.raw(i==page_?"] ":"  ");base(out);
        used+=unsigned(ps_text_length(pageName(i)))+3;
    }
    if(total>w-4){char hint[40];ps_terminal_format(hint,sizeof(hint),"  %u/%u  Tab next",unsigned(page_)+1,unsigned(PageCount));
        const unsigned n=unsigned(ps_text_length(hint));if(used+n<w-4){ink(out,0);out.raw(hint);used+=n;}}
    out.field("",w-4-used);edge(out,s," \xe2\x94\x82"," |");out.raw("\r\n");
    const bool category=w>=95;
    const unsigned groupWidth=category?16:0,labelWidth=w<59?12:w<95?24:28;
    const unsigned valueWidth=w-7-labelWidth-(category?groupWidth+3:0);
    gridRule(out,s,w,groupWidth,labelWidth,"\xe2\x94\xac","\xe2\x94\x9c","\xe2\x94\xa4");
    auto cell=[&](const char *text,unsigned n){ink(out,0);out.field(text,n);edge(out,s," \xe2\x94\x82 "," | ");};
    edge(out,s,"\xe2\x94\x82 ","| ");if(category)cell("Category",groupWidth);cell("Setting",labelWidth);
    ink(out,0);out.field("Value",valueWidth);edge(out,s," \xe2\x94\x82"," |");out.raw("\r\n");
    gridRule(out,s,w,groupWidth,labelWidth,"\xe2\x94\xbc","\xe2\x94\x9c","\xe2\x94\xa4");
    const unsigned count=rowCount(s);if(selected_>=count)selected_=count?uint8_t(count-1):0;
    const unsigned diagnostics=page_==Led?(s.lines<24?5:9):page_==Game?4:page_==Pixel?3:0;
    unsigned visible=s.lines>11+diagnostics?s.lines-11-diagnostics:1;
    if(visible>PS_DEVICE_VIEW_MAX_ROWS)visible=PS_DEVICE_VIEW_MAX_ROWS;
    // Bound complete frames rather than silently dropping their final border.
    const unsigned budgetRows=(PS_DEVICE_VIEW_FRAME_BYTES > 8192 ? PS_DEVICE_VIEW_FRAME_BYTES-8192 : 1)/(w+1000U);
    if(visible>budgetRows)visible=budgetRows?budgetRows:1;
    if(visible>(count?count:1))visible=count?count:1;
    if(paintedRows_!=visible){paintAll=true;paintedRows_=visible;}
    const unsigned start=selected_>=visible?selected_-visible+1:0;
    for(unsigned i=0;i<visible;++i){
        const unsigned at=start+i;
        const Row *r=at<count?s.rows+at:nullptr;const bool selected=r&&at==selected_&&!grid_;
        Snapshot single;single.rows=r;single.rowCount=r?1:0;
        const uint32_t stamp=hash(single)^at;
        const bool selectionChanged=(selected_!=paintedSelection_)&&(at==selected_||at==paintedSelection_);
        if(!paintAll&&painted_[i]==stamp&&!selectionChanged&&paintedGrid_==grid_)continue;
        painted_[i]=stamp;
        if(ansi_){char pos[24];ps_terminal_format(pos,sizeof(pos),"\x1b[%u;1H",6+i);out.raw(pos);}
        edge(out,s,"\xe2\x94\x82 ","| ");
        if(category)cell(r?r->category:"",groupWidth);
        if(selected&&ansi_)out.raw(ps_style_surface(theme_,trueColor_,2));
        ink(out,selected?1:4);out.raw(selected?(s.unicode?"\xe2\x96\xb8 ":"> "):"  ");
        out.field(r?r->label:"",labelWidth-2);edge(out,s," \xe2\x94\x82 "," | ");
        if(selected&&editing_&&(!dialog_||!ansi_)){editValue(out,valueWidth);editRow_=6+i;editColumn_=labelWidth+6+(category?groupWidth+3:0);editWidth_=valueWidth;}
        else if(r)value(out,s,*r,valueWidth);else out.field("",valueWidth);
        edge(out,s," \xe2\x94\x82"," |");out.raw("\r\n");
    }
    paintedSelection_=selected_;paintedGrid_=grid_;
    if(ansi_){char pos[24];ps_terminal_format(pos,sizeof(pos),"\x1b[%u;1H",6+visible);out.raw(pos);}
    gridRule(out,s,w,groupWidth,labelWidth,"\xe2\x94\xb4","\xe2\x95\xb0","\xe2\x95\xaf");
    if(pageAvailable(page_,s))renderDiagnostics(out,s,w);
    base(out);ink(out,0);
    const Row *r=currentRow(s);
    const char *hint=editing_?"Enter apply  Esc cancel  ^U clear":
        grid_?(page_==Led?"f settings  w white  r red  o off":"Enter test  f settings  g group"):
        r&&r->pending?"Waiting for readback":r&&!r->editable?"Read only":
        PS_DEVICE_VIEW_WIDGETS&&r&&r->kind==Toggle?(PS_DEVICE_VIEW_DIALOGS?"Space toggle  Enter value  F2 dialog":"Space toggle  Enter value"):
        PS_DEVICE_VIEW_WIDGETS&&r&&r->kind==Level?(s.unicode?"\xe2\x86\x90/\xe2\x86\x92 adjust  Enter value":"+/- adjust  Enter value"):
        PS_DEVICE_VIEW_WIDGETS&&r&&r->kind==Choice?"Space next  +/- choose  Enter value":PS_DEVICE_VIEW_DIALOGS?"Enter edit  F2 dialog":"Enter edit";
    out.raw(hint);out.raw("\r\n"); // Trusted bounded literals, including Unicode key symbols.
    out.raw(s.unicode?"\xe2\x86\x91/\xe2\x86\x93 select  Tab pages  Esc back":"Up/Down select  Tab pages  Esc back");out.raw("\r\n");
    char status[64];ps_terminal_format(status,sizeof(status),"%u-%u / %u",count?start+1:0,count<start+visible?count:start+visible,count);
    ink(out,1);out.raw(status);out.raw("  ");ink(out,0);out.safe(notice_,w-unsigned(ps_text_length(status))-2);out.raw("\r\n");
    if(ansi_)out.raw("\x1b[J");
    if(PS_DEVICE_VIEW_DIALOGS&&dialog_&&editing_&&ansi_){
        const unsigned dw=w<74?w-4:70,left=(w-dw)/2+1,top=s.lines/2-3;
        auto position=[&](unsigned row){char b[24];ps_terminal_format(b,sizeof(b),"\x1b[%u;%uH",row,left);out.raw(b);};
        position(top);rule(out,s,dw,"\xe2\x95\xad","\xe2\x95\xae",false);
        position(top+1);edge(out,s,"\xe2\x94\x82 ","| ");ink(out,4);out.field(editLabel_,dw-4);edge(out,s," \xe2\x94\x82"," |");
        position(top+2);edge(out,s,"\xe2\x94\x82 ","| ");editValue(out,dw-4);edge(out,s," \xe2\x94\x82"," |");
        position(top+3);edge(out,s,"\xe2\x94\x82 ","| ");out.field("Enter apply   Esc cancel",dw-4);edge(out,s," \xe2\x94\x82"," |");
        position(top+4);rule(out,s,dw,"\xe2\x95\xb0","\xe2\x95\xaf",false);
        editRow_=top+2;editColumn_=left+2;editWidth_=dw-4;
    }
    if(editing_&&ansi_)editLine(write);
    return true;
}

template<class W> void renderDiagnostics(Writer<W> &out,const Snapshot &snapshot,unsigned width) {
            if (page_ == Led) {
                const uint8_t startLine = snapshot.lines < 24 ? ((cell_ - 1) / 10 < 4 ? 0 : 4) : 0;
                const uint8_t endLine = snapshot.lines < 24 ? startLine + 4 : 8;
                for (uint8_t line = startLine; line < endLine; ++line) {
                    for (uint8_t col = 0; col < 10; ++col) {
                        const uint8_t id = line * 10 + col + 1;
                        const bool picked = id == cell_;
                        if (ansi_) {
                            const uint8_t state =
                                snapshot.cellStates ? snapshot.cellStates[id - 1] : 255;
                            out.raw(state == 0 ? "\x1b[37m" : state == 1 ? "\x1b[31m" : "\x1b[90m");
                            if (picked && grid_)
                                out.raw("\x1b[7m");
                        }
                        char number[6];
                        ps_terminal_format(number, sizeof(number), width<50?(picked?">%02u":" %02u"):(picked ? "[%02u]" : " %02u "),
                                           id);
                        out.raw(number);
                        if (ansi_)
                            base(out);
                    }
                    out.raw("\r\n");
                }
                if (ansi_) {
                    ink(out,0);
                }
                out.raw(width<50?(snapshot.unicode?"\xe2\x86\x91/\xe2\x86\x93 \xe2\x86\x90/\xe2\x86\x92  w/r/o\r\n":"Up/Down Left/Right  w/r/o\r\n"):snapshot.unicode ? "\xe2\x86\x91/\xe2\x86\x93 \xe2\x86\x90/\xe2\x86\x92 "
                                           "move  w white  r red  o off\r\n"
                                         : "Up/Down Left/Right  w white r red o off\r\n");
                if (ansi_) {
                    base(out);
                }
            } else if (page_ == Game) {
                out.raw("\r\n");
                for (uint8_t id = 1; id <= 9; ++id) {
                    const bool enabled = snapshot.enabledKeys & (1U << (id - 1));
                    if (ansi_)
                        out.raw(!enabled                ? "\x1b[90m"
                                : snapshot.winner == id ? "\x1b[1;35m"
                                : snapshot.winner       ? "\x1b[31m"
                                                        : "\x1b[37m");
                    char box[8];
                    ps_terminal_format(box, sizeof(box), "[%u]%s", id, id == 9 ? "\r\n" : " ");
                    out.raw(box);
                    if (ansi_)
                        base(out);
                }
                char winner[64];
                ps_terminal_format(winner, sizeof(winner), "Winner %u  %s\r\n", snapshot.winner,
                                   snapshot.gameReady ? "Ready" : "AVR unavailable");
                out.raw(winner);
                out.raw("1-9 press box | r reset to white\r\n");
            } else if (page_ == Pixel) {
                syncPixel(snapshot);
                const uint8_t count = group_ ? snapshot.groupCounts[groupAt(snapshot, pixel_)] : 1;
                char line[112];
                ps_terminal_format(line, sizeof(line),
                                   width < 70 ? "Pixels %u..%u / %u (%u)\r\n"
                                              : "Physical pixel %u..%u / %u (%u capacity)\r\n",
                                   pixel_ + 1, pixel_ + count, snapshot.usedPixels,
                                   snapshot.pixelCount);
                out.raw(line);
                out.raw(group_ ? "Group  " : "Single pixel  ");
                out.raw(color());
                out.raw("\r\n");
                if (ansi_)
                    ink(out,0);
                out.raw(width<50?(snapshot.unicode?"\xe2\x86\x90/\xe2\x86\x92 +/-  g group  w/r/o\r\n":"Left/Right +/- g group w/r/o\r\n"):snapshot.unicode
                            ? "\xe2\x86\x90/\xe2\x86\x92 +/- step  g group  w/r/o color\r\n"
                            : "Left/Right +/- step  g group  w/r/o color\r\n");
                if (ansi_)
                    base(out);
            }
}
