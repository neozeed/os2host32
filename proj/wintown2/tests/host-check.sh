#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/check
CC=${CC:-gcc}
FLAGS="-std=c89 -pedantic-errors -fcommon -g -DOS2_PM -DMP_PORT_TEST -DMP_SELFTEST -I. -Ios2pm ${CHECK_FLAGS:-}"
for name in sim anim animtab budget eval disastr tiles sprite tools notify charts scenario; do
    if [ "$name" = sim ]; then
        # Force the Microsoft CRT's 15-bit output range even on a host whose
        # rand() returns 31 bits. Other files retain the normal host rand().
        $CC $FLAGS -Drand=mp_test_rand15 -c "$name.c" -o "build/check/$name.o"
    else
        $CC $FLAGS -c "$name.c" -o "build/check/$name.o"
    fi
done
for name in s_disast s_eval s_gen s_power s_scan s_traf s_zone; do
    $CC $FLAGS -c "micropolis/src/sim/$name.c" -o "build/check/$name.o"
done
for name in s_sim_pm os2pm_main os2bmp pm_bridge cityio; do
    $CC $FLAGS -c "os2pm/$name.c" -o "build/check/$name.o"
done
$CC $FLAGS -c tests/pm_core_test.c -o build/check/test.o
$CC ${CHECK_FLAGS:-} build/check/*.o -o build/check/pm-core-test
build/check/pm-core-test
# Check PM-only declarations against the real 1990 headers. This does not
# validate the 32-bit calling convention or the target compiler/linker.
$CC -std=c89 -pedantic-errors -Wno-pointer-sign -Wno-pointer-to-int-cast \
    -D_syscall= -Dcdecl= -DOS2_PM -isystem include/os2h -I. -Ios2pm \
    -fsyntax-only os2pm/*.c sim.c anim.c animtab.c budget.c eval.c \
    disastr.c tiles.c sprite.c tools.c notify.c charts.c scenario.c \
    micropolis/src/sim/s_disast.c micropolis/src/sim/s_eval.c \
    micropolis/src/sim/s_gen.c micropolis/src/sim/s_power.c \
    micropolis/src/sim/s_scan.c micropolis/src/sim/s_traf.c micropolis/src/sim/s_zone.c

$CC -std=c89 -pedantic-errors ${CHECK_FLAGS:-} tests/pm_scroll_test.c -o build/check/pm-scroll-test
build/check/pm-scroll-test

$CC -std=c89 -pedantic-errors ${CHECK_FLAGS:-} tests/pm_minimap_test.c -o build/check/pm-minimap-test
build/check/pm-minimap-test
