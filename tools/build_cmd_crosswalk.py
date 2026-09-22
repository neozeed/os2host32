#!/usr/bin/env python3
"""Build the first OS/2 6.64/2.0 -> Dec-1991 NT CMD semantic crosswalk.

The OS/2 handler offsets come from the recovered 6.64 command table.  The NT
symbols come directly from the Dec-1991 PE COFF symbol/debug information.
"""
import csv
import os
import sys

OS2_COMMANDS = [
    ('DIR',      '2:396A', '_eDirectory', 'high'),
    ('ERASE',    '2:28E7', '_eDelete',    'high'),
    ('DEL',      '2:28E7', '_eDelete',    'high'),
    ('TYPE',     '2:3981', '_eType',      'high'),
    ('COPY',     '2:28D0', '_eCopy',      'high'),
    ('CD',       '2:3DCF', '_eChdir',     'high'),
    ('CHDIR',    '2:3DCF', '_eChdir',     'high'),
    ('RENAME',   '2:2F01', '_eRename',    'high'),
    ('REN',      '2:2F01', '_eRename',    'high'),
    ('ECHO',     '2:0703', '_eEcho',      'high'),
    ('SET',      '3:11B7', '_eSet',       'high'),
    ('PAUSE',    '2:105E', '_ePause',     'high'),
    ('DATE',     '3:16DC', '_eDate',      'high'),
    ('TIME',     '3:171F', '_eTime',      'high'),
    ('PROMPT',   '3:118E', '_ePrompt',    'high'),
    ('MD',       '2:3D1C', '_eMkdir',     'high'),
    ('MKDIR',    '2:3D1C', '_eMkdir',     'high'),
    ('RD',       '2:3ECD', '_eRmdir',     'high'),
    ('RMDIR',    '2:3ECD', '_eRmdir',     'high'),
    ('PATH',     '3:1088', '_ePath',      'high'),
    ('GOTO',     '2:0D26', '_eGoto',      'high'),
    ('SHIFT',    '2:1147', '_eShift',     'high'),
    ('CLS',      '3:1B2C', '_eCls',       'high'),
    ('CALL',     '2:12C4', '_eCall',      'high'),
    ('VERIFY',   '3:1BBB', '_eVerify',    'high'),
    ('VER',      '3:002E', '_eVersion',   'high'),
    ('VOL',      '3:0097', '_eVolume',    'high'),
    ('EXIT',     '3:1BA4', '_eExit',      'high'),
    ('SETLOCAL', '2:11AF', '_eSetlocal',  'high'),
    ('ENDLOCAL', '2:1224', '_eEndlocal',  'high'),
    ('CHCP',     '3:0000', '_eChcp',      'high'),
    ('START',    '3:0017', '_eStart',     'high'),
    ('DPATH',    '3:16C7', '_eAppend',    'medium'),
    ('KEYS',     '3:2E42', '_eKeys',      'high'),
    ('MOVE',     '2:3334', '_eMove',      'high'),
    ('FOR',      '2:07B5', '_eFor',       'high'),
    ('IF',       '2:0F39', '_eIf',        'high'),
    ('DETACH',   '3:0B16', '_eDetach',    'high'),
    ('REM',      'NO-OP',  '_ParseRem',   'medium'),
    ('EXTPROC',  '2:14C8', '_eExtproc',   'high'),
]

