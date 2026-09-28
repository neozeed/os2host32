# SESMGR R2 backend design

## Purpose

SESMGR is an execution-policy boundary, not merely another collection of host API wrappers.  The same OS/2 session rules must eventually be usable by:

- the native Win32 OS2HOST32 personality;
- the WHP guest execution engine; and
- the ReactOS external OS2SS personality server.

R2 therefore separates OS/2 Session Manager policy/state from host process mechanics without changing the existing native ABI.

## Layering

```text
OS/2 caller
    |
    v
SESMGR.dll ABI / STARTDATA wire veneer
    |
    v
common/sesmgr/os2_sesmgr.c
    |
    +-- STARTDATA policy and validation
    +-- session-id allocation
    +-- shared registry format and stale-record reaping
    +-- related-session ownership table
    +-- title/query ownership semantics
    +-- DosStopSession semantics
    |
    v
Os2SesmgrBackendOps
    |
    v
common/win32/os2_sesmgr_win32.c
    +-- Win32 process identity/liveness
    +-- named mapping + mutex registry storage
    +-- CreateProcess / STARTUPINFO / consoles
    +-- Job Objects for related children
    +-- resume / terminate / wait / handle close
    +-- SetConsoleTitle
```

## Common-owned state

`struct Os2SesmgrRegistry` is deliberately backend-neutral.  It owns the logical shared record format:

- session id;
- child process identity;
- owner process identity;
- program name;
- session title.

A process identity is `(pid, token_low, token_high)`.  The native Win32 backend uses process creation time for the token, preserving the pre-R2 PID-reuse protection.  Other backends may use an equivalent stable identity token.

`struct Os2SesmgrSession` also owns the local table of related sessions started by this personality instance.  Native handles are opaque `uintptr_t` tokens; common code never names a Win32 `HANDLE`.

## Registry storage is a backend operation

The common layer owns registry *format and semantics*, but not its storage mechanism.

The Win32 backend retains the existing named objects:

- `Local\\OS2HOST32_SESMGR_R1`
- `Local\\OS2HOST32_SESMGR_R1_MUTEX`

The new common registry layout is intentionally binary-compatible with the old R1 mapped structure, so a refactor does not change the native cross-process contract.

A future OS2SS backend can return a pointer to server-owned registry storage under its own lock.  A WHP backend can use runtime-owned guest-session storage.  Neither needs to reproduce Win32 file mappings or mutex names.

## STARTDATA boundary

The packed historical 32-bit STARTDATA layout remains private to `dlls/sesmgr/sesmgr.c`.  The veneer interprets the historical accepted prefix lengths and produces a normalized `Os2SesmgrStartRequest`.

The common layer validates the OS/2-facing policy:

- Related is independent or child;
- FgBg is foreground or background;
- TraceOpt remains unsupported except NONE;
- InheritOpt is SHELL or PARENT;
- SessionType is default/fullscreen/windowable-VIO/PM;
- supported PgmControl bit combinations;
- Reserved must be zero when present;
- TermQ remains unsupported when non-empty;
- PgmHandle remains unsupported when non-zero.

The backend receives only the normalized request.  WHP/OS2SS adapters must validate/copy guest/client strings before constructing that request; common code does not treat a guest address as a host pointer.

## Launch semantics

Common code allocates the OS/2 session id before asking the backend to launch, preserving the existing gap-on-launch-failure behavior.

For a related child:

1. backend launches suspended;
2. common records the child in the local ownership table;
3. common records the shared session metadata;
4. backend resumes the initial thread.

For an independent session, the backend launch handles are released immediately and the session is not entered in the owner's related-session registry, preserving existing native behavior.

## Stop semantics

`DosStopSession` remains an owner-local operation over related sessions started by the current personality instance.  Common code selects the session(s), invokes backend termination, waits, removes shared metadata and releases opaque native handles.

The Win32 backend preserves the current proof-level behavior: terminate a Job Object when available, otherwise terminate the process.  A later OS2SS backend may implement cooperative session-close policy without changing the common API surface.

## Title and query semantics

`DosSmSetTitle` and private `O2HostQuerySessions` now use common registry policy.

A process may update its own record, or the owner may update its child.  Stale child/owner records are reaped before title/query operations using backend liveness checks.

`O2HostQuerySessions` remains a private CMD32 extension and returns only sessions owned by the current process identity, exactly as before.

## Win32-only mappings preserved

The backend still translates normalized STARTDATA to the current host behavior:

- fullscreen/default/windowable VIO -> new console;
- PM -> detached process;
- background VIO -> `SW_SHOWNOACTIVATE` when no explicit show state exists;
- invisible/minimize/maximize and position/size -> `STARTUPINFOA`;
- related child -> `CREATE_SUSPENDED` plus Job Object;
- custom environment -> validated and case-insensitively sorted Win32 environment block;
- `OS2HOST32_LOADER` -> host loader override.

These are backend mechanics, not common OS/2 semantics.

## Deliberately unchanged limitations

R2 does not expand SESMGR functionality.  Existing approximations remain:

- termination queues are unsupported;
- install-database PgmHandle is unsupported;
- StopSession is forced termination rather than cooperative close;
- `NOAUTOCLOSE` is accepted but has no additional host behavior;
- full-screen VIO is represented by a normal new Win32 console;
- icon binding is recorded only in diagnostics;
- InheritOpt validation exists, but native environment inheritance remains the existing implementation;
- only the existing three historical exports plus private ordinal 1000 are present.

## Future backends

### WHP

A WHP backend can use runtime guest-session objects as `Os2SesmgrNative` tokens, implement process identity from guest personality IDs, store the registry in runtime memory, and let its scheduler create/resume/stop guest sessions.  It does not need Win32 Job Objects or a shared mapping.

### ReactOS OS2SS

OS2SS can keep `Os2SesmgrRegistry` server-resident under an OS2SS lock, create subsystem-5 vessels through its process/session mechanism, and use server-side process records as native tokens.  The common STARTDATA and ownership rules remain unchanged.
