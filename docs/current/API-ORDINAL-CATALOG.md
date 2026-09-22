# API ordinal catalogue

The canonical catalogue is `common/api/os2_api_catalog.inc`.

Current inventory:

| Route | Entries | Meaning |
|---|---:|---|
| `SHARED` | 8 | One common implementation is used by native DOSCALLS and WHP. |
| `BACKEND` | 188 | Catalogued subsystem API with backend-specific implementation remaining. |
| `INTRINSIC` | 21 | Loader/scheduler/process operation which is intentionally execution-engine-specific. |
| `NATIVE_ONLY` | 30 | Present in the native DLL export set but not yet exposed by WHP. |
| **Total** | **247** | All catalogue entries. |

Module inventory:

| Module | Entries |
|---|---:|
| DOSCALLS | 79 |
| KBDCALLS | 6 |
| VIOCALLS | 5 |
| QUECALLS | 4 |
| SESMGR | 4 |
| NLS | 3 |
| PMWIN | 91 |
| PMGPI | 43 |
| PMSHAPI | 8 |
| PMWP | 1 |
| HELPMGR | 3 |

Run `make catalog-check` after editing a DEF file or the catalogue.  The check
rejects duplicate module/ordinal keys, malformed entries, non-DWORD-aligned known
argument sizes, missing DEF exports, and name mismatches.
