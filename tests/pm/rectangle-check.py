"""Compile the actual rectangle implementations with a small native-DC mock.

This checks coordinate/color routing, not Windows GDI or an OS/2 live run.
Run: python3 tests/pm/rectangle-check.py
"""
import pathlib
import re
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
functions = []
for source, names in [
    ("dlls/pmwin/pmwin.c", ["pm_ps_height", "WinFillRect"]),
    ("dlls/pmgpi/pmgpi.c", ["gpi_surface_height", "GpiBox"]),
]:
    text = (root / source).read_text()
    for name in names:
        match = re.search(r"^[^\n]*\b" + name + r"\([^;{]*\{.*?^\}", text, re.M | re.S)
        if not match:
            raise RuntimeError("Cannot extract " + name)
        functions.append(match.group(0))
with tempfile.TemporaryDirectory(prefix="pm-rectangle-check-") as tmp:
    include = pathlib.Path(tmp) / "rectangle-functions.inc"
    include.write_text("\n\n".join(functions))
    binary = str(pathlib.Path(tmp) / "rectangle-check")
    subprocess.run(["cc", "-std=c89", "-pedantic-errors", "-I" + tmp,
                    str(root / "tests/pm/rectangle-check.c"), "-o", binary], check=True)
    subprocess.run([binary], check=True)
