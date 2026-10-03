/* Standard modal, single-selection FILEDLG adapter. Guest custom templates,
 * procedures and EA/multiple selection are explicitly rejected. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <cderr.h>
#include <stdint.h>
#include <string.h>
struct FileDlg {
    uint32_t size,flags,user;int32_t result,error;
    uint32_t title,ok,proc,type,types,drive,drives,module;
    char path[260];uint32_t paths,count;uint16_t id;int16_t x,y,ea;
};
typedef char filedlg_size_check[sizeof(struct FileDlg)==328?1:-1];
struct FileState { HWND dialog,owner;struct FileDlg *guest; };
static UINT_PTR CALLBACK file_hook(HWND dialog,UINT msg,WPARAM wp,LPARAM lp)
{
    struct FileState *s;(void)wp;
    if(msg!=WM_INITDIALOG) return 0;
    s=(struct FileState *)((OPENFILENAMEA *)lp)->lCustData;s->dialog=dialog;
    if(s->guest->ok) SetDlgItemTextA(dialog,IDOK,(const char *)(uintptr_t)s->guest->ok);
    if(s->guest->flags&1) {
        RECT parent,r;HWND owner=s->owner?s->owner:GetDesktopWindow();
        GetWindowRect(owner,&parent);GetWindowRect(dialog,&r);
        SetWindowPos(dialog,NULL,parent.left+(parent.right-parent.left-r.right+r.left)/2,
            parent.top+(parent.bottom-parent.top-r.bottom+r.top)/2,0,0,SWP_NOSIZE|SWP_NOZORDER);
    }
    return 0;
}
uint32_t __cdecl WinFileDlg(uint32_t parent,uint32_t owner,struct FileDlg *f)
{
    OPENFILENAMEA ofn;struct FileState state;char file[260],filter[560],initial[260];
    char *slash,*mask;size_t n;BOOL ok;DWORD err,pid;HWND wh;
    (void)parent;
    if(!f) return 0;
    if(f->size!=sizeof(*f)) return 0;
    f->result=0;f->error=0;f->count=0;f->paths=0;
    if((f->flags&~0xB01UL) || f->proc || f->type || f->types || f->drives ||
       ((f->flags&0x300)!=0x100 && (f->flags&0x300)!=0x200)) {
        f->error=3;return 0;
    }
    if(!memchr(f->path,0,sizeof(f->path))) { f->error=8;return 0; }
    wh=(owner>1)?(HWND)(uintptr_t)owner:NULL;
    if(wh && (!IsWindow(wh) || !GetWindowThreadProcessId(wh,&pid) || pid!=GetCurrentProcessId())) { f->error=3;return 0; }
    memset(&ofn,0,sizeof(ofn));memset(&state,0,sizeof(state));state.owner=wh;state.guest=f;
    strcpy(file,f->path);initial[0]=0;strcpy(filter,"Files");
    mask=file;slash=strrchr(file,'\\');if(!slash) slash=strrchr(file,'/');
    if(slash) {
        n=(size_t)(slash-file)+1;memcpy(initial,file,n);initial[n]=0;mask=slash+1;
    }
    if(strchr(mask,'*') || strchr(mask,'?')) {
        n=strlen(mask);memcpy(filter+6,mask,n+1);filter[7+n]=0;file[0]=0;
    } else {
        memcpy(filter,"All files\0*.*\0\0",15);
        if(slash) memmove(file,mask,strlen(mask)+1);
    }
    if(!initial[0] && f->drive) {
        const char *drive=(const char *)(uintptr_t)f->drive;
        if(strlen(drive)>=sizeof(initial)) { f->error=8;return 0; }strcpy(initial,drive);
    }
    ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=wh;ofn.lpstrFilter=filter;
    ofn.lpstrFile=file;ofn.nMaxFile=sizeof(file);ofn.lpstrTitle=(const char *)(uintptr_t)f->title;
    ofn.lpstrInitialDir=initial[0]?initial:NULL;ofn.lpfnHook=file_hook;ofn.lCustData=(LPARAM)&state;
    ofn.Flags=OFN_ENABLEHOOK|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY;
    if(f->flags&0x100) { ofn.Flags|=OFN_FILEMUSTEXIST;ok=GetOpenFileNameA(&ofn); }
    else { ofn.Flags|=OFN_OVERWRITEPROMPT;ok=GetSaveFileNameA(&ofn); }
    err=ok?0:CommDlgExtendedError();
    if(err) { f->error=(err==FNERR_BUFFERTOOSMALL)?8:12;return 0; }
    f->result=ok?1:2;
    if(ok) { strcpy(f->path,file);f->count=1; }
    /* OS/2 modal WinFileDlg returns a non-NULL (already dismissed) HWND on
     * both OK and Cancel. lReturn distinguishes those outcomes. */
    return (uint32_t)(uintptr_t)state.dialog;
}
