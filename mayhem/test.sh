#!/usr/bin/env bash
#
# mayhem/test.sh — runs the MilkyPlay known-answer selftest that mayhem/build.sh built (no compiling here).
#
# /mayhem/milkyplay_selftest loads the shipped demo tunes (resources/music/*.xm), three hand-built seed
# modules (MOD/S3M/IT), an XI instrument, a hand-built WAV sample and a gzip stream, and prints what the
# MilkyPlay loaders, mixer and decompressors produce: titles, channel/order/pattern counts, sample-data
# hashes, decoded sample values, rendered-audio hashes and the decompressed bytes. Each printed line is
# compared with the exact expected line below. The comparison runs in bash (not in the binary), so a
# patch that makes the selftest exit 0 without doing the work fails: its lines never appear.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

SELFTEST=/mayhem/milkyplay_selftest
if [ ! -x "$SELFTEST" ]; then
  echo "missing $SELFTEST — mayhem/build.sh should have built it" >&2
  emit_ctrf "milkyplay-kat" 0 1 0
  exit 1
fi

EXPECTED=$(cat <<'EXPECTED_EOF'
module milky.xm load=0
module milky.xm name=[milk in veins] channels=10 orders=17 patterns=17 instruments=7 samples=112 tempo=6 speed=133
module milky.xm sampledata bytes=10185 fnv=1c2f5f8920bd83a9
module milky.xm render frames=59392 fnv=d6c32f137f978fd0
module slumberjack.xm load=0
module slumberjack.xm name=[slumberjack         ] channels=8 orders=42 patterns=27 instruments=7 samples=112 tempo=3 speed=84
module slumberjack.xm sampledata bytes=9099 fnv=668761cc48c444d4
module slumberjack.xm render frames=13312 fnv=275507ee15f93c4a
module sv_ttt.xm load=0
module sv_ttt.xm name=[The Titan Turrican] channels=6 orders=16 patterns=17 instruments=44 samples=704 tempo=4 speed=135
module sv_ttt.xm sampledata bytes=10871 fnv=93263e342ce1b286
module sv_ttt.xm render frames=39936 fnv=6c62dfd090e06675
module theday.xm load=0
module theday.xm name=[the day they landed] channels=8 orders=45 patterns=42 instruments=7 samples=112 tempo=8 speed=160
module theday.xm sampledata bytes=28466 fnv=82c0e25542c1eb7d
module theday.xm render frames=53248 fnv=d992ad7b67d24632
module universalnetwork2_real.xm load=0
module universalnetwork2_real.xm name=[ universal network 2] channels=6 orders=31 patterns=32 instruments=16 samples=256 tempo=3 speed=125
module universalnetwork2_real.xm sampledata bytes=67377 fnv=0082c2b69fce5139
module universalnetwork2_real.xm render frames=32768 fnv=e43f1a1465892731
module kat.mod load=0
module kat.mod name=[mayhem kat] channels=4 orders=1 patterns=1 instruments=31 samples=1 tempo=6 speed=125
module kat.mod sampledata bytes=32 fnv=4d88db9a13d08f83
module kat.mod render frames=62464 fnv=94a162f9f9e0da0b
module kat.s3m load=0
module kat.s3m name=[mayhem s3m] channels=2 orders=1 patterns=1 instruments=1 samples=1 tempo=6 speed=125
module kat.s3m sampledata bytes=16 fnv=7a56c4ada130bac3
module kat.s3m render frames=62464 fnv=e8cf6f273c9be383
module kat.it load=0
module kat.it name=[mayhem it] channels=1 orders=1 patterns=1 instruments=1 samples=1 tempo=6 speed=125
module kat.it sampledata bytes=16 fnv=7a622f5c5c7f2fc3
module kat.it render frames=62464 fnv=1e3681822593a5a3
xi kat.xi load=0
xi kat.xi name=[mayhem xi] samples=1 len=16 vol=255 volfade=512
xi kat.xi data=4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,64
mod load=0
mod name=[mayhem kat] channels=4 orders=1 patterns=1 instruments=31 samples=1 tempo=6 speed=125
mod sampledata bytes=32 fnv=4d88db9a13d08f83
mod cell0 note=49 ins=1
mod sample0 len=32 vol=255
mod render frames=62464 fnv=ca10f69bd9c7b81b
wav identify=1 channels=1
wav load=0 len=8 type=0
wav data=0,32,64,96,-128,-96,-64,-32
wav save=0 size=52
decompress junk identify=0
decompress gzip identify=1 result=1 out=[milky|]
EXPECTED_EOF
)

ACTUAL=$("$SELFTEST" resources/music/*.xm mayhem/fuzz_module/testsuite/kat.mod mayhem/fuzz_module/testsuite/kat.s3m \
         mayhem/fuzz_module/testsuite/kat.it mayhem/fuzz_instrument/testsuite/kat.xi 2>&1)
rc=$?

passed=0 failed=0
while IFS= read -r line; do
  if printf '%s\n' "$ACTUAL" | grep -Fxq -- "$line"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
    echo "MISSING: $line" >&2
  fi
done <<< "$EXPECTED"

if [ "$rc" -ne 0 ]; then
  echo "selftest exited with status $rc" >&2
  failed=$((failed + 1))
fi

emit_ctrf "milkyplay-kat" "$passed" "$failed" 0
