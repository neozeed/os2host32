#!/usr/bin/env python3
from pathlib import Path
import hashlib, re, struct, subprocess, sys

ROOT=Path(__file__).resolve().parents[1]
SRC=Path(sys.argv[1]).resolve()
REPRO=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else None
REQ='091855fc4f9de8052c8cf4a55830580aab5558da'
LE4X_PACKAGE='76155b6e129e464d9a15e95c92817446f2c05042baaec9309c1639b41e83ef24'
BASE_BOOT_SRC='3a4fc6fd2d119ec437de51188f652d1dd6809eeeb9ab5dbf58780b7fba2edb4a'
FROZEN={
'common/os2msg.h':'f74ca13a414c2eb8973188c403f127f62941f334c0b9e369859ca2c5c94a233a',
'common/os2loader.c':'a32ca6faed71c80b315ae49dc7fe463b46a39b58d2f89a727c065b7bb36a36eb',
'common/os2loader.h':'e72841539c389751faf030cb34627baa396c007adca1c8c096418dbb14e87324',
'common/os2image.c':'53a2df1b49f0f13d70e66a33445f76c652611a9593574913b994e1d1a5ce4954',
'common/os2image.h':'646b8ce62509ede9d1842cbf955c4f5c8776ae9d6b84793660d8886b5ebeaf30',
'common/os2veneer.c':'953f8dc8ba9102844aecd8aa39148b136e910fa4a3df6f18e6e8ba433c56207b',
'common/os2veneer.h':'bda6efb17f09c9d748d8ff9147e0ebf424df895b734d54529646901d324562bb',
'common/os2startup.c':'b80c64f11235242b1bd818f4be233a11d2beb1d83f78a6b3e24c2a757410ff3a',
'common/os2startup.h':'b49e334759765a5509b23bc9b424b3b301d9a7c16ef2f22583bf7ed1b07a4970',
'common/os2sha256.c':'6b54682808ff7ab1af7222b128baf214a86b9b16ca883ea71438174bf8eb1c53',
'common/os2sha256.h':'1c5f2aefe2663652d3fceb697acfe5485b87db868895fb8b97b5772f5536fa9e',
}

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def pe(p):
    b=Path(p).read_bytes(); off=struct.unpack_from('<I',b,0x3c)[0]
    machine=struct.unpack_from('<H',b,off+4)[0]; opt=off+24
    magic=struct.unpack_from('<H',b,opt)[0]; base=struct.unpack_from('<I',b,opt+28)[0]; sub=struct.unpack_from('<H',b,opt+68)[0]
    out=subprocess.check_output(['llvm-objdump','-p',str(p)],text=True,errors='replace')
    dlls=[]; funcs={}; current=None
    for line in out.splitlines():
        m=re.search(r'DLL Name: ([^\s]+)',line,re.I)
        if m:
            current=m.group(1).lower(); dlls.append(current); funcs.setdefault(current,[]); continue
        m=re.match(r'\s+\d+\s+([A-Za-z_][A-Za-z0-9_@?$]*)\s*$',line)
        if m and current: funcs[current].append(m.group(1))
    return machine,magic,base,sub,dlls,funcs

ok=True; L=[]
def chk(label,cond,detail=''):
    global ok
    ok &= bool(cond)
    L.append(f'{label}: {"PASS" if cond else "FAIL"}'+(f' ({detail})' if detail else ''))

head=subprocess.check_output(['git','-C',str(SRC),'rev-parse','HEAD'],text=True).strip()
status=subprocess.check_output(['git','-C',str(SRC),'status','--porcelain'],text=True)
L += ['ReactOS OS2SS LE4C static verification','========================================','']
chk('ReactOS HEAD exact',head==REQ,head)
chk('ReactOS tree clean',status=='')
L.append('Packaged reference fixture SHA256: '+sha(ROOT/'fixtures/hi.exe'))
for rel,expected in FROZEN.items():
    chk(f'Frozen LE4X {rel} byte-identical',sha(ROOT/rel)==expected,sha(ROOT/rel))

