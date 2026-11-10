#!/usr/bin/env bash
# Build and run the engine tests with the computer's own C compiler, with
# AddressSanitizer and UBSan.
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$HERE/build/host"
mkdir -p "$OUT"
"${CC:-cc}" -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all \
    "$HERE/tests/test_engine.c" "$HERE"/engine/*.c -lm -o "$OUT/test_engine"
"$OUT/test_engine"
