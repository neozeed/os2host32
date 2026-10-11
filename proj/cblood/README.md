# R32: same-planet passenger return and varied fallback static

The RELEASE teleport eye follows the original 2C66 onboard check. R31 and
earlier UI code incorrectly required active_record to be empty; selecting a
passenger's old planet finds that passenger's record even while aboard.
Core DOS selection and transport routines remain unchanged.

# Captain Blood EGA — R32: DOS transitions

R32 builds on the frozen R30 speaker checkpoint. It replaces the invented
screen fade, direct title flips and direct passenger appearance/removal with
native C translations of the original DOS routines. The SDL game does not
load or execute x86 code. Python and the test-only CPU are verification tools,
not build/play dependencies.

## Build and play

From the extracted source root, run `BUILD-SDL2-R32.cmd`, then
`RUN-SDL2-R32.cmd`. The MinGW GCC / SDL2 requirements are unchanged. Alternatively:

```sh
make -f r32/Makefile
./blood-sdl-r32 r20/assets
```

The game saves to `BLOOD-R32.GAM`. To continue an R30 game, copy
`BLOOD-R30.GAM` to `BLOOD-R32.GAM` in the working directory. The original disk
rule remains: during the first five game minutes the disk eye loads; later it
saves. F9 mutes the PC speaker; F12 opens the paused debugger. F8 assistance
remains an explicit testing option.

## Original routines translated

Offsets below are B2 executable file offsets, not CS-relative addresses.

- `28A7`: white XOR triples at radii 10/12/14, 30/32/34, 50/52/54,
  70/72/74 and 90/92/94, each erased before the next triple.
- `2769`, `26E6`, `2745`: four random walks along both edge lines, retaining
  the original three-row clearing and all 2560 RNG calls.
- `2609`: grey edge lines between blue neighbour rows. Globe/hyperspace
  callers use y=18/147; galaxy uses y=18/149.
- Caller routing follows `58D0`, `6E44`, `2A62`: bridge-to-globe,
  bridge-to-galaxy and galaxy-to-hyperspace. Scan toggles, returning to the
  globe from detail, returning to the bridge, arrival and completed landscape
  do not acquire a generic circle fade.
- `4AB4`: exchange both title pages one pixel at a time, in the original
  `CX += 149 mod 64000` order. Both directions and both resulting pages match.
  Original title holds remain seven/six seconds (`JA 6` / `JA 5`).
- `6CE1`: 100 groups of 200 random byte trials, with the original four-plane
  comparison, conditional RNG calls, random noise and occasional target copy.
  Final `539A` byte alignment copies x=40..119, y=40..109. Teleport-up and
  disintegration show this dissolve, including the pickup before the ending.
- `754C`: original generated source, all 24 noise offsets, four-plane EGA
  colours and original sound 1. R30's red recolouring is removed.
- Wall hits display the native terrain/HUD frame and stop speed through the
  original terrain routine. The invented red overlay and one-second freeze
  are removed. Both wall damage and fatal missile damage enter `754C`.

Circle/edge waits, flight updates and failed-flight waits use the original
12-IRQ service period: PIT divisor 5970 at 1193182 Hz, about 60.041 ms.
They are independent of the SDL rendering frame rate and pause with F12.

## Failed-flight environment dependency

The full original failure routine exposes an unusual dependency: `55A2`
clobbers BP to 8. `754C` then uses BP as its source segment again, so only the
first frame reads generated terrain noise. Frames 2–24 read DOS segment
`0008`, physical low memory starting at `00080`.

R32 preserves that source change. The game files do not include that machine
memory. Without a sample, R32 generates fresh synthetic static for each frame,
using an independent PRNG seeded from SDL time. This fallback is **not the
exact pattern of your DOS installation**, and does not alter gameplay RNG.
A supplied sample remains unchanged throughout playback.
The included `r32/dos-tools/DOSLOW.COM` can capture a sample from your DOS
setup; copy its `DOSLOW.BIN` into `r20/assets`. For exact comparison, a debugger
sample taken while the original game is running is preferable. See
`r32/dos-tools/README.md` for the byte range and instructions. No fabricated
historical DOS memory image is supplied.

## R32 validation

SDL dummy video/audio integration passed with UndefinedBehaviorSanitizer and
-Werror, including same-planet pickup/return/conversation, different-planet
release, save/load, disintegration, and existing transition tests. Fallback
checks verify varied bytes, unchanged gameplay RNG and untouched supplied
memory. R31 differential results below are inherited; the DOS transition
primitives are unchanged. Windows live execution remains unavailable.

## Inherited R31 verification

`r32/tests/test_transitions.py` executes the complete original drawing,
page-exchange, byte-comparison, noise and RNG instructions using the unchanged
Tiny386 CPU and a test-only four-plane EGA device. Graphics routines are not
hooked. Only BIOS/service waits and the already-verified sound cues are
supplied by the test host.

- 72 circle/edge frames: four seeds, both caller geometries, full pixels/RNG.
- 80 title frames: both directions, exact intermediate pixels and both pages.
- 900 portrait frames: appearance, disappearance and unchanged bytes, three
  seeds; complete final rectangle and page synchronization checked too.
- 72 failed-flight frames: three seeds and supplied low-memory images; exact
  source initialization, source-segment change, pixels and final RNG.
- Five DOS sampler cases: success, create/write/short-write/close failures.
- Retained effects: 14 arrival stages, two complete warp sequences, 456 star
  frames, 960 circles, 400 lines and 60 planet-destruction band frames.
- All 15 speaker programs: 63776 IRQ comparisons and 99039 PIT/port writes.
- Actual SDL2 dummy video/audio integration with UndefinedBehaviorSanitizer:
  navigation, dialog, passengers, ending, debugger, speaker, timed transition
  routing, title swap, teleport/disintegration and collision/failure lifecycle.

Commands (Linux verification environment):

```sh
python3 r32/tests/test_transitions.py
python3 r32/tests/test_effects.py
python3 r32/tests/test_speaker.py
make -f r32/Makefile check CFLAGS='-std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined'
```

## Remaining boundaries

`4AB4` and `6CE1` contain no explicit IRQ waits. Their exact pixel/RNG order
is retained, but SDL presents batches (1600 title pixels or 1000 portrait
trials per rendering frame). Their wall-clock duration is a host pacing
choice, not a reconstructed historical CPU speed.

This does not establish a full end-to-end DOS-equivalent playthrough or
cycle-identical scheduling of all UI/alien routines. Existing R30 boundaries
remain, including fixed new-game seeds, unchanged flight hand degradation,
and the documented PE malformed-pose behaviour. Windows live execution was
unavailable here; the original Windows build route is retained.

Previous checkpoints, original assets and gameplay `src` modules are preserved.
