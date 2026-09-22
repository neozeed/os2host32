# M31D R5 - BIO client-size + overlapping resource object fix

R5 fixes two independent compatibility bugs exposed by BIO:

1. `pm_wndproc` now translates native `WM_SIZE` to OS/2 `WM_SIZE` for ordinary
   PM client windows. BIO initializes `rclClient` and `LinesPerPage` only from
   `WM_SIZE`; without it the chart paint code computes from a zero-sized client
   and only the white erase is visible.

2. The loader now preserves an immutable per-object shadow for any LE object
   referenced by the resource table. Old OS/2 linkers may give several
   resource-only objects the same preferred base (BIO has objects 3, 4, and 5
   all at base zero). The direct mapping arena intentionally aliases those
   objects, so later resource objects can overwrite earlier resource bytes.
   PMWIN now receives each resource from its correct preserved object image.
   This should restore BIO's menu and icon while keeping the accelerator,
   dialogs, and string table intact.

R5 also adds focused tracing for `GpiCharStringAt` under `OS2_PM_TRACE=1` so a
remaining Legend/text issue can be distinguished from geometry immediately.
