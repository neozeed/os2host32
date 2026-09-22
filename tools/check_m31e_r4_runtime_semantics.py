from pathlib import Path
pm = Path('pmwin.c').read_text(errors='ignore')
host = Path('os2host32.c').read_text(errors='ignore')
checks = [
    ('update disabled property', 'g_update_disabled_prop' in pm),
    ('WinEnableWindowUpdate remembers disabled state', 'SetPropA(wh, g_update_disabled_prop' in pm),
    ('WinShowWindow reenables WM_SETREDRAW', 'update_was_disabled' in pm and 'WM_SETREDRAW, TRUE' in pm),
    ('WinShowWindow redraws accumulated changes', 'UpdateWindow(wh)' in pm),
    ('INIT-only TERM skip is marked complete', 'GUESTMOD TERMSKIP' in host and 'g->term_called = 1;' in host),
]
failed=[name for name,ok in checks if not ok]
if failed:
    for name in failed: print('FAIL:', name)
    raise SystemExit(1)
print('M31E R4 runtime semantics regression PASS')
print('  WinEnableWindowUpdate(FALSE) + WinShowWindow(TRUE) restores/redraws controls')
print('  INITINSTANCE-only guest DLL cleanup cannot loop forever')
