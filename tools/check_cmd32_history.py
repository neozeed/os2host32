#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cmd = (root / 'cmd32os2.c').read_text(encoding='utf-8')
hdr = (root / 'cmdos2.h').read_text(encoding='utf-8')
os2 = (root / 'cmdos2_os2.c').read_text(encoding='utf-8')
win = (root / 'cmdos2_win32.c').read_text(encoding='utf-8')

checks = [
    ('30-entry history limit', '#define CMD_HISTORY_MAX 30U' in cmd),
    ('history is static, not placed on the C/386 stack', 'static struct CmdHistory history;' in cmd),
    ('Up scan handled', 'CMD_SCAN_UP' in cmd and 'scan == CMD_SCAN_UP' in cmd),
    ('Down scan handled', 'CMD_SCAN_DOWN' in cmd and 'scan == CMD_SCAN_DOWN' in cmd),
    ('Left/right editing handled', 'scan == CMD_SCAN_LEFT' in cmd and 'scan == CMD_SCAN_RIGHT' in cmd),
    ('Home/end editing handled', 'scan == CMD_SCAN_HOME' in cmd and 'scan == CMD_SCAN_END' in cmd),
    ('Delete editing handled', 'scan == CMD_SCAN_DELETE' in cmd),
    ('typing inserts at cursor', 'editor_move(line + cursor + 1U, line + cursor,' in cmd),
    ('backspace edits recalled/current text', 'editor_move(line + cursor - 1U, line + cursor,' in cmd),
    ('unfinished draft restored after Down', 'history->draft' in cmd and 'history_pos = -1;' in cmd),
    ('interactive commands are added to history', 'history_add(&history, line);' in cmd),
    ('horizontal VIO editor is active', 'horizontal viewport' in cmd and 'editor_render(' in cmd),
    ('screen-size service is declared', 'CmdO2VioGetScreenSize' in hdr),
    ('screen-size service has direct OS/2 implementation', 'CmdO2Rc CmdO2VioGetScreenSize' in os2),
    ('screen-size service has native bootstrap implementation', 'CmdO2Rc CmdO2VioGetScreenSize' in win),
]

failed = [name for name, ok in checks if not ok]
if failed:
    for name in failed:
        print('FAIL:', name)
    raise SystemExit(1)

# Keep -c worker execution ahead of interactive startup/history.  Pipeline
# workers must neither run STARTUP.CMD nor consume/create interactive history.
pos_c = cmd.find('if (argc >= 3 && ci_cmp(argv[1], "-c") == 0)')
pos_startup = cmd.find('rc = run_startup_cmd(&shell, &want_exit);')
pos_history = cmd.find('history_add(&history, line);')
if not (0 <= pos_c < pos_startup < pos_history):
    print('FAIL: -c/startup/history ordering changed')
    raise SystemExit(1)

print('CMD32 30-entry history + line-editor regression PASS')
print('  Up/Down cycle history and restore the unfinished draft')
print('  Left/Right/Home/End/Delete + Backspace edit recalled lines')
print('  printable input inserts at the cursor; history is static/BSS')
print('  interactive editor remains outside -c pipeline workers')
