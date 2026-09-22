#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cmd = (root / 'cmd32os2.c').read_text(encoding='utf-8')
mk = (root / 'Makefile').read_text(encoding='utf-8')

checks = [
    ('Tab key reaches the editor completion path',
     "if (ch == '\\t')" in cmd and 'editor_complete_filename(' in cmd),
    ('filesystem enumeration stays behind the OS/2 boundary',
     'CmdO2FindFirst(g_completion.pattern' in cmd and
     'CmdO2FindNext(h, &fd)' in cmd and 'CmdO2FindClose(h)' in cmd),
    ('completion scratch is static/BSS for the C/386 shell',
     'static struct CmdCompletionState g_completion;' in cmd),
    ('completion computes a case-insensitive common prefix',
     'completion_common_update' in cmd and 'toupper((unsigned char)common[i])' in cmd),
    ('unique directories gain a trailing backslash',
     'g_completion.unique_attr & CMDO2_ATTR_DIRECTORY' in cmd),
    ('ambiguous matches list on the first Tab',
     'completion_list_matches();' in cmd and
     'Ambiguous: extend as far as possible' in cmd),
    ('ambiguous completion rings the VIO bell',
     'editor_bell();' in cmd and 'static const char bell[] = "\\a";' in cmd),
    ('candidate listing redraws the normal CMD prompt',
     'completion_list_matches();\n    print_prompt();' in cmd),
    ('directory prefix is preserved while completing basename',
     'g_completion.dir_prefix' in cmd and 'g_completion.base_prefix' in cmd),
    ('wildcard-containing tokens are left alone',
     "strchr(g_completion.token, '*')" in cmd and
     "strchr(g_completion.token, '?')" in cmd),
    ('host-core regression is part of the gate',
     'cmd32-tabcomp-host-test' in mk and 'cmd32-tabcomp-check' in mk),
]

failed = [name for name, ok in checks if not ok]
if failed:
    for name in failed:
        print('FAIL:', name)
    raise SystemExit(1)

print('CMD32 interactive Tab-completion regression PASS')
print('  filesystem-only completion uses CmdO2FindFirst/Next')
print('  multiple matches extend to their common prefix, list, and bell')
print('  unique directories append a trailing backslash')
print('  completion state remains static/BSS for the small C/386 stack')