boot=(ROOT/'os2boot/os2boot.c').read_text()
chk('SHA identity is diagnostic-only','identity is not a launch guard' in boot and 'VerifyHiHash' not in boot)
chk('compatible-plan guard present','VerifyCompatibleHiPlan' in boot and 'compatible LE plan' in boot)
chk('fixture file size is bounded, not exact','LE4C_MAX_IMAGE_SIZE' in boot and 'LE4C_HI_FILE_SIZE' not in boot)
chk('stack/object sizes come from parsed plan','Plan.objects[1].virtual_size' in boot and 'Plan.stack_offset' in boot)

expected_bins=[('OS2SS.EXE',2),('OS2LE4CLAUNCH.EXE',3),('OS2BOOT.EXE',5)]
for name,sub in expected_bins:
    m,magic,base,s,dlls,funcs=pe(ROOT/'build'/name)
    L += ['',f'===== {name} =====']
    chk('PE32 i386',m==0x14c and magic==0x10b,f'machine=0x{m:04x} magic=0x{magic:04x}')
    chk(f'subsystem {sub}',s==sub,str(s))
    if name=='OS2LE4CLAUNCH.EXE':
        chk('launcher imports only ntdll+kernel32',set(dlls)=={'ntdll.dll','kernel32.dll'},','.join(dlls))
        chk('launcher sole KERNEL32 API is WriteConsoleA',funcs.get('kernel32.dll',[])==['WriteConsoleA'],','.join(funcs.get('kernel32.dll',[])))
    else:
        chk('NTDLL-only imports',set(dlls)=={'ntdll.dll'},','.join(dlls))
    if name=='OS2BOOT.EXE':
        chk('OS2BOOT image base 0x00400000',base==0x00400000,f'0x{base:08x}')
    for fn in ['NtReadVirtualMemory','NtWriteVirtualMemory','NtSetContextThread','NtGetContextThread','NtSetLdtEntries']:
        allfuncs=sum(funcs.values(),[])
        chk(f'forbidden {fn} absent',fn not in allfuncs)
    L.append(f'SHA256: {sha(ROOT/"build"/name)}')

msg=(ROOT/'common/os2msg.h').read_text(); con=(ROOT/'common/le4c_console.h').read_text(); api=(ROOT/'os2ss/api.c').read_text(); srv=(ROOT/'os2ss/os2ss.c').read_text(); launch=(ROOT/'launcher/os2le4claunch.c').read_text()
L += ['','===== frozen LE4X execution path =====']
chk('OS2 protocol remains version 1','#define OS2_PROTOCOL_VERSION        1UL' in msg)
chk('OS2 API message remains 0x38','sizeof(OS2_API_MESSAGE) == 0x38' in msg)
chk('internal execution guard follows parsed plan','InternalStats.internal_applied != Plan.internal_fixup_sites' in boot and 'InternalStats.internal_verified != Plan.internal_fixup_sites' in boot and 'InternalStats.internal_mismatches != 0u' in boot)
chk('external execution guard follows parsed plan','ExternalStats.resolved != Plan.external_fixup_sites' in boot and 'ExternalStats.verified != Plan.external_fixup_sites' in boot and 'ExternalStats.mismatches != 0u' in boot)
chk('C/386 execution arm preserved','OS2BOOT: LE4C EXECUTION ARMED' in boot and 'Le4cEnterLe(EntryVa, Startup.initial_esp)' in boot)
chk('native FS preservation remains','native_fs_preserved' in boot and 'mov fs' not in boot.lower())

