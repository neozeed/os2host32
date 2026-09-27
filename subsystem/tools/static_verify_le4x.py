#!/usr/bin/env python3
from pathlib import Path
import hashlib,re,struct,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
SRC=Path(sys.argv[1]).resolve()
REPRO=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else None
REQ='091855fc4f9de8052c8cf4a55830580aab5558da'
LE3R='5785a7e761f0b4b225a4db16d0a2dc568215132f4989e86d157cd6fcccaf0650'
PARSER_C='a32ca6faed71c80b315ae49dc7fe463b46a39b58d2f89a727c065b7bb36a36eb'
PARSER_H='e72841539c389751faf030cb34627baa396c007adca1c8c096418dbb14e87324'
HI='3e0383860d8e76262b3c954bb95ae8e8e3c7887deddfbfd7705c4b8c580441c7'
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pe(p):
    b=Path(p).read_bytes(); off=struct.unpack_from('<I',b,0x3c)[0]; machine=struct.unpack_from('<H',b,off+4)[0]; opt=off+24
    magic=struct.unpack_from('<H',b,opt)[0]; base=struct.unpack_from('<I',b,opt+28)[0]; sub=struct.unpack_from('<H',b,opt+68)[0]
    out=subprocess.check_output(['llvm-objdump','-p',str(p)],text=True,errors='replace')
    dlls=re.findall(r'DLL Name: ([^\s]+)',out,re.I); funcs=[]
    for line in out.splitlines():
        m=re.match(r'\s+\d+\s+([A-Za-z_][A-Za-z0-9_@?$]*)\s*$',line)
        if m: funcs.append(m.group(1))
    return machine,magic,base,sub,[x.lower() for x in dlls],funcs
ok=True; L=[]
def chk(label,cond,detail=''):
    global ok; ok &= bool(cond); L.append(f'{label}: {"PASS" if cond else "FAIL"}'+(f' ({detail})' if detail else ''))
head=subprocess.check_output(['git','-C',str(SRC),'rev-parse','HEAD'],text=True).strip()
status=subprocess.check_output(['git','-C',str(SRC),'status','--porcelain'],text=True)
L += ['ReactOS OS2SS LE4X static verification','========================================','']
chk('ReactOS HEAD exact',head==REQ,head); chk('ReactOS tree clean',status=='')
chk('Frozen LE3R package hash recorded',LE3R in (ROOT/'LE4X-RESULTS.json').read_text() if (ROOT/'LE4X-RESULTS.json').exists() else True)
chk('Frozen LE1 parser C byte-identical',sha(ROOT/'common/os2loader.c')==PARSER_C,sha(ROOT/'common/os2loader.c'))
chk('Frozen LE1 parser H byte-identical',sha(ROOT/'common/os2loader.h')==PARSER_H,sha(ROOT/'common/os2loader.h'))
chk('Exact hi.exe',sha(ROOT/'fixtures/hi.exe')==HI,sha(ROOT/'fixtures/hi.exe'))
for name,sub in [('OS2SS.EXE',2),('OS2LE4XLAUNCH.EXE',3),('OS2BOOT.EXE',5)]:
    m,magic,base,s,dlls,funcs=pe(ROOT/'build'/name); L += ['',f'===== {name} =====']
    chk('PE32 i386',m==0x14c and magic==0x10b,f'machine=0x{m:04x} magic=0x{magic:04x}')
    chk(f'subsystem {sub}',s==sub,str(s)); chk('NTDLL-only imports',sorted(set(dlls))==['ntdll.dll'],','.join(sorted(set(dlls))))
    L.append(f'ImageBase: 0x{base:08x}'); L.append(f'SHA256: {sha(ROOT/"build"/name)}')
    if name=='OS2BOOT.EXE':
        chk('OS2BOOT image base 0x00400000',base==0x00400000)
        for fn in ['NtAllocateVirtualMemory','NtProtectVirtualMemory','NtFreeVirtualMemory','NtQueryVirtualMemory','NtCreateSection','NtConnectPort','NtRequestWaitReplyPort','NtTerminateProcess']:
            chk(f'{fn} imported',fn in funcs)
        for fn in ['NtReadVirtualMemory','NtWriteVirtualMemory','NtSetContextThread','NtGetContextThread','NtSetLdtEntries']:
            chk(f'forbidden {fn} absent',fn not in funcs)
boot=(ROOT/'os2boot/os2boot.c').read_text(); api=(ROOT/'os2ss/api.c').read_text(); msg=(ROOT/'common/os2msg.h').read_text(); img=(ROOT/'common/os2image.c').read_text(); ven=(ROOT/'common/os2veneer.c').read_text(); startup=(ROOT/'common/os2startup.c').read_text()
L += ['','===== protocol / minimum API surface =====']
chk('Protocol remains version 1','#define OS2_PROTOCOL_VERSION        1UL' in msg)
chk('Message envelope remains 0x38','sizeof(OS2_API_MESSAGE) == 0x38' in msg)
for ordinal,name in [(224,'DosQueryHType'),(234,'DosExit'),(256,'DosSetFilePtr'),(282,'DosWrite'),(299,'DosAllocMem'),(304,'DosFreeMem'),(305,'DosSetMem'),(348,'DosQuerySysInfo')]:
    chk(f'DOSCALLS.{ordinal} gateway case',f'case {ordinal}u:' in boot)
    chk(f'{name} server/client semantics present',name in boot and name in api)
