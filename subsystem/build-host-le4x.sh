#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
OUT=${1:-"$ROOT/evidence/host-oracle"}
IMAGE=${IMAGE:-"$ROOT/fixtures/hi.exe"}
mkdir -p "$OUT"
$CC -std=c99 -O2 -Wall -Wextra -Werror \
  "$ROOT/tools/le4x_oracle.c" \
  "$ROOT/common/os2loader.c" "$ROOT/common/os2image.c" \
  "$ROOT/common/os2veneer.c" "$ROOT/common/os2startup.c" "$ROOT/common/os2sha256.c" \
  -o "$OUT/le4x_oracle"
"$OUT/le4x_oracle" "$IMAGE" "$OUT"
