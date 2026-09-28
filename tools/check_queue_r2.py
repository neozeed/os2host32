#!/usr/bin/env python3
"""Static architecture checks for the QUECALLS R2 split."""
from pathlib import Path
import hashlib
import re
import sys

root = Path(__file__).resolve().parents[1]
veneer = (root / "dlls/quecalls/quecalls.c").read_text()
def_bytes = (root / "dlls/quecalls/quecalls.def").read_bytes()
common_h = (root / "common/include/os2_queue.h").read_text()
backend_h = (root / "common/include/os2_queue_backend.h").read_text()
common_c = (root / "common/queue/os2_queue.c").read_text()
win32_c = (root / "common/win32/os2_queue_win32.c").read_text()
makefile = (root / "Makefile").read_text()
catalog = (root / "common/api/os2_api_catalog.inc").read_text()

errors = []

def need(condition, message):
    if not condition:
        errors.append(message)

need(hashlib.sha256(def_bytes).hexdigest() ==
     "edccc8b6d80c1076dfdf637230f3575d3ed8a98791ad5617933001ebfafcbd75",
     "QUECALLS .def changed; four historical export ordinals must remain frozen")

expected = {
    "DosReadQueue": 9,
    "DosWriteQueue": 14,
    "DosOpenQueue": 15,
    "DosCreateQueue": 16,
}
def_text = def_bytes.decode("ascii")
for name, ordinal in expected.items():
    need(re.search(r"^\s*%s\s+@%d\s+NONAME\s*$" % (name, ordinal),
                   def_text, re.MULTILINE) is not None,
         "%s @%d missing from QUECALLS.def" % (name, ordinal))
    need(re.search(r"\b%s\s*\(" % name, veneer) is not None,
         "%s missing from ABI veneer" % name)
    need(('OS2_API("QUECALLS", %du, "%s"' % (ordinal, name)) in catalog,
         "%s @%d missing from API catalogue" % (name, ordinal))

need("windows.h" not in veneer.lower(), "QUECALLS ABI veneer includes windows.h")
for token in ("CRITICAL_SECTION", "CreateEventA", "WaitForSingleObject",
              "GetCurrentProcessId", "static struct O2Queue"):
    need(token not in veneer,
         "QUECALLS ABI veneer still owns Win32/queue implementation: %s" % token)
need("os2_queue_win32_session()" in veneer,
     "QUECALLS veneer is not bound through the backend-neutral queue session")
need("(O2ULONG)(uintptr_t)pData" in veneer and
     "(void *)(uintptr_t)data_value" in veneer,
     "native pointer/value conversion is not isolated at the ABI veneer")

for token in ("struct Os2QueueSession", "struct Os2QueueObject",
              "struct Os2QueueEntry", "owner_pid", "discipline",
              "data_value", "availability"):
    need(token in common_h, "common queue state missing %s" % token)
need("os2_queue_try_read" in common_h and "os2_queue_try_read" in common_c,
     "nonblocking scheduler-facing common queue primitive is missing")
need("OS2_QUEUE_FIFO" in common_h and "OS2_QUEUE_LIFO" in common_h and
     "OS2_QUEUE_PRIORITY" in common_h and "OS2_QUEUE_CONVERT_ADDRESS" in common_h,
     "queue discipline constants are incomplete")
need("insert_position" in common_c and "OS2_QUEUE_PRIORITY" in common_c,
     "FIFO/LIFO/priority ordering is not common-owned")
need("queue_name_equal" in common_c and "queue_name_valid" in common_c,
     "queue name semantics are not common-owned")
need("#include <windows.h>" not in common_h and
     "#include <windows.h>" not in common_c,
     "common QUECALLS layer has a Win32 dependency")
need("void *data" not in common_h,
     "common queue entries expose host pointers instead of opaque 32-bit values")

for token in ("lock", "unlock", "current_pid", "create_availability",
              "destroy_availability", "set_available", "wait_available"):
    need(token in backend_h, "queue backend contract missing %s" % token)

for token in ("#include <windows.h>", "CRITICAL_SECTION", "CreateEventA",
              "SetEvent", "ResetEvent", "WaitForSingleObject",
              "GetCurrentProcessId"):
    need(token in win32_c, "Win32 queue backend missing %s" % token)
need("TRUE, FALSE" in win32_c,
     "Win32 queue availability is not a manual-reset event")
need("struct Os2QueueObject" not in win32_c,
     "Win32 backend duplicates common queue object state")

for token in ("QUEUE_COMMON_SRC = common/queue/os2_queue.c",
              "QUEUE_WIN32_SRC = common/win32/os2_queue_win32.c",
              "queue-check", "queue-veneer-check", "queue-win32-shim-check",
              "queue-static-check"):
    need(token in makefile, "Makefile missing QUECALLS R2 wiring: %s" % token)

if errors:
    for error in errors:
        print("QUEUE-R2 CHECK FAIL: " + error, file=sys.stderr)
    sys.exit(1)
print("QUEUE-R2 static architecture check: PASS (4 frozen exports)")
