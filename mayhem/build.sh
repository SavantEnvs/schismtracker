#!/usr/bin/env bash
#
# mayhem/build.sh — build Schism Tracker's fuzz harnesses and its own test suite.
#
# Runs inside the commit image (mayhem/Dockerfile) as `mayhem` in /mayhem. Offline and re-runnable:
# every dependency is an apt -dev package installed by the Dockerfile; the sanitized build tree and
# the test build tree live in gitignored directories under mayhem/ (see mayhem/.gitignore).
#
#   mayhem/build-fuzz/   sanitized + SanCov-instrumented project objects, linked into each harness
#   mayhem/build-test/   the project's own `schismtrackertest` with NORMAL flags (run by test.sh)
#   /mayhem/fuzz_*       libFuzzer harnesses;  /mayhem/fuzz_*-standalone  run-once reproducers
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
FUZZ_DIR="$SRC/mayhem/build-fuzz"
TEST_DIR="$SRC/mayhem/build-test"
HARNESSES="fuzz_load_song fuzz_load_sample fuzz_load_instrument"

# The autotools outputs (configure, Makefile.in, ...) are gitignored upstream, so regenerate them
# whenever a `git clean -ffdX` has removed them.
[ -x configure ] || autoreconf -i

# 1) Sanitized project. -fsanitize=fuzzer-no-link puts SanCov on the project's own code so Mayhem sees
#    edges inside the loaders and the player, not just in the harness. LDFLAGS carries the sanitizers
#    because configure's link tests and the final `schismtracker` link both need the runtimes.
#    LeakSanitizer is on during configure's link tests only; the binaries below get lsan_off.o.
mkdir -p "$FUZZ_DIR"
(
  cd "$FUZZ_DIR"
  [ -f config.status ] || ASAN_OPTIONS=detect_leaks=0 "$SRC/configure" CC="$CC" \
      CFLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS -O1" \
      LDFLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link"
  make -j"$MAYHEM_JOBS" schismtracker
)

# 2) The objects the harnesses link: everything the `schismtracker` binary is made of, minus
#    main.c's object. main.c is compiled once more with its tiny `main` wrapper renamed so the
#    libFuzzer driver's `main` is the one that links; schism_main() and everything else in main.c
#    stay as upstream has them.
OBJS=$(cd "$FUZZ_DIR" && find . -name 'schismtracker-*.o' ! -name 'schismtracker-main.o' | sort)

# Compile/link flags exactly as upstream's Makefile has them for the schismtracker program.
mk_var() {
  (cd "$FUZZ_DIR" && make --no-print-directory -f Makefile -f - print-var VAR="$1" <<'MK'
print-var:
	@echo $($(VAR))
MK
  )
}
CPPFLAGS_PROJ="$(mk_var DEFS) $(mk_var DEFAULT_INCLUDES) $(mk_var INCLUDES) $(mk_var AM_CPPFLAGS) $(mk_var schismtracker_CPPFLAGS) $(mk_var CPPFLAGS)"
LIBS="$(mk_var schismtracker_LDADD) $(mk_var LIBS)"
HFLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS -O1"

(cd "$FUZZ_DIR"
 # shellcheck disable=SC2086
 $CC $CPPFLAGS_PROJ $HFLAGS -Dmain=schism_cli_main -c "$SRC/schism/main.c" -o main-cli.o
 $CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.c" -o lsan_off.o
 $CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o standalone_main.o)

# 3) Each harness twice: libFuzzer, and the run-once standalone reproducer.
for h in $HARNESSES; do
  (
    cd "$FUZZ_DIR"
    # shellcheck disable=SC2086
    $CC $CPPFLAGS_PROJ $HFLAGS -c "$SRC/mayhem/$h.c" -o "$h.o"
    # shellcheck disable=SC2086
    $CC $SANITIZER_FLAGS $LIB_FUZZING_ENGINE "$h.o" lsan_off.o main-cli.o $OBJS $LIBS -o "$SRC/$h"
    # shellcheck disable=SC2086
    $CC $SANITIZER_FLAGS "$h.o" standalone_main.o lsan_off.o main-cli.o $OBJS $LIBS -o "$SRC/$h-standalone"
  )
done

# 4) The project's own test suite, with its NORMAL flags (separate tree, no sanitizers, no SanCov).
mkdir -p "$TEST_DIR"
(
  cd "$TEST_DIR"
  [ -f config.status ] || env -u CFLAGS -u CXXFLAGS -u LDFLAGS "$SRC/configure" CC="$CC" \
      --enable-tests CFLAGS="$COVERAGE_FLAGS" LDFLAGS="$COVERAGE_FLAGS"
  make -j"$MAYHEM_JOBS" schismtrackertest
)
