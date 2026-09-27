#!/usr/bin/env python3
from pathlib import Path
import hashlib,re,struct,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
SRC=Path(sys.argv[1]).resolve(); REPRO=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else None
REQ='091855fc4f9de8052c8cf4a55830580aab5558da'
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pe(p):
    b=Path(p).read_bytes(); off=struct.unpack_from('<I',b,0x3c)[0]; opt=off+24
    machine=struct.unpack_from('<H',b,off+4)[0]; magic=struct.unpack_from('<H',b,opt)[0]
    base=struct.unpack_from('<I',b,opt+28)[0]; sub=struct.unpack_from('<H',b,opt+68)[0]
    out=subprocess.check_output(['llvm-objdump','-p',str(p)],text=True,errors='replace')
    dlls=[]; funcs={}; cur=None
    for line in out.splitlines():
        m=re.search(r'DLL Name: ([^\s]+)',line,re.I)
        if m: cur=m.group(1).lower(); dlls.append(cur); funcs.setdefault(cur,[]); continue
        m=re.match(r'\s+\d+\s+([A-Za-z_][A-Za-z0-9_@?$]*)\s*$',line)
        if m and cur: funcs[cur].append(m.group(1))
    return machine,magic,base,sub,dlls,funcs
ok=True; out=[]
def chk(name,cond,detail=''):
    global ok
    ok &= bool(cond); out.append(f'{name}: {"PASS" if cond else "FAIL"}'+(f' ({detail})' if detail else ''))
head=subprocess.check_output(['git','-C',str(SRC),'rev-parse','HEAD'],text=True).strip()
clean=subprocess.check_output(['git','-C',str(SRC),'status','--porcelain'],text=True)==''
out += ['ReactOS OS2SS LE4IO interactive I/O static verification','=====================================================','']
chk('ReactOS HEAD exact',head==REQ,head); chk('ReactOS tree clean',clean)
boot=(ROOT/'os2boot/os2boot.c').read_text(); api=(ROOT/'os2ss/api.c').read_text(); srv=(ROOT/'os2ss/os2ss.c').read_text(); launch=(ROOT/'launcher/os2le4claunch.c').read_text(); msg=(ROOT/'common/os2msg.h').read_text(); con=(ROOT/'common/le4c_console.h').read_text()
for name,sub in [('OS2SS.EXE',2),('OS2LE4CLAUNCH.EXE',3),('OS2BOOT.EXE',5)]:
    m,magic,base,s,dlls,funcs=pe(ROOT/'build'/name); out += ['',f'===== {name} =====']
    chk('PE32 i386',m==0x14c and magic==0x10b,f'machine=0x{m:04x} magic=0x{magic:04x}')
    chk(f'subsystem {sub}',s==sub,str(s))
    if name=='OS2LE4CLAUNCH.EXE':
        chk('launcher imports only NTDLL+KERNEL32',set(dlls)=={'ntdll.dll','kernel32.dll'},','.join(dlls))
        k=set(funcs.get('kernel32.dll',[])); chk('launcher KERNEL32 surface only ReadConsoleA+WriteConsoleA',k=={'ReadConsoleA','WriteConsoleA'},','.join(sorted(k)))
    else: chk('NTDLL-only imports',set(dlls)=={'ntdll.dll'},','.join(dlls))
    if name=='OS2BOOT.EXE': chk('OS2BOOT ImageBase 0x00400000',base==0x00400000,f'0x{base:08x}')
    allf=sum(funcs.values(),[])
    for fn in ['NtReadVirtualMemory','NtWriteVirtualMemory','NtSetContextThread','NtGetContextThread','NtSetLdtEntries']:
        chk(f'forbidden {fn} absent',fn not in allf)
    out.append('SHA256: '+sha(ROOT/'build'/name))
out += ['','===== protocol / API surface =====']
chk('protocol version remains 1','#define OS2_PROTOCOL_VERSION        1UL' in msg)
chk('OS2 API message remains 0x38','sizeof(OS2_API_MESSAGE) == 0x38' in msg)
chk('DosGetDateTime protocol API exists','Os2ApiDosGetDateTime' in msg and 'Os2SrvDosGetDateTime' in api and 'ClientDosGetDateTime' in boot)
chk('DosRead protocol API exists','Os2ApiDosRead' in msg and 'Os2SrvDosRead' in api and 'ClientDosRead' in boot)
chk('DosGetDateTime uses native system/local time','NtQuerySystemTime' in api and 'RtlSystemTimeToLocalTime' in api and 'RtlTimeToTimeFields' in api)
chk('stdin DosRead uses shared view + console bridge','DosReadRequest.SharedOffset' in boot and 'Os2Le4cConsoleRead' in api and 'Process->SharedServerBase' in api)
chk('console bridge has read operation','Le4cConsoleRead' in con and 'ReadConsoleA' in launch)
chk('console input acknowledged with bytes','VISIBLE REACTOS CONSOLE INPUT' in launch and 'SendConsoleAck' in launch)
out += ['','===== temporary native file backend =====']
for ord,name in [(223,'DosQueryPathInfo'),(257,'DosClose'),(259,'DosDelete'),(272,'DosSetFileSize'),(273,'DosOpen')]:
    chk(f'{name} gateway present',f'case {ord}u:' in boot and f'Client{name}' in boot)
