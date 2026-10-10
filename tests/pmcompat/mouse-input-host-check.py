#!/usr/bin/env python3
"""Compile actual PMWIN mouse callback branches and key-state exports with mocks."""
from pathlib import Path
import os, re, shlex, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'dlls/pmwin/pmwin.c').read_text()
def function(name):
    m=re.search(r'^(?:static )?[^\n]+\b'+name+r'\([^\n]*\)\s*\{',source,re.M)
    assert m,name
    i=m.end();depth=1
    while depth:
        if source[i]=='{':depth+=1
        elif source[i]=='}':depth-=1
        i+=1
    return source[m.start():i]
blocks=re.findall(r'    case WM_MOUSEMOVE:\n.*?(?=    case WM_VSCROLL:)',source,re.S)
assert len(blocks)==2
constants='\n'.join(re.findall(r'^#define O2_(?:WM_MOUSEMOVE|WM_BUTTON[123](?:DOWN|UP|DBLCLK)|VK_\w+)\s+[^\n]+',source,re.M))
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define __cdecl
typedef uint32_t DWORD,O2MPARAM,O2ULONG;
typedef uint16_t O2USHORT;
typedef uintptr_t HWND,O2HWND;
typedef uint32_t UINT,WPARAM;
typedef int32_t LPARAM,LONG;
typedef struct { LONG left,top,right,bottom; } RECT;
#define LOWORD(v) ((uint16_t)(uint32_t)(v))
#define HIWORD(v) ((uint16_t)((uint32_t)(v)>>16))
#define WM_MOUSEMOVE 0x200
#define WM_LBUTTONDOWN 0x201
#define WM_LBUTTONUP 0x202
#define WM_LBUTTONDBLCLK 0x203
#define WM_RBUTTONDOWN 0x204
#define WM_RBUTTONUP 0x205
#define WM_RBUTTONDBLCLK 0x206
#define WM_MBUTTONDOWN 0x207
#define WM_MBUTTONUP 0x208
#define WM_MBUTTONDBLCLK 0x209
#define VK_LBUTTON 1
#define VK_RBUTTON 2
#define VK_CANCEL 3
#define VK_MBUTTON 4
#define VK_BACK 8
#define VK_TAB 9
#define VK_RETURN 13
#define VK_SHIFT 16
#define VK_CONTROL 17
#define VK_MENU 18
#define VK_PAUSE 19
#define VK_CAPITAL 20
#define VK_ESCAPE 27
#define VK_SPACE 32
#define VK_PRIOR 33
#define VK_NEXT 34
#define VK_END 35
#define VK_HOME 36
#define VK_LEFT 37
#define VK_UP 38
#define VK_RIGHT 39
#define VK_DOWN 40
#define VK_SNAPSHOT 44
#define VK_INSERT 45
#define VK_DELETE 46
#define VK_F1 112
#define VK_F24 135
#define VK_NUMLOCK 144
#define VK_SCROLL 145
#define VK_LSHIFT 160
#define VK_RSHIFT 161
#define VK_LCONTROL 162
#define VK_RCONTROL 163
#define VK_LMENU 164
#define VK_RMENU 165
static short state[256];
static unsigned key_calls,last_key,callbacks;
static O2USHORT got_msg;
static O2MPARAM got_mp1,got_mp2;
static short GetKeyState(int vk) { key_calls++;last_key=(unsigned)vk;return state[vk]; }
static int GetClientRect(HWND hwnd,RECT *r) { assert(hwnd==42);r->left=r->top=0;r->right=560;r->bottom=360;return 1; }
static int call_guest(HWND hwnd,O2USHORT msg,O2MPARAM mp1,O2MPARAM mp2) {
    assert(hwnd==42);callbacks++;got_msg=msg;got_mp1=mp1;got_mp2=mp2;return 0;
}
static int call_dialog_guest(HWND hwnd,void *dlg,O2USHORT msg,O2MPARAM mp1,O2MPARAM mp2) {
    assert(dlg);return call_guest(hwnd,msg,mp1,mp2);
}
'''+constants+'\n'+function('pm_mouse_message')+'\n'+function('pm_mouse_point')+'\n'+function('os2_vk_from_win')+'\n'+function('WinGetKeyState')+r'''
static int dialog_mouse(HWND hwnd,UINT msg,LPARAM lParam,void *dlg) {
    switch(msg) {
'''+blocks[0]+r'''
    default: break;
    }
    return -1;
}
static int client_mouse(HWND hwnd,UINT msg,LPARAM lParam) {
    switch(msg) {
'''+blocks[1]+r'''
    default: break;
    }
    return -1;
}
int main(void) {
    unsigned i,before;
    LPARAM point=(LPARAM)((uint32_t)(uint16_t)-7|((uint32_t)(uint16_t)130<<16));
    /* All ten translated messages, through BOTH actual callback branches. */
    for(i=0;i<10;i++) {
        before=callbacks;assert(!client_mouse(42,WM_MOUSEMOVE+i,point));
        assert(callbacks==before+1 && got_msg==0x70+i);
        assert((int16_t)LOWORD(got_mp1)==-7 && (int16_t)HIWORD(got_mp1)==230 && got_mp2==0);
        before=callbacks;assert(!dialog_mouse(42,WM_MOUSEMOVE+i,point,(void *)1));
        assert(callbacks==before+1 && got_msg==0x70+i);
        assert((int16_t)LOWORD(got_mp1)==-7 && (int16_t)HIWORD(got_mp1)==230 && got_mp2==0);
    }
    before=callbacks;
    assert(client_mouse(42,0x999,point)==-1 && dialog_mouse(42,0x999,point,(void *)1)==-1);
    assert(dialog_mouse(42,WM_RBUTTONDOWN,point,NULL)==-1 && callbacks==before);
    assert(pm_mouse_message(0x999)==0);
    /* Captured coordinates can lie outside the client. */
    point=(LPARAM)((uint32_t)600|((uint32_t)(uint16_t)-12<<16));
    assert(!client_mouse(42,WM_RBUTTONUP,point));
    assert(LOWORD(got_mp1)==600 && HIWORD(got_mp1)==372);
    for(i=1;i<=3;i++) {
        unsigned vk=i==3?VK_MBUTTON:i;
        state[VK_CANCEL]=(short)0x8000; /* must not alias OS/2 middle button */
        state[vk]=0;before=key_calls;
        assert(WinGetKeyState(1,(LONG)i)==0 && key_calls==before+1 && last_key==vk);
        state[vk]=(short)0x8001;before=key_calls;
        assert((uint16_t)WinGetKeyState(1,(LONG)i)==0x8001 && key_calls==before+1 && last_key==vk);
        state[vk]=0;
    }
    state[VK_F1]=(short)0x8000;assert((uint16_t)WinGetKeyState(1,O2_VK_F1)==0x8000);
    state[VK_RETURN]=(short)0x8000;assert((uint16_t)WinGetKeyState(1,O2_VK_NEWLINE)==0x8000);
    assert(!WinGetKeyState(1,0x7777));
    puts("PMWIN mouse PASS: client/dialog move + 3 buttons down/up/double-click, signed coordinates, button polling");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
    c=Path(temp)/'mouse.c';exe=Path(temp)/'mouse-check';c.write_text(code)
    subprocess.run(shlex.split(os.environ.get('HOSTCC','cc'))+['-std=c89','-Wall','-Wextra','-Werror',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
