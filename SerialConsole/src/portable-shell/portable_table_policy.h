#ifndef PORTABLE_TABLE_POLICY_H
#define PORTABLE_TABLE_POLICY_H
/* Shared selection policy; NULL override means automatic, "" means hidden. */
static inline const char *ps_table_marker(const char *override_text,int unicode,int categories,int collapsible,int chevrons) {
    if(override_text)return override_text;
    return !categories||!collapsible||!chevrons?(unicode?"\xe2\x96\xb8 ":"> "):"";
}
#endif