chk('file reads use local NtReadFile for h>=3','NativeFileFromOs2(Handle)' in boot and 'NtReadFile(Native' in boot)
chk('file writes use local NtWriteFile for h>=3','NtWriteFile(Native' in boot)
chk('file seek uses NtSetInformationFile','FilePositionInformation' in boot and 'NtSetInformationFile' in boot)
chk('open uses native NtCreateFile','NtCreateFile(&FileHandle' in boot)
chk('path info uses NtQueryFullAttributesFile','NtQueryFullAttributesFile' in boot)
chk('delete uses NtDeleteFile','NtDeleteFile' in boot)
chk('fixed local HFILE table only','LE4IO_MAX_FILES' in boot and 'gFiles[LE4IO_MAX_FILES]' in boot)
out += ['','===== generalized compatible LE execution guard =====']
chk('SHA remains diagnostic only','identity is not a launch guard' in boot)
chk('generic plan guard exists','VerifyCompatibleLePlan' in boot and 'unsupported_fixup_records' in boot)
chk('only OFF32 internal fixups accepted','OS2L_SRC_OFF32' in boot)
chk('only REL32 ordinal externals accepted','OS2L_SRC_REL32' in boot and 'OS2L_TARGET_IMPORT_ORDINAL' in boot)
chk('supported ordinal set includes phoon/infocom','223u,224u,230u,234u,256u,257u,259u,272u,273u,281u,282u,299u,304u,305u,348u' in boot)
chk('veneer count follows parsed imports','Plan->ordinal_import_count' in boot and 'ResolutionCount' in boot)
chk('execution guard follows dynamic plan counts','InternalStats.internal_applied != Plan.internal_fixup_sites' in boot and 'ExternalStats.resolved != Plan.external_fixup_sites' in boot)
chk('native FS preservation unchanged','native_fs_preserved' in boot and 'mov fs' not in boot.lower())
out += ['','===== persistent subsystem / console QoL preserved =====']
chk('OS2SS directory readiness probe preserved','CheckOs2SubsystemRunning' in launch and 'NtOpenDirectoryObject' in launch)
chk('persistent subsystem skip path preserved','OS2SS already running; skipping deferred subsystem load' in launch)
chk('launcher accepts target image argument','GetTargetImageArgument' in launch and 'target image=%wZ' in launch)
chk('main-thread console bridge preserved','using CSR-registered main thread' in launch and 'RtlCreateUserThread(' not in launch)
chk('streaming console output preserved','while (Total < Length)' in srv and 'LE4C_CONSOLE_MAX_BYTES' in srv)
chk('no VIO implementation',not re.search(r'\bVio[A-Za-z0-9_]*\s*\(', '\n'.join([boot,api,srv,launch])))
chk('no ANSI parser','escape sequence' not in ('\n'.join([boot,api,srv,launch])).lower())
out += ['','===== fixture host-oracle evidence =====']
for fixture in ['hi.exe','hi2.exe','phoon.exe','infocom.exe']:
    ev=ROOT/'evidence'/('oracle-'+fixture+'.txt')
    chk(f'{fixture} oracle evidence present',ev.exists())
    if ev.exists():
        t=ev.read_text(); chk(f'{fixture} construction oracle PASS','LE4IO host construction oracle PASS' in t and 'execution_performed=0' in t)
if REPRO:
    out += ['','===== deterministic rebuild =====']
    for n in ['OS2SS.EXE','OS2LE4CLAUNCH.EXE','OS2BOOT.EXE']:
        chk(f'deterministic {n}',(ROOT/'build'/n).read_bytes()==(REPRO/n).read_bytes())
chk('ReactOS source remains clean',subprocess.check_output(['git','-C',str(SRC),'status','--porcelain'],text=True)=='')
out += ['','runtime_tested = false','interactive_input_observed = null','phoon_runtime_observed = null','infocom_runtime_observed = null']
print('\n'.join(out)+'\n'); sys.exit(0 if ok else 1)