L += ['','===== TEMPORARY NATIVE CONSOLE BACKEND =====']
chk('bridge protocol explicitly temporary','TEMPORARY NATIVE CONSOLE BACKEND' in con)
chk('bridge is separate from OS2 protocol','LE4C_CONSOLE_VERSION' in con and '#include "os2msg.h"' not in con)
chk('bridge message layout asserted','sizeof(LE4C_CONSOLE_MESSAGE) == 0xF8' in con)
chk('console bridge chunk fits x86 LPC maximum', 'LE4C_CONSOLE_MAX_BYTES       192UL' in con and 'sizeof(LE4C_CONSOLE_MESSAGE) == 0xF8' in con)
chk('DosWrite streams across console chunks', 'while (Total < Length)' in srv and 'Chunk > LE4C_CONSOLE_MAX_BYTES' in srv and 'Total += ChunkWritten' in srv)
chk('DosWrite no longer rejects bridge-sized writes', 'Length > LE4C_CONSOLE_MAX_BYTES)' not in api)

chk('console port under OS2SS namespace','L"\\\\OS2SS\\\\Le4cConsolePort"' in con)
chk('OS2SS remains DosWrite semantic authority','Os2SrvDosWrite' in api and 'Os2Le4cConsoleWrite' in api)
chk('shared-view offset/length validation preserved','Process->SharedServerBase' in api and 'Process->SharedDataSize - Offset' in api)
chk('raw client pBuffer not introduced','pBuffer' not in api)
chk('visible write is acknowledged before success','NtWaitForSingleObject(gConsoleAckEvent' in srv and 'Actual != Length' in api)
chk('serial debug evidence remains distinct','SERIAL DEBUG OUTPUT' in api)
chk('visible console diagnostic present','visible console write rc=0 actual=%lu' in api)
chk('launcher consumes StandardOutput/StandardError','Params->StandardOutput' in launch and 'Params->StandardError' in launch)
chk('launcher uses supported WriteConsoleA boundary','WriteConsoleA(OutputHandle' in launch)
chk('launcher bridge runs on original main thread','ConsoleBridgeLoop();' in launch and 'using CSR-registered main thread' in launch)
chk('launcher no longer creates raw bridge thread',not re.search(r'\bRtlCreateUserThread\s*\(', launch) and 'ConsoleBridgeThread' not in launch)
chk('launcher stays alive through child lifetime','NtDuplicateObject' in launch and 'NtWaitForSingleObject(ChildWaitHandle' in launch)
chk('bridge exit follows DosExit','Os2Le4cConsoleNotifyExit' in api and 'Le4cConsoleExit' in launch)
chk('no second-console API used',all(x not in launch for x in ['AllocConsole','AttachConsole','FreeConsole','CREATE_NEW_CONSOLE']))

active='\n'.join((ROOT/x).read_text(errors='ignore') for x in ['os2ss/os2ss.c','os2ss/api.c','os2ss/process.c','os2boot/os2boot.c','launcher/os2le4claunch.c','common/le4c_console.h'])
chk('no VIO implementation symbols',not re.search(r'\bVio[A-Za-z0-9_]*\s*\(',active))
chk('no ANSI parser implementation','ansi parser' not in active.lower() and 'escape sequence' not in active.lower())
chk('no KBD/MOU/PM implementation',not re.search(r'\b(Kbd|Mou|Win|Gpi)[A-Z][A-Za-z0-9_]*\s*\(',active))

