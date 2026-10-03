/* Private implementation included by pmwin.c and its host regression test.
 * Resource bytes are copied: unloading a resource cannot invalidate HACCEL.
 * Queue associations live in PM thread state; window associations are HWND
 * properties, which Windows removes when a window dies. */
#define PM_ACCEL_TABLES 128
struct PMAccelTable { DWORD handle, bytes; unsigned char *data; };
struct PMAccelCommand { WORD message, command; HWND target; };
static struct PMAccelTable pm_accels[PM_ACCEL_TABLES];
static CRITICAL_SECTION pm_accel_lock;
static DWORD pm_accel_serial = 0x41000000UL;
static const char pm_accel_property[] = "OS2HOST32.PM.Accelerator";
static HWND native_hwnd(O2HWND hwnd);
static int pm_is_own_process_window(HWND hwnd);

static void pm_accel_init(void) { InitializeCriticalSection(&pm_accel_lock); }
static void pm_accel_term(void)
{
    unsigned i;
    for (i=0;i<PM_ACCEL_TABLES;++i) if (pm_accels[i].data)
        HeapFree(GetProcessHeap(),0,pm_accels[i].data);
    DeleteCriticalSection(&pm_accel_lock);
}
static struct PMAccelTable *pm_accel_find(DWORD handle)
{
    unsigned i;
    for (i=0;i<PM_ACCEL_TABLES;++i)
        if (handle && pm_accels[i].handle==handle) return &pm_accels[i];
    return NULL;
}
static DWORD pm_accel_create(const void *source,DWORD bytes)
{
    unsigned i; DWORD needed,handle; unsigned char *data;
    if (!source || bytes<4) return pm_api_error(0x1208);
    needed=4UL+6UL*pm_rd16((const unsigned char *)source);
    if (needed>bytes) return pm_api_error(0x1208);
    data=(unsigned char *)HeapAlloc(GetProcessHeap(),0,needed);
    if (!data) return pm_api_error(0x203e);
    memcpy(data,source,needed);
    EnterCriticalSection(&pm_accel_lock);
    for(i=0;i<PM_ACCEL_TABLES;++i) if(!pm_accels[i].handle) break;
    if(i==PM_ACCEL_TABLES) {
        LeaveCriticalSection(&pm_accel_lock); HeapFree(GetProcessHeap(),0,data);
        return pm_api_error(0x203e);
    }
    do { handle=++pm_accel_serial; } while(!handle || pm_accel_find(handle));
    pm_accels[i].handle=handle; pm_accels[i].bytes=needed; pm_accels[i].data=data;
    LeaveCriticalSection(&pm_accel_lock); return handle;
}
/* 713, 723, 709: explicit table ownership and exact packed table copies. */
O2ULONG __cdecl WinCreateAccelTable(O2HAB hab,const void *table)
{
    (void)hab;
    if(!table) return pm_api_error(0x1208);
    return pm_accel_create(table,4UL+6UL*pm_rd16((const unsigned char *)table));
}
O2ULONG __cdecl WinDestroyAccelTable(O2ULONG handle)
{
    struct PMAccelTable *table; unsigned char *data=NULL;
    EnterCriticalSection(&pm_accel_lock); table=pm_accel_find(handle);
    if(table) { data=table->data; memset(table,0,sizeof(*table)); }
    LeaveCriticalSection(&pm_accel_lock);
    if(!data) return pm_api_error(0x101a); /* PMERR_INVALID_HACCEL */
    HeapFree(GetProcessHeap(),0,data); return 1;
}
O2ULONG __cdecl WinCopyAccelTable(O2ULONG handle,void *out,O2ULONG maximum)
{
    struct PMAccelTable *table; DWORD bytes=0;
    EnterCriticalSection(&pm_accel_lock); table=pm_accel_find(handle);
    if(table) {
        bytes=table->bytes;
        if(out) { if(bytes>maximum) bytes=maximum; memcpy(out,table->data,bytes); }
    }
    LeaveCriticalSection(&pm_accel_lock);
    if(!table) return pm_api_error(0x101a);
    return bytes;
}
/* 776 */
O2ULONG __cdecl WinLoadAccelTable(O2HAB hab,O2ULONG module,O2ULONG id)
{
    const struct PMCompatResource *r;
    (void)hab;
    if(id>65535) return pm_api_error(0x1208);
    r=pm_find_resource(module,O2_RT_ACCELTABLE,(WORD)id);
    if(!r) return pm_api_error(0x101a);
    return pm_accel_create(r->data,r->size);
}
/* 850: NULL HWND addresses this thread's queue; NULL HACCEL detaches. */
O2ULONG __cdecl WinSetAccelTable(O2HAB hab,O2ULONG handle,O2HWND window)
{
    struct PMCompatThreadState *s; HWND hwnd=NULL; BOOL ok=TRUE;
    (void)hab;
    if(window) {
        hwnd=native_hwnd(window);
        if(!pm_is_own_process_window(hwnd)) return pm_api_error(0x1001);
    }
    s=pm_thread_state(); if(!s) return pm_api_error(0x203e);
    EnterCriticalSection(&pm_accel_lock);
    if(handle && !pm_accel_find(handle)) ok=FALSE;
    else if(hwnd) {
        if(handle) ok=SetPropA(hwnd,pm_accel_property,(HANDLE)(ULONG_PTR)handle);
        else RemovePropA(hwnd,pm_accel_property);
    } else s->queue_accel=handle;
    LeaveCriticalSection(&pm_accel_lock);
    return ok?1:pm_api_error(0x101a);
}
/* 798 */
O2ULONG __cdecl WinQueryAccelTable(O2HAB hab,O2HWND window)
{
    struct PMCompatThreadState *s; DWORD handle; HWND hwnd=NULL;
    (void)hab;
    if(window) {
        hwnd=native_hwnd(window);
        if(!pm_is_own_process_window(hwnd)) return pm_api_error(0x1001);
        handle=(DWORD)(ULONG_PTR)GetPropA(hwnd,pm_accel_property);
    } else { s=pm_thread_state(); handle=s?s->queue_accel:0; }
    EnterCriticalSection(&pm_accel_lock);
    if(!pm_accel_find(handle)) handle=0;
    LeaveCriticalSection(&pm_accel_lock); return handle;
}
static int pm_accel_match_table(DWORD handle,WORD flags,WORD ch,WORD vk,
                                WORD scan,struct PMAccelCommand *command)
{
    struct PMAccelTable *table; DWORD pos; WORD fs,key; int found=0;
    if(flags&0x40) return 0; /* KC_KEYUP */
    EnterCriticalSection(&pm_accel_lock); table=pm_accel_find(handle);
    if(table) for(pos=4;pos<table->bytes;pos+=6) {
        fs=pm_rd16(table->data+pos); key=pm_rd16(table->data+pos+2);
        if((fs&0x38)!=(flags&0x38)) continue; /* shift/control/alt */
        if((fs&2) && (flags&2)) found=key==vk;
        else if((fs&4) && (flags&4)) found=key==scan;
        else if((fs&1) && (flags&1)) {
            WORD character=ch;
            if((flags&0x10) && character>=1 && character<=26) character=(WORD)('a'+character-1);
            if(key>='A' && key<='Z') key=(WORD)(key-'A'+'a');
            if(character>='A' && character<='Z') character=(WORD)(character-'A'+'a');
            found=key==character;
        }
        if(found) {
            command->command=pm_rd16(table->data+pos+4);
            command->message=(WORD)((fs&0x100)?0x21:(fs&0x200)?0x22:0x20);
            break;
        }
    }
    LeaveCriticalSection(&pm_accel_lock); return found;
}
static int pm_accel_match_window(HWND hwnd,WORD flags,WORD ch,WORD vk,WORD scan,
                                 struct PMAccelCommand *command)
{
    HWND current; unsigned depth=0; DWORD handle;
    handle=WinQueryAccelTable(1,0);
    if(pm_accel_match_table(handle,flags,ch,vk,scan,command)) { command->target=hwnd; return 1; }
    for(current=hwnd;current && depth<64;current=GetParent(current),++depth) {
        handle=(DWORD)(ULONG_PTR)GetPropA(current,pm_accel_property);
        if(pm_accel_match_table(handle,flags,ch,vk,scan,command)) { command->target=current; return 1; }
    }
    return 0;
}
