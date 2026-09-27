#!/usr/bin/env python3
from pathlib import Path
import hashlib, re, subprocess, sys
ROOT=Path(__file__).resolve().parents[1]
SRC=Path(sys.argv[1]).resolve()
REPRO=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else None
REQ='091855fc4f9de8052c8cf4a55830580aab5558da'
BASE_OS2SS='6720cd41b4a8879bb620432c0a9334dc3ba904b7da0ce07b4b9c6dba1a35a22f'
BASE_BOOT='3dfc0633f7dfa0b820fcae4ad46973f59f8214ce9876d5da7228d7e4a935ebdf'
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
ok=True; out=[]
def chk(name, cond, detail=''):
    global ok
    ok &= bool(cond); out.append(f'{name}: {"PASS" if cond else "FAIL"}'+(f' ({detail})' if detail else ''))
launch=(ROOT/'launcher/os2le4claunch.c').read_text()
defs=(ROOT/'common/ntdll.def').read_text()
build=(ROOT/'build-le4c-canonical.sh').read_text()
smloop=(SRC/'base/system/smss/smloop.c').read_text(errors='ignore')
smsb=(SRC/'base/system/smss/smsbapi.c').read_text(errors='ignore')
os2srv=(ROOT/'os2ss/os2ss.c').read_text()
proc=(ROOT/'os2ss/process.c').read_text()
head=subprocess.check_output(['git','-C',str(SRC),'rev-parse','HEAD'],text=True).strip()
status=subprocess.check_output(['git','-C',str(SRC),'status','--porcelain'],text=True)
out += ['LE4C multi-launch QoL static verification','=========================================','']
chk('ReactOS HEAD exact',head==REQ,head)
chk('ReactOS tree clean',status=='')
chk('non-invasive OS2SS directory probe exists','CheckOs2SubsystemRunning' in launch and 'NtOpenDirectoryObject' in launch and 'DIRECTORY_TRAVERSE' in launch)
chk('probe targets \\OS2SS','L"\\\\OS2SS"' in launch)
chk('running subsystem skips deferred load','OS2SS already running; skipping deferred subsystem load' in launch)
chk('absent subsystem requests deferred load','OS2SS not running; requesting deferred subsystem' in launch and 'SmLoadDeferedSubsystem' in launch)
chk('single deferred-load callsite',launch.count('SmLoadDeferedSubsystem(SmApiPort, &Os2Name)')==1,str(launch.count('SmLoadDeferedSubsystem(SmApiPort, &Os2Name)')))
chk('startup-race namespace recheck','became available during startup race' in launch and launch.count('CheckOs2SubsystemRunning(&SubsystemRunning)')>=2)
chk('console reconnect retry is bounded','ConnectConsoleBridgeWithRetry' in launch and 'Attempt < 20' in launch and 'NtDelayExecution' in launch)
chk('retry only uses native NTDLL','NtDelayExecution' in defs and 'NtOpenDirectoryObject' in defs)
chk('launcher still connects console bridge before process create',launch.find('ConnectConsoleBridgeWithRetry()') < launch.find('RtlCreateUserProcess('))
chk('launcher still submits every new child through SmExecPgm','SmExecPgm(SmApiPort, &Pi, FALSE)' in launch)
chk('SMSS deferred-load handler executes configured subsystem', 'SmpExecuteCommand(&RegEntry->Value' in smloop)
chk('SMSS create-session path locates registered subsystem by type','SmpLocateKnownSubSysByType(MuSessionId, SubSystemType)' in smsb)
chk('OS2SS SB loop accepts repeated SbpCreateSession','else if (ReceiveMsg.ApiNumber == SbpCreateSession)' in os2srv)
chk('OS2SS allocates fresh process record per create-session','Os2CreateProcessRecord' in os2srv and 'g_NextPersonalityProcessId++' in proc)
chk('OS2SS clears console association after launcher close','gConsoleCommPort = NULL' in os2srv and 'console launcher closed/died' in os2srv)
chk('frozen streaming OS2SS binary unchanged',sha(ROOT/'build/OS2SS.EXE')==BASE_OS2SS,sha(ROOT/'build/OS2SS.EXE'))
chk('frozen flexible OS2BOOT binary unchanged',sha(ROOT/'build/OS2BOOT.EXE')==BASE_BOOT,sha(ROOT/'build/OS2BOOT.EXE'))
chk('launcher subsystem remains console 3','/subsystem:console' in build)
chk('OS2BOOT still patched subsystem 5','patch_pe_subsystem.py" "$BUILD/OS2BOOT.EXE" 5' in build)
if REPRO:
    for n in ['OS2SS.EXE','OS2LE4CLAUNCH.EXE','OS2BOOT.EXE']:
        chk(f'deterministic {n}',(ROOT/'build'/n).read_bytes()==(REPRO/n).read_bytes())
chk('ReactOS still clean after checks',subprocess.check_output(['git','-C',str(SRC),'status','--porcelain'],text=True)=='')
out += ['','runtime_tested = false','multilaunch_behavior_observed = null','subsystem_reuse_observed = null']
print('\n'.join(out)+'\n')
sys.exit(0 if ok else 1)
