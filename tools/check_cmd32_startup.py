#!/usr/bin/env python3
from pathlib import Path

src = Path("cmd32os2.c").read_text(encoding="utf-8")

required = [
    '#define CMD32_STARTUP_CMD "C:\\\\OS2\\\\ENV\\\\STARTUP.CMD"',
    'if (!path_is_file(CMD32_STARTUP_CMD))',
    'return run_batch_file(s, CMD32_STARTUP_CMD, "", want_exit);',
    'rc = run_startup_cmd(&shell, &want_exit);',
]
for text in required:
    if text not in src:
        raise SystemExit(f"CMD32 startup regression FAIL: missing {text!r}")

# The internal pipeline worker path uses CMD32OS2 -c.  It must return before
# the interactive startup hook so STARTUP.CMD is not rerun for every pipe side.
pos_c = src.index('if (argc >= 3 && ci_cmp(argv[1], "-c") == 0)')
pos_startup = src.index('rc = run_startup_cmd(&shell, &want_exit);')
pos_prompt_loop = src.index('while (!want_exit) {', pos_startup)
if not (pos_c < pos_startup < pos_prompt_loop):
    raise SystemExit("CMD32 startup regression FAIL: startup ordering changed")

print("CMD32 STARTUP.CMD regression PASS")
print(r"  interactive shell probes C:\OS2\ENV\STARTUP.CMD")
print("  missing startup file is ignored")
print("  startup runs through the existing batch engine before the first prompt")
print("  -c pipeline workers return before the startup hook")
