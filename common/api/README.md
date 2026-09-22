# Canonical OS/2 API catalogue

`os2_api_catalog.inc` is an X-macro table with one row per known
module/ordinal pair:

```c
OS2_API("DOSCALLS", 282u, "DosWrite", 16u, OS2_API_ROUTE_SHARED)
```

Fields are module name, ordinal, public API name, 32-bit argument bytes, and
implementation route.  Use zero argument bytes when the ABI is unknown or has
historical variants; do not guess a stack size merely to fill the field.

The table is compiled by `os2_api_catalog.c` and is also inspected by
`tools/check_api_catalog.py`.  After adding or changing a compatibility DLL
export, run:

    make catalog-check

Changing a route to `OS2_API_ROUTE_SHARED` additionally requires both the native
DLL export and WHP dispatcher to call `os2_core_<ApiName>`; `make wiring-check`
enforces that invariant.
