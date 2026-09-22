#!/usr/bin/env python3
"""Compare printable ASCII strings in OS/2 2.0 and Dec-1991 NT CMD images."""
import os
import sys


def ascii_strings(data, minimum=4):
    out = []
    start = None
    for i, b in enumerate(data):
        if 32 <= b <= 126:
            if start is None:
                start = i
        else:
            if start is not None and i - start >= minimum:
                out.append((start, data[start:i].decode('ascii')))
            start = None
    if start is not None and len(data) - start >= minimum:
        out.append((start, data[start:].decode('ascii')))
    return out


def useful(s):
    # Ignore very short compiler/noise strings when presenting common anchors.
    if len(s) < 5:
        return False
    alpha = sum(ch.isalpha() for ch in s)
    return alpha >= 2


def main():
    if len(sys.argv) != 4:
        print('usage: compare_cmd_strings.py os2cmd ntcmd outdir', file=sys.stderr)
        return 2
    op, np, outdir = sys.argv[1:]
    os.makedirs(outdir, exist_ok=True)
    os2 = ascii_strings(open(op,'rb').read())
    nt = ascii_strings(open(np,'rb').read())
    os2map = {}
    ntmap = {}
    for off,s in os2:
        os2map.setdefault(s,[]).append(off)
    for off,s in nt:
        ntmap.setdefault(s,[]).append(off)
    common = sorted((s for s in set(os2map) & set(ntmap) if useful(s)),
                    key=lambda s:(-len(s),s))
    with open(os.path.join(outdir,'common_strings.tsv'),'w',encoding='utf-8',newline='') as f:
        f.write('string\tos2_offsets\tnt_offsets\n')
        for s in common:
            safe=s.replace('\t','\\t').replace('\r','\\r').replace('\n','\\n')
            f.write('%s\t%s\t%s\n' %
                    (safe,
                     ','.join('0x%X'%x for x in os2map[s]),
                     ','.join('0x%X'%x for x in ntmap[s])))
    print('OS/2 strings: %d unique' % len(os2map))
    print('NT strings:   %d unique' % len(ntmap))
    print('common useful exact strings: %d' % len(common))
    for s in common[:20]:
        print('  %s' % s[:100])
    return 0

if __name__=='__main__':
    raise SystemExit(main())
