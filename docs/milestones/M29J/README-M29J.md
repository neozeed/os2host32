# M29J - run the reconstructed CMD itself as C/386 OS/2

M29I proved `DosFindFirst` / `DosFindNext` through the real OS/2 backend and
left the seven C/386 VIO/KBD migration bridges stable under the IBM/Microsoft
LX linker.

M29J deliberately stops adding isolated API probes.  The existing
`cmd32os2.c` already contains the reconstructed command loop: prompt and line
editing, parser/dispatch, DIR, TYPE, CD/CHDIR, MD/MKDIR, RD/RMDIR, SET, ECHO,
PAUSE, CLS, EXIT, file commands, batch machinery, redirection/pipes and the
external-program boundary.

The new `build-os2-shell.cmd` compiles that command processor and its recovered
modules with Microsoft C/386 and links them against `cmdos2_os2.c`, LIBC.LIB
and OS2386.LIB.  `cmd32os2_os2.def` carries the same seven proven far16
VIO/KBD imports as the frozen M29I backend.

## Why this is a probe rather than a claimed pass

The full shell uses substantially more of Microsoft C's runtime than the
backend regression programs.  The next build/link/runtime failure is useful:
it identifies a real dependency of the reconstructed CMD rather than an API
chosen in advance.

## Build

    build-os2-shell.cmd

If it links, inspect it first:

    os2host32 --scan cmd32os2_os2.exe

The seven far16 imports should remain recognizable.

Then exercise the least complicated full-shell path:

    os2host32 --run cmd32os2_os2.exe -c "echo M29J_SHELL_OK"

Useful follow-ups, one at a time:

    os2host32 --run cmd32os2_os2.exe -c "dir"
    os2host32 --run cmd32os2_os2.exe -c "ver"
    os2host32 --run cmd32os2_os2.exe -c "set"

Finally run interactively:

    os2host32 --run cmd32os2_os2.exe

The existing command loop should print its bootstrap banner and prompt, accept
KBD input through KbdCharIn, echo via VioWrtTTY, and return to the prompt after
built-ins.  `CLS` uses the M29H VIO path and `DIR` uses the M29I find path.

Do not modify OS2HOST32 merely to make this executable link: M29I is the frozen
loader/backend baseline for this milestone.
