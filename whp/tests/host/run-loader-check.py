#!/usr/bin/env python3
"""Execute production LE/LX parsing, fixups and C/386 bridge installation on Linux."""
import pathlib,re,subprocess,tempfile,sys
root=pathlib.Path(__file__).resolve().parents[1]
s=(root/'whp_os2_v2_hi.c').read_text()
def func(n):
 m=re.search(r'^static [^\n]*\b'+n+r'\(',s,re.M);assert m,n
 b=s.index('{',m.end());e=b+1;depth=1
 while depth:depth+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[m.start():e]+'\n'
macros='\n'.join(l for l in s.splitlines() if l.startswith('#define ') and not l.endswith('\\'))
structs=s[s.index('struct LeObject {'):s.index('struct GuestAlloc {')]
head='''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#define _stricmp strcasecmp
static __attribute__((noreturn)) void fatal(const char *s){fprintf(stderr,"fatal: %s\\n",s);exit(1);}
'''
functions=['rd16','rds16','rd32','wr32','align_up','file_range','load_file','parse_header','parse_objects','parse_import_modules','skip_objmod','source_object_offset','find_or_add_import','import_index','find_or_add_name_import','name_import_index','scan_fixups']
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp)
 code=head+macros+'\n'+structs+'\nstruct Runtime {uint8_t *ram;uint32_t module_next,stub_next;};\n'+'\n'.join(map(func,functions))+'\n'+(root/'v2_far16.h').read_text()+'\n'+func('apply_fixups')+func('check_image')+'\nint main(int argc,char **argv){return argc==2?check_image(argv[1]):2;}\n'
 (p/'check.c').write_text(code)
 subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Wno-misleading-indentation','-Werror','-fsanitize=address,undefined','-g',str(p/'check.c'),'-o',str(p/'check')],check=True)
 for arg in sys.argv[1:]:subprocess.run([str(p/'check'),str(pathlib.Path(arg).resolve())],check=True)

 # Deliberately corrupt the first 14-byte fragment of the supplied CMD fixture.
 # Mutation is temporary; the original guest executable is never rewritten.
 if sys.argv[1:] and pathlib.Path(sys.argv[1]).name=='cmd32os2_os2.exe':
  data=bytearray(pathlib.Path(sys.argv[1]).read_bytes())
  matches=[i for i in range(len(data)-14) if data[i]==0x9a and data[i+5:i+8]==b'\x66\x67\xea']
  assert matches
  data[matches[0]]=0x90
  bad=p/'bad-thunk.exe';bad.write_bytes(data)
  result=subprocess.run([str(p/'check'),str(bad)],capture_output=True,text=True)
  assert result.returncode!=0 and 'unrecognized far16 fragment' in result.stderr,result.stderr
  print('loader negative check PASS: corrupt migration fragment rejected')
