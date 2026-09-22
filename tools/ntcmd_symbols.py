#!/usr/bin/env python3
"""Extract source contributions and function symbols from the Dec-1991 NT CMD PE.

Uses GNU objdump output because this transitional PE carries COFF FILE records
and public/static symbols.  The resulting TSV files are analysis artefacts only;
the runtime remains C89-oriented.
"""
import os
import re
import subprocess
import sys

SYM_RE = re.compile(r'^\[\s*\d+\]\(sec\s+(-?\d+)\)\(fl\s+0x[0-9a-fA-F]+\)\(ty\s+\d+\)\(scl\s+(\d+)\)\s+\(nx\s+\d+\)\s+0x([0-9a-fA-F]+)\s+(.+)$')
AUX_RE = re.compile(r'^AUX scnlen 0x([0-9a-fA-F]+)')


def run_objdump(path):
    return subprocess.check_output(['objdump', '-t', path], text=True, errors='replace').splitlines()


def parse(path):
    lines = run_objdump(path)
    current_file = None
    modules = []
    funcs = []
    pending_text = None

    i = 0
    while i < len(lines):
        line = lines[i]
        m = SYM_RE.match(line)
        if not m:
            i += 1
            continue
        sec = int(m.group(1))
        scl = int(m.group(2))
        value = int(m.group(3), 16)
        name = m.group(4).strip()

        if sec == -2 and scl == 103:
            current_file = name
            pending_text = None
            i += 1
            continue

        if sec == 1 and name == '.text' and current_file:
            length = 0
            if i + 1 < len(lines):
                a = AUX_RE.match(lines[i+1])
                if a:
                    length = int(a.group(1), 16)
            modules.append((current_file, value, length, value + length))
            pending_text = (value, length)
            i += 1
            continue

        if sec == 1 and name != '.text' and not name.startswith('.'): 
            # COFF storage class 2 is external/public, 3 is static.  Both are
            # useful here because the CMD binary retains many internal names.
            if scl in (2, 3):
                funcs.append((value, name, scl))
        i += 1

    # Map each function to the narrowest text contribution containing its RVA.
    rows = []
    for addr, name, scl in sorted(funcs):
        owners = [m for m in modules if m[1] <= addr < m[3] and m[2] > 0]
        owner = min(owners, key=lambda x: x[2]) if owners else None
        rows.append((addr, name, scl, owner[0] if owner else ''))
    return modules, rows


def main():
    if len(sys.argv) != 3:
        print('usage: ntcmd_symbols.py ntcmd.exe outdir', file=sys.stderr)
        return 2
    src, outdir = sys.argv[1], sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    modules, funcs = parse(src)

    with open(os.path.join(outdir, 'nt_source_modules.tsv'), 'w', encoding='utf-8', newline='') as f:
        f.write('source_file\ttext_rva\ttext_size\ttext_end\n')
        for name, start, length, end in modules:
            f.write('%s\t0x%08X\t0x%X\t0x%08X\n' % (name, start, length, end))

    with open(os.path.join(outdir, 'nt_functions.tsv'), 'w', encoding='utf-8', newline='') as f:
        f.write('rva\tname\tstorage_class\tsource_file\n')
        for addr, name, scl, owner in funcs:
            f.write('0x%08X\t%s\t%d\t%s\n' % (addr, name, scl, owner))

    cmdfuncs = [(a,n,s,o) for a,n,s,o in funcs if '\\windows\\cmd\\' in o.lower()]
    groups = {}
    for addr, name, scl, owner in cmdfuncs:
        groups.setdefault(owner, []).append((addr, name, scl))
    with open(os.path.join(outdir, 'NT_CMD_FUNCTION_INDEX.md'), 'w', encoding='utf-8') as f:
        f.write('# Dec-1991 NT CMD function index\n\n')
        f.write('Recovered from PE COFF FILE records and named symbols.\n\n')
        for owner in sorted(groups, key=lambda x: min(v[0] for v in groups[x])):
            leaf = owner.replace('\\', '/').split('/')[-1]
            f.write('## `%s`\n\n' % leaf)
            f.write('| RVA | Function |\n|---:|---|\n')
            for addr, name, scl in sorted(groups[owner]):
                f.write('| `0x%08X` | `%s` |\n' % (addr, name))
            f.write('\n')

    print('NT CMD source contributions: %d' % len([m for m in modules if '\\windows\\cmd\\' in m[0].lower()]))
    print('NT CMD named functions:      %d' % len(cmdfuncs))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
