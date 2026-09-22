#!/usr/bin/env python3
from pathlib import Path
import sys

s = Path('pmwin.c').read_text(errors='replace')
need = [
    'if (_stricmp(cls, "BUTTON") == 0 && code == BN_CLICKED)',
    'call_dialog_guest(hwnd, dlg, O2_WM_COMMAND',
    'call_dialog_guest(hwnd, dlg, O2_WM_CONTROL',
    'pm_trace("dialog control notify"',
    'pm_trace("dialog button command"',
]
missing = [x for x in need if x not in s]
if missing:
    for x in missing:
        print('M31F R6 dialog regression: missing:', x)
    sys.exit(1)

# Guard against restoring the R5 bug: a blanket non-listbox fallback to
# WM_COMMAND for all child-control notifications.
old = '''if (lParam != 0 && (code == LBN_SELCHANGE || code == LBN_DBLCLK))'''
if old in s:
    print('M31F R6 dialog regression: old listbox-only WM_CONTROL split restored')
    sys.exit(1)

print('M31F R6 dialog notification check: PASS')
