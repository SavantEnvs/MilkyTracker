#!/usr/bin/env bash
#
# mayhem/build.sh — builds the MilkyPlay fuzz harnesses, their standalone reproducers and the oracle
# selftest, all offline from the in-image toolchain and apt packages (zlib, lhasa, zziplib).
set -euo pipefail

# clang rejects SOURCE_DATE_EPOCH='' (empty) — it must be unset or a valid integer.
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

# Build knobs come from the ENVIRONMENT (overridable), with sane defaults — no if-statements,
# just parameter-expansion fallbacks. Default sanitizers are the base's ASan+UBSan (halting); override
# per build via the Dockerfile's `--build-arg SANITIZER_FLAGS="..."`.
# NB: SANITIZER_FLAGS uses `=` (no colon) on purpose — `=` only fills when the var is UNSET, so an
# explicit EMPTY value (`--build-arg SANITIZER_FLAGS=`) is honored and builds with NO sanitizers
# (useful when you want the program's natural crash / full backtrace, not an ASan report). The other
# knobs use `:=` (default on empty too). MAYHEM_JOBS sets build parallelism (falls back to nproc).
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
# DEBUG_FLAGS carries DWARF debug info INDEPENDENTLY of the sanitizer off-switch (so an empty
# SANITIZER_FLAGS still yields DWARF symbols). DWARF MUST be < 4 (Mayhem triage can't read >=4); clang-19's
# plain `-g` emits DWARF-5, so `-gdwarf-3` is explicit. Apply $DEBUG_FLAGS to the fuzz/harness/standalone
# builds (NOT the test/oracle build). Rust/Go carry the same intent via their language flags below.
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${RUST_DEBUG_FLAGS:=-C debuginfo=2 -C force-frame-pointers=yes -C llvm-args=-gdwarf-3}"
: "${GO_DEBUG_FLAGS:=-gcflags=all=-N -l}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
# COVERAGE_FLAGS: empty by default → no effect on the normal oracle build. Set it via the Dockerfile's
# `--build-arg COVERAGE_FLAGS="-fprofile-instr-generate -fcoverage-mapping"` to instrument the TEST
# build for source-coverage measurement (how much of the project the test suite actually exercises —
# a quality signal for the oracle; complements the anti-reward-hack sabotage check). APPEND it to the
# test build's compile+link flags in step 3 (NOT the fuzz build); after `test.sh` runs, merge with
# `llvm-profdata` and report with `llvm-cov`. Empty value is honored (`=`, not `:=`).
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS RUST_DEBUG_FLAGS GO_DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

# Upstream's full build needs SDL2 and the whole GUI; the fuzzed surface (the MilkyPlay module/sample/
# instrument loaders and the compression front-ends) is compiled directly, as upstream's CMake lists it.
GEN="$SRC/build/gen"
mkdir -p "$GEN/tracker"
sed -e 's/${VER_X}/1/;s/${VER_YY}/05/;s/${VER_ZZ}/01/' src/tracker/version.h.in > "$GEN/tracker/version.h"

INC="-Isrc/milkyplay -I$GEN/tracker -Isrc/ppui -Isrc/ppui/osinterface -Isrc/ppui/osinterface/posix -Isrc/compression -Isrc/compression/zziplib -I/usr/include/liblhasa-1.0 -Imayhem -Imayhem/stubs"
DEFS="-DMILKYTRACKER -DDRIVER_UNIX -DSYNTH"

SOURCES=$(ls src/milkyplay/*.cpp)
SOURCES="$SOURCES src/ppui/osinterface/posix/PPSystem_POSIX.cpp src/ppui/osinterface/posix/PPPath_POSIX.cpp"
SOURCES="$SOURCES src/ppui/osinterface/posix/PPMutex.cpp src/ppui/osinterface/PPPathFactory.cpp"
SOURCES="$SOURCES src/ppui/Dictionary.cpp src/ppui/DictionaryKey.cpp"
SOURCES="$SOURCES $(ls src/compression/*.cpp)"
SOURCES="$SOURCES src/compression/zziplib/MyIO.cpp"

compile_one() { # <outdir> <src> <flags...>
  local out="$1" src="$2"; shift 2
  "$CXX" "$@" $INC $DEFS -std=c++11 -w -c "$src" -o "$out/$(echo "$src" | tr / _).o"
}
export -f compile_one
export INC DEFS CXX

build_set() { # <outdir> <flags...>
  local out="$1"; shift
  rm -rf "$out"; mkdir -p "$out"
  printf '%s\n' $SOURCES | xargs -P "$MAYHEM_JOBS" -I{} bash -c 'compile_one "$@"' _ "$out" {} "$@"
}

# 1) Fuzz objects: the whole library instrumented with the sanitizers, SanCov and DWARF 3.
FUZZ_OBJ="$SRC/build/fuzz-obj"
build_set "$FUZZ_OBJ" $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$FUZZ_OBJ/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$FUZZ_OBJ/standalone_main.o"

LIBS="-lz -llhasa -lzzip"
LIB_OBJS=$(ls "$FUZZ_OBJ"/src_*.o)
for t in fuzz_module fuzz_render fuzz_sample fuzz_instrument fuzz_decompress; do
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link $INC $DEFS -std=c++11 -w \
      -c "mayhem/$t.cc" -o "$FUZZ_OBJ/$t.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$FUZZ_OBJ/$t.o" $LIB_OBJS "$FUZZ_OBJ/lsan_off.o" $LIBS -o "/mayhem/$t"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$FUZZ_OBJ/$t.o" "$FUZZ_OBJ/standalone_main.o" $LIB_OBJS "$FUZZ_OBJ/lsan_off.o" $LIBS -o "/mayhem/$t-standalone"
done

# 2) Oracle selftest: a separate, normal-flags (no sanitizers) build of the same library, driven by
#    mayhem/selftest.cc; dynamically linked so the sabotage shim's neutering of it is detectable.
TEST_OBJ="$SRC/build/test-obj"
build_set "$TEST_OBJ" -O2 -g0 $COVERAGE_FLAGS
$CXX -O2 $COVERAGE_FLAGS $INC $DEFS -std=c++11 -w mayhem/selftest.cc $(ls "$TEST_OBJ"/src_*.o) $LIBS -o /mayhem/milkyplay_selftest
file /mayhem/milkyplay_selftest | grep -q 'dynamically linked'
