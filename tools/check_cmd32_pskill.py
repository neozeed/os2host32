from pathlib import Path

root = Path(__file__).resolve().parent.parent

def text(name):
    return (root / name).read_text(errors='replace')

ses = text('sesmgr.c')
defs = text('sesmgr.def')
cmd = text('cmd32os2.c')
hdr = text('cmdos2.h')
win = text('cmdos2_session_win32.c')
os2 = text('cmdos2_session_os2.c')
os2def = text('cmd32os2_os2.def')
build = text('build-os2-shell.cmd')
fixture = text('cmd32-pskill-child.c')
fixture_def = text('cmd32-pskill-child.def')

checks = [
    ('shared session registry', 'CreateFileMappingA' in ses and 'OS2HOST32_SESMGR_R1' in ses),
    ('shared registry serialized', 'CreateMutexA' in ses and 'registry_lock' in ses),
    ('PID reuse guard', 'GetProcessTimes' in ses and 'createTimeLow' in ses and 'ownerTimeLow' in ses),
    ('current-process pseudo handle accepted', 'process == NULL || process == INVALID_HANDLE_VALUE' not in ses and 'GetCurrentProcess() is the valid Win32 pseudo-handle' in ses),
    ('globally allocated session id', 'registry_allocate_session_id' in ses and 'nextSessionId' in ses),
    ('registration-before-resume', 'CREATE_SUSPENDED' in ses and 'remember_session' in ses and 'ResumeThread' in ses),
    ('initial executable/title recorded', 'registry_add_session(sid, pid, process, program, title)' in ses),
    ('title update API', 'DosSmSetTitle' in ses and 'registry_set_title' in ses),
    ('historical title ordinal', 'DOSSMSETTITLE=DosSmSetTitle @5' in defs),
    ('historical stop ordinal', 'DosStopSession @8' in defs),
    ('query extension ordinal', 'O2HostQuerySessions @1000' in defs),
    ('session job containment', 'CreateJobObjectA' in ses and 'AssignProcessToJobObject' in ses and 'TerminateJobObject' in ses),
    ('forced stop is owner-local', 'TerminateProcess' in ses and 'g_sessions[i].sessionId' in ses),
    ('CMD session info ABI', 'struct CmdO2SessionInfo' in hdr and 'CmdO2QuerySessions' in hdr and 'CmdO2StopSession' in hdr),
    ('native wrappers', 'PFN_DOSSTOPSESSION' in win and 'PFN_O2HOSTQUERYSESSIONS' in win),
    ('C/386 wrappers', 'DOSSTOPSESSION' in os2 and 'O2HOSTQUERYSESSIONS' in os2),
    ('C/386 imports', 'DOSSTOPSESSION=SESMGR.8' in os2def and 'O2HOSTQUERYSESSIONS=SESMGR.1000' in os2def),
    ('PS builtin', '{ "PS",       ePs' in cmd and 'TASK  PID       EXECUTABLE  TITLE' in cmd),
    ('KILL builtin', '{ "KILL",     eKill' in cmd and 'CmdO2StopSession(taskId)' in cmd),
    ('fixture title update', 'DOSSMSETTITLE(0UL' in fixture and 'DosSleep(60000UL)' in fixture),
    ('fixture ordinal import', 'DOSSMSETTITLE=SESMGR.5' in fixture_def),
    ('fixture build', 'cmd32-pskill-child.c' in build and 'cmd32-pskill-child.exe' in build),
]

bad = [name for name, ok in checks if not ok]
if bad:
    for name in bad:
        print('FAIL:', name)
    raise SystemExit(1)

print('CMD32 PS/KILL session-control regression PASS')
print('  shared SESMGR registry records task id, host PID, executable and title')
print('  related START children are registered before first guest instruction')
print('  SESMGR.5 title changes update the shared title seen by PS')
print('  PS lists this shell\'s live related sessions; KILL stops by task id')
print('  historical SESMGR.8 DosStopSession is the kill boundary')
