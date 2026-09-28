#!/usr/bin/env python3
from pathlib import Path
import hashlib
import sys

ROOT = Path(__file__).resolve().parents[1]

def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8')

def sha(rel):
    return hashlib.sha256((ROOT / rel).read_bytes()).hexdigest()

def need(cond, msg):
    if not cond:
        print('SESMGR R2 static check: FAIL:', msg)
        sys.exit(1)

# Frozen public ABI and loader/far16 world.
need(sha('dlls/sesmgr/sesmgr.def') ==
     'a61b200f3b1e6ff595d6219b31779129bf9c88dfae3b01ea5400ec78918ee913',
     'sesmgr.def changed')
loader_hash = sha('loader/os2host32.c')
need(loader_hash in (
     '793ee7de7c6d0bd79b7d8bbdfaab6774cb2ec6af39b88d0cefc3c714430f0c55',
     'f70f7136e49971f5cacaeec4b17aab1afb768f9f32dcf267d47512f9f210ebda',
     '2c529621dab08a3f5f01de7b4e22b83b7bd34077061ae1e18f094e7ff0ca99ad',
     '9892c6e8d89536b7645561c23599caedc83c61b20c9f9fbb4c315c0b1093f425',
     'b0f4d9b33b7891e37525d0123b7901d06ad6249df93af0c12f741359f98d306e',
     '2558dfed9c6099f5b2a5998edb582fa6ec111671f45ef29265538590335c42aa'),
     'loader/os2host32.c changed outside frozen SESMGR or later MOU/signal/C386 far16 wiring')

def_text = read('dlls/sesmgr/sesmgr.def')
for exp in ('DOSSMSETTITLE=DosSmSetTitle @5', 'DosStopSession @8',
            'DosStartSession @17', 'O2HostQuerySessions @1000'):
    need(exp in def_text, 'missing frozen export ' + exp)

common = read('common/sesmgr/os2_sesmgr.c')
header = read('common/include/os2_sesmgr.h')
backend_h = read('common/include/os2_sesmgr_backend.h')
win32 = read('common/win32/os2_sesmgr_win32.c')
veneer = read('dlls/sesmgr/sesmgr.c')
make = read('Makefile')

# Common semantics must remain host/runtime neutral.
for bad in ('windows.h', 'CreateProcess', 'HANDLE', 'CRITICAL_SECTION',
            'CreateFileMapping', 'WaitForSingleObject', 'SetConsoleTitle'):
    need(bad not in common, 'Win32 mechanic leaked into common core: ' + bad)
for bad in ('OS2HOST32_SESMGR_R1', 'CreateProcess', 'CreateJobObject',
            'TerminateProcess', 'OpenProcess', 'GetProcessTimes'):
    need(bad not in header and bad not in backend_h,
         'Win32 implementation detail leaked into common headers: ' + bad)

# Public DLL is a wire/ABI veneer, not the implementation.
for bad in ('CreateProcessA', 'CreateFileMappingA', 'CreateJobObjectA',
            'TerminateProcess', 'OpenProcess', 'GetExitCodeProcess'):
    need(bad not in veneer, 'Win32 implementation remains in SESMGR veneer: ' + bad)
for call in ('os2_sesmgr_DosStartSession', 'os2_sesmgr_DosStopSession',
             'os2_sesmgr_DosSmSetTitle', 'os2_sesmgr_QuerySessions'):
    need(call in veneer, 'veneer does not route through common core: ' + call)

# Core owns the policy/state that future WHP/OS2SS backends need unchanged.
for token in ('struct Os2SesmgrRegistry', 'struct Os2SesmgrOwnedSession',
              'registry_allocate_id', 'registry_reap_locked', 'remember_owned',
              'validate_start', 'OS2_SESMGR_ERROR_PROCESS_NOT_PARENT'):
    need(token in common or token in header,
         'missing common Session Manager state/policy: ' + token)

# Win32 owns only host process/registry storage mechanics.
for token in ('CreateProcessA', 'CREATE_NEW_CONSOLE', 'DETACHED_PROCESS',
              'CreateJobObjectA', 'TerminateJobObject', 'TerminateProcess',
              'CreateFileMappingA', 'MapViewOfFile', 'CreateMutexA',
              'GetProcessTimes', 'SetConsoleTitleA'):
    need(token in win32, 'missing Win32 backend mechanic: ' + token)
need('Local\\\\OS2HOST32_SESMGR_R1' in win32,
     'Win32 shared registry compatibility name changed/missing')

# STARTDATA stays a packed ABI concern at the veneer.
need('#pragma pack(push, 2)' in veneer and 'struct O2StartData' in veneer,
     'packed STARTDATA wire veneer missing')
need('O2StartData_must_be_60_bytes' in veneer,
     '32-bit STARTDATA size guard missing')

# Build/test integration.
for token in ('SESMGR_COMMON_SRC', 'SESMGR_WIN32_SRC', 'sesmgr-check',
              'sesmgr-veneer-check', 'sesmgr-static-check'):
    need(token in make, 'Makefile missing ' + token)

print('SESMGR R2 static architecture: PASS')
