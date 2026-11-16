#!/usr/bin/env bash
#
# mayhem/test.sh — RUN Schism Tracker's own test suite (`schismtrackertest`, built by build.sh with
# the project's normal flags). The suite asserts values: charset conversions, bit/str/util helpers,
# slurp, the IFF and config parsers, disko in-memory writes, the mixer-link sanity checks, ...
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

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

runner="$SRC/mayhem/build-test/schismtrackertest"
if [ ! -x "$runner" ]; then
  echo "test.sh: $runner missing — mayhem/build.sh should have built it" >&2
  exit 2
fi

out="$(mktemp)"
trap 'rm -f "$out"' EXIT
timeout 900 "$runner" >"$out" 2>&1
cat "$out"

# Last line of the runner: "Results: N passed, M failed"
line="$(grep -E '^Results: [0-9]+ passed, [0-9]+ failed' "$out" | tail -n 1)"
if [ -z "$line" ]; then
  echo "test.sh: no 'Results:' summary from schismtrackertest (crashed or exited early)" >&2
  emit_ctrf "schismtrackertest" 0 1 0
  exit 1
fi
passed="$(sed -E 's/^Results: ([0-9]+) passed, ([0-9]+) failed.*/\1/' <<<"$line")"
failed="$(sed -E 's/^Results: ([0-9]+) passed, ([0-9]+) failed.*/\2/' <<<"$line")"
emit_ctrf "schismtrackertest" "$passed" "$failed" 0