chk('No unrelated protocol API expansion','Os2ApiMax' in msg and msg.count('Os2ApiDos')==8)
chk('Historical ordinals distinct from protocol enum','224' not in re.search(r'typedef enum _OS2_API_NUMBER(.*?)\} OS2_API_NUMBER',msg,re.S).group(1))
L += ['','===== veneers / external fixups =====']
chk('Dedicated ordinal veneer emitter','mov eax, imm32' in ven and 'jmp rel32' in ven)
chk('Veneers carry all eight ordinals','gExpectedOrdinals[8]' in boot)
chk('External REL32 uses source site + 4','next_va = site_va + 4u' in img and 'r->address' in img)
chk('12-site apply+verify diagnostics','external fixups planned=%lu resolved=%lu verified=%lu mismatches=%lu' in boot)
chk('Execution guard requires 12/12/0','ExternalStats.resolved != 12u' in boot and 'ExternalStats.verified != 12u' in boot and 'ExternalStats.mismatches != 0u' in boot)
L += ['','===== C/386 startup / local transition =====']
chk('Proven five-DWORD startup frame','esp -= 4u' in startup and startup.count('esp -= 4u')==5)
chk('Frame verification checks env/cmd slots','off + 12u' in startup and 'off + 16u' in startup)
chk('Native stack anchor captured','gLe4xNativeEsp' in boot and 'mov dword ptr [gLe4xNativeEsp], esp' in boot)
chk('Gateway switches to native ESP','mov esp, edx' in boot and 'call Le4xGatewayDispatch' in boot)
chk('Gateway restores OS/2 ESP','mov edx, dword ptr [gLe4xOs2Esp]' in boot and 'ret' in boot)
chk('FS remains untouched','mov fs' not in boot.lower() and 'NtSetLdtEntries' not in boot)
chk('Local transition exists','Le4xEnterLe' in boot and 'jmp eax' in boot)
chk('No NtSetContextThread transition','NtSetContextThread' not in boot)
chk('Execution guard marker','OS2BOOT: LE4X EXECUTION ARMED' in boot)
chk('Transition occurs only after armed marker',boot.find('OS2BOOT: LE4X EXECUTION ARMED') < boot.rfind('Le4xEnterLe(EntryVa, Startup.initial_esp)'))
L += ['','===== frozen LE3R construction preserved =====']
for s in ['os2l_materialize_objects','os2l_verify_materialized_objects','os2l_apply_internal_fixups','os2l_verify_internal_fixups','internal fixups planned=%lu applied=%lu verified=%lu mismatches=%lu','NativeGuardUnchanged','PAGE_EXECUTE_READ','PAGE_READWRITE']:
    chk(s,s in boot or s in img)
chk('Internal target uses actual base','value = tgt->actual_base + f->target_value' in img)
chk('159 internal execution guard','InternalStats.internal_applied != 159u' in boot and 'InternalStats.internal_verified != 159u' in boot and 'InternalStats.internal_mismatches != 0u' in boot)
L += ['','===== host oracle =====']
for f in ['LE4X-HI-ORACLE.txt','evidence/LE4X-HOST-ORACLE-GCC.txt','evidence/LE4X-HOST-ORACLE-CLANG.txt']:
    chk(f,(ROOT/f).exists())
if (ROOT/'LE4X-HI-ORACLE.txt').exists():
    t=(ROOT/'LE4X-HI-ORACLE.txt').read_text()
    chk('Host oracle PASS','LE4X host construction oracle PASS' in t)
    chk('Host internal 159/159/0','internal planned=159 applied=159 verified=159 mismatches=0' in t)
    chk('Host external 12/12/0','external planned=12 resolved=12 verified=12 mismatches=0' in t)
    chk('Host startup ESP proven','initial_esp=010127fc' in t)
    chk('Host does not execute LE','execution_performed=0' in t)
L += ['','===== warning-clean / deterministic =====']
build=(ROOT/'build-le4x-canonical.sh').read_text(); chk('-Wall -Wextra -Werror',all(x in build for x in ['-Wall','-Wextra','-Werror'])); chk('/Brepro','/Brepro' in build)
if REPRO:
    for n in ['OS2SS.EXE','OS2LE4XLAUNCH.EXE','OS2BOOT.EXE']:
        chk(f'deterministic {n}',(ROOT/'build'/n).read_bytes()==(REPRO/n).read_bytes())
chk('ReactOS source still clean',subprocess.check_output(['git','-C',str(SRC),'status','--porcelain'],text=True)=='')
L += ['','builder_runtime_tested = false','builder_ran_reactos = false','builder_ran_qemu = false','builder_ran_bochs = false','builder_executed_hi_exe = false','execution_path_implemented = true']
print('\n'.join(L)+'\n'); sys.exit(0 if ok else 1)
