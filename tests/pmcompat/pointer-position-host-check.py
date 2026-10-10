#!/usr/bin/env python3
"""Compile the actual PMWIN pointer exports against deterministic Win32 calls."""
from pathlib import Path
import os, re, shlex, subprocess, tempfile
root = Path(__file__).resolve().parents[2]
source = (root / 'dlls/pmwin/pmwin.c').read_text()
def body(name):
    match = re.search(r'O2ULONG __cdecl '+name+r'\([^)]*\)\s*\{', source)
    assert match, name
    i, depth = match.end(), 1
    while depth:
        if source[i] == '{': depth += 1
        elif source[i] == '}': depth -= 1
        i += 1
    return source[match.start():i]
code = r'''
#include <assert.h>
#include <stdio.h>
#define __cdecl
#define SM_CYSCREEN 1
typedef unsigned long O2ULONG;
typedef unsigned long O2HWND;
typedef long O2LONG;
typedef struct { O2LONG x,y; } O2POINTL;
typedef struct { long x,y; } POINT;
static POINT cursor;
static int failure;
static int GetSystemMetrics(int metric) { assert(metric==1);return 1080; }
static int SetCursorPos(int x,int y) { if(failure)return 0;cursor.x=x;cursor.y=y;return 1; }
static int GetCursorPos(POINT *out) { if(failure)return 0;*out=cursor;return 1; }
'''+body('WinSetPointerPos')+'\n'+body('WinQueryPointerPos')+r'''
int main(void) {
    O2POINTL point;
    assert(WinSetPointerPos(1,0,0) && cursor.x==0 && cursor.y==1079);
    assert(WinQueryPointerPos(1,&point) && point.x==0 && point.y==0);
    assert(WinSetPointerPos(1,1919,1079) && cursor.x==1919 && cursor.y==0);
    assert(WinQueryPointerPos(1,&point) && point.x==1919 && point.y==1079);
    assert(WinSetPointerPos(1,-7,540) && cursor.x==-7 && cursor.y==539);
    assert(WinQueryPointerPos(1,&point) && point.x==-7 && point.y==540);
    failure=1;
    assert(!WinSetPointerPos(1,100,100));
    assert(!WinQueryPointerPos(1,&point));
    assert(!WinQueryPointerPos(1,0));
    puts("PMWIN pointer pixel origin, signed X, round-trip and host failures: PASS");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
    c = Path(temp)/'pointer.c';exe = Path(temp)/'pointer-check'
    c.write_text(code)
    subprocess.run(shlex.split(os.environ.get('HOSTCC','cc'))+['-std=c89','-Wall','-Wextra','-Werror',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