MODULE_PLAN = {
    'cmd.c':      ('keep',  'core dispatch, redirection, tree execution'),
    'cop.c':      ('keep',  'operators, pipes, parentheses; adapt process/pipe glue'),
    'ctools2.c':  ('mixed', 'I/O handles, messages, process wait/kill and helpers'),
    'cinit.c':    ('replace','NT/host initialization is personality-specific'),
    'ctools3.c':  ('mixed', 'paths, file/device/pipe helpers'),
    'cenv.c':     ('keep',  'PATH/PROMPT/SET/environment shell semantics'),
    'cbatch.c':   ('keep',  'batch language: FOR/GOTO/IF/CALL/SHIFT/SETLOCAL'),
    'cmem.c':     ('keep',  'shell allocation helpers'),
    'string.c':   ('keep',  'shell string helpers'),
    'cext.c':     ('adapt', 'external-command path; map to DosExecPgm/OS2SS'),
    'clex.c':     ('keep',  'lexer/tokenizer/prompt variable expansion'),
    'ckeys.c':    ('adapt', 'KEYS and keyboard behavior; map to KBDCALLS'),
    'ffirst.c':   ('adapt', 'file enumeration; map to OS/2 find APIs'),
    'cpwork.c':   ('mixed', 'COPY/DEL/RENAME/MOVE logic with OS-facing file wrappers'),
    'cclock.c':   ('adapt', 'DATE/TIME APIs'),
    'cother.c':   ('mixed', 'CLS/EXIT/VERIFY and small built-ins'),
    'cparse.c':   ('keep',  'parser and parse tree construction'),
    'ctools1.c':  ('mixed', 'filesystem and command-line helper logic'),
    'cfile.c':    ('mixed', 'COPY/DELETE/RENAME/MOVE implementation'),
    'cchcp.c':    ('adapt', 'code-page command; NLS/VIO/KBD mapping'),
    'start.c':    ('adapt', 'START/session semantics; SESMGR later'),
    'cpath.c':    ('mixed', 'directory stack/path commands; OS file APIs'),
    'cinfo.c':    ('mixed', 'DIR/TYPE/VER/VOL front-end helpers'),
    'dir.c':      ('mixed', 'directory listing logic; enumeration/free-space adapters'),
    'display.c':  ('adapt', 'display formatting output; route to VIO/console personality'),
    'console.c':  ('adapt', 'NT console glue; replace with VIO/KBD personality'),
    'uipriv.c':   ('replace','NT privilege-specific code, not OS/2 shell core'),
    'cpparse.c':  ('keep',  'command-line parser helper'),
    'csig.c':     ('adapt', 'signal/control handling'),
    'cdata.c':    ('keep',  'global shell data/tables'),
}


def load_functions(path):
    by_name = {}
    with open(path, encoding='utf-8', newline='') as f:
        for row in csv.DictReader(f, delimiter='\t'):
            by_name[row['name']] = row
    return by_name


def base_name(p):
    return p.replace('\\','/').split('/')[-1]


def main():
    if len(sys.argv) != 3:
        print('usage: build_cmd_crosswalk.py nt_functions.tsv outdir', file=sys.stderr)
        return 2
    functions_path, outdir = sys.argv[1], sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    funcs = load_functions(functions_path)

    with open(os.path.join(outdir, 'os2_nt_command_crosswalk.tsv'), 'w', encoding='utf-8', newline='') as f:
        f.write('command\tos2_664_handler\tnt_function\tnt_rva\tnt_source\tconfidence\tnotes\n')
        for command, os2addr, ntname, confidence in OS2_COMMANDS:
            row = funcs.get(ntname, {})
            rva = row.get('rva','')
            src = row.get('source_file','')
            note = 'direct semantic/name match'
            if confidence == 'medium':
                note = 'strong semantic candidate; verify control-flow/strings'
            f.write('%s\t%s\t%s\t%s\t%s\t%s\t%s\n' %
                    (command, os2addr, ntname, rva, src, confidence, note))

    # Module porting plan, decorated with observed contribution ranges where known.
    modules = {}
    modpath = os.path.join(outdir, 'nt_source_modules.tsv')
    if os.path.exists(modpath):
        with open(modpath, encoding='utf-8', newline='') as f:
            for row in csv.DictReader(f, delimiter='\t'):
                modules[base_name(row['source_file']).lower()] = row
    with open(os.path.join(outdir, 'nt_module_port_plan.tsv'), 'w', encoding='utf-8', newline='') as f:
        f.write('module\tstrategy\ttext_rva\ttext_size\trationale\n')
        for mod, (strategy, rationale) in MODULE_PLAN.items():
            row = modules.get(mod.lower(), {})
            f.write('%s\t%s\t%s\t%s\t%s\n' %
                    (mod, strategy, row.get('text_rva',''), row.get('text_size',''), rationale))

    # A handful of non-command anchors that matter to OS2SS.
    anchors = [
        ('core entry', '_main'), ('dispatch', '_Dispatch'), ('parser', '_Parser'),
        ('lexer', '_Lex'), ('external program', '_ExecPgm'),
        ('external command', '_ExtCom'), ('redirection', '_SetRedir'),
        ('pipe operator', '_ePipe'), ('environment setup', '_SetUpEnvironment'),
        ('directory engine', '_Dir'), ('copy engine', '_copy')
    ]
    with open(os.path.join(outdir, 'nt_semantic_anchors.tsv'), 'w', encoding='utf-8', newline='') as f:
        f.write('role\tfunction\trva\tsource_file\n')
        for role, name in anchors:
            row = funcs.get(name, {})
            f.write('%s\t%s\t%s\t%s\n' % (role, name, row.get('rva',''), row.get('source_file','')))

    print('crosswalk rows: %d' % len(OS2_COMMANDS))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
