# OS2HOST32 NLS R2 backend design

## Goal

Make OS/2 National Language Support process/session state independent of the
execution backend, while preserving the existing native ABI and the existing
CP437/CP850 behavior.

NLS R2 is deliberately a **state ownership refactor**, not an NLS API expansion.

## Resulting architecture

```text
Native OS/2 application
    -> DOSCALLS.DLL / NLS.DLL veneer
    -> Os2DosSession.nls
    -> common/nls/os2_nls.c

WHP guest
    -> shared personality NLS API
    -> Runtime.nls_state
    -> common/nls/os2_nls.c

Future ReactOS OS2SS
    -> OS2_PROCESS NLS state
    -> common/nls/os2_nls.c
```

The common NLS state is authoritative after initialization.

## Common-owned state

`struct Os2NlsState` owns:

- current OS/2 country;
- current process code page;
- prepared code-page list;
- backend binding used only for initialization/bootstrap;
- selection of the built-in OS/2 country/code-page data used for case mapping
  and DBCS queries.

The built-in immutable data remains in the common NLS registry/tables:

- country 1 (US);
- country 44 (UK);
- CP437;
- CP850;
- upper/lower case tables;
- DBCS lead-byte tables for the implemented code pages.

No process-visible NLS state remains in `os2_doscalls_win32.c`.

## Backend contract

NLS R2 intentionally has only one backend operation:

```c
int (*query_initial_profile)(void *opaque,
                             uint32_t *country,
                             uint32_t *codepage);
```

This is a bootstrap hint, not an authoritative locale service.

The common layer validates the suggested country and code page against its own
implemented OS/2 registry.  Unsupported host values do not become guest-visible
state.  With no backend, or with unsupported host defaults, deterministic
OS/2 defaults remain country 1 / CP437.

Future resource/path lookup or host conversion services should be added only
when an implemented NLS API actually requires them.  R2 does not add speculative
callbacks.

## Win32 backend

`common/win32/os2_nls_win32.c` implements the bootstrap operation with:

- `GetLocaleInfoA(... LOCALE_ICOUNTRY | LOCALE_RETURN_NUMBER ...)`;
- `GetOEMCP()`.

It does **not** implement guest-visible formatting, code-page selection, case
mapping, DBCS policy, COUNTRYINFO packing, or prepared-codepage state.

Those remain common semantics.

## Native DOSCALLS ownership

`struct Os2DosSession` now embeds:

```c
struct Os2NlsState nls;
```

The native DOSCALLS NLS entrypoints no longer traverse the transitional Win32
dispatch seam:

- `DosSetProcessCp` (DOSCALLS.289)
- `DosQueryCp` (DOSCALLS.291)
- `DosQueryCtryInfo` (DOSCALLS.395)
- `DosQueryDBCSEnv` (DOSCALLS.396)
- `DosMapCase` (DOSCALLS.397)

They operate directly on `session->nls`.

`NLS.DLL` remains the existing three-export ABI veneer.  Its private imports from
DOSCALLS still reach the same `Os2DosSession.nls`, so a `DosSetProcessCp` change
is immediately observed through both DOSCALLS and NLS entrypoints.

## WHP

WHP now calls:

```c
os2_nls_win32_init_session(&rt.nls_state);
```

rather than routing NLS bootstrap through generic `os2_win32_services`.

This preserves current Win32-hosted WHP behavior while making the backend
selection explicit.  A future WHP-specific policy can replace that initializer
without changing common NLS semantics or guest API code.

## Future OS2SS

A ReactOS OS2SS process record can embed the same state directly, conceptually:

```c
struct OS2_PROCESS {
    ...
    struct Os2NlsState nls;
};
```

OS2SS may initialize it with deterministic OS/2 defaults, a subsystem policy,
or a future OS2SS resource backend.  It does not need Win32 locale globals.

## Explicit non-goals

NLS R2 does not add:

- `DosQueryCollate` / historical `DosGetCollate`;
- new countries or code pages;
- COUNTRY.SYS parsing;
- filesystem/path-based NLS resource loading;
- Unicode conversion APIs;
- process-global Win32 locale mutation;
- new NLS.DLL or DOSCALLS exports.

Those can be separate milestones built on this state/backend boundary.