L += ['','===== ReactOS source-backed console mechanism evidence =====']
ppb=(SRC/'sdk/lib/rtl/ppb.c').read_text(errors='ignore')
proc=(SRC/'sdk/lib/rtl/process.c').read_text(errors='ignore')
coni=(SRC/'dll/win32/kernel32/client/console/init.c').read_text(errors='ignore')
rw=(SRC/'dll/win32/kernel32/client/console/readwrite.c').read_text(errors='ignore')
frw=(SRC/'dll/win32/kernel32/client/file/rw.c').read_text(errors='ignore')
k32thr=(SRC/'dll/win32/kernel32/client/thread.c').read_text(errors='ignore')
rtlthr=(SRC/'sdk/lib/rtl/thread.c').read_text(errors='ignore')
csrapi=(SRC/'subsystems/csr/csrsrv/api.c').read_text(errors='ignore')
chk('RtlCreateProcessParameters inherits ConsoleHandle','ConsoleHandle = NtCurrentPeb()->ProcessParameters->ConsoleHandle;' in ppb)
chk('RtlCreateProcessParameters does not populate StandardOutput','Param->StandardOutput' not in ppb)
chk('RtlCreateUserProcess duplicates only supplied StandardOutput','if (ProcessParameters->StandardOutput)' in proc and 'ZwDuplicateObject' in proc)
chk('ReactOS console-app test is subsystem 3','IMAGE_SUBSYSTEM_WINDOWS_CUI' in coni and 'IsConsoleApp' in coni)
chk('non-console app clears inherited ConsoleHandle','Parameters->ConsoleHandle = NULL;' in coni)
chk('WriteConsoleA routes to IntWriteConsole','return IntWriteConsole' in rw and 'WriteConsoleA' in rw)
chk('IntWriteConsole uses CSR ConsolepWriteConsole','CsrClientCallServer' in rw and 'ConsolepWriteConsole' in rw)
chk('WriteFile redirects console handles to WriteConsoleA','IsConsoleHandle(hFile)' in frw and 'WriteConsoleA(hFile' in frw)
chk('KERNEL32 CreateThread notifies CSR','BasepNotifyCsrOfThread(hThread, &ClientId)' in k32thr)
chk('raw RtlCreateUserThread uses ZwCreateThread directly','ZwCreateThread(&Handle' in rtlthr and 'StartAddress' in rtlthr)
chk('CSR rejects API from unknown CSR thread','This is an API Message coming from a non-CSR Thread' in csrapi and 'STATUS_ILLEGAL_FUNCTION' in csrapi)

L += ['','===== LE4X host regression =====']
oracle=(ROOT/'evidence/LE4X-HOST-ORACLE-GCC.txt')
chk('frozen host oracle evidence present',oracle.exists())
if oracle.exists():
    t=oracle.read_text()
    chk('host oracle PASS','LE4X host construction oracle PASS' in t)
    chk('host 159/159/0 internal','internal planned=159 applied=159 verified=159 mismatches=0' in t)
    chk('host 12/12/0 external','external planned=12 resolved=12 verified=12 mismatches=0' in t)
    chk('host startup frame verified','initial_esp=' in t and 'stack_top=' in t)
    chk('host executes no LE','execution_performed=0' in t)

L += ['','===== developer build ergonomics =====']
mk=(ROOT/'Makefile').read_text() if (ROOT/'Makefile').exists() else ''
chk('top-level Makefile included',bool(mk))
chk('Makefile supports canonical build','build-le4c-canonical.sh' in mk and 'REACTOS ?=' in mk)
chk('Makefile supports verify','verify:' in mk)
chk('Makefile supports arbitrary image oracle','check-image:' in mk and 'IMAGE ?=' in mk)

L += ['','===== warning-clean / deterministic =====']
build=(ROOT/'build-le4c-canonical.sh').read_text()
chk('-Wall -Wextra -Werror',all(x in build for x in ['-Wall','-Wextra','-Werror']))
chk('/Brepro','/Brepro' in build)
if REPRO:
    for n in ['OS2SS.EXE','OS2LE4CLAUNCH.EXE','OS2BOOT.EXE']:
        chk(f'deterministic {n}',(ROOT/'build'/n).read_bytes()==(REPRO/n).read_bytes())
chk('ReactOS source still clean',subprocess.check_output(['git','-C',str(SRC),'status','--porcelain'],text=True)=='')
L += ['','frozen_le4x_package_sha256 = '+LE4X_PACKAGE,'builder_runtime_tested = false','visible_console_output_observed = null','visible_hi_observed = null','le4x_regression_passed = null','le4c_pass = false']
print('\n'.join(L)+'\n')
sys.exit(0 if ok else 1)
