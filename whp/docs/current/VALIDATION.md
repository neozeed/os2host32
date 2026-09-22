# Milestone 3 validation status

## Passed in the packaging environment

- `make verify`
- GCC strict-C89 common-core build and execution
- Clang strict-C89 common-core build and execution
- GCC AddressSanitizer/UndefinedBehaviorSanitizer common-core execution
- GCC and Clang host builds of `le2pe386` with the ordinal catalogue
- transformation of `whp/fixtures/hi.exe` with both host builds; the import
  summary resolved all eight DOSCALLS ordinals by API name
- Python syntax checks for catalogue and wiring validators
- complete `make -n all` and `make -n whp` dependency dry runs
- whitespace/error check with `git diff --check`

## Not available in the packaging environment

The environment does not contain:

- `i686-w64-mingw32-gcc`;
- Microsoft `cl`/linker;
- Windows SDK or `WinHvPlatform.h`;
- a Windows Hypervisor Platform runtime.

Therefore this package does not claim that fresh Windows binaries were linked or
that the guest regressions were rerun after refactoring.  Perform the Windows
validation sequence in `NEXT-CHAT-HANDOFF.md` before declaring runtime parity.
