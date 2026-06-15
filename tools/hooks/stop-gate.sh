#!/usr/bin/env bash
#
# Stop hook: the per-turn test gate. Builds daw_tests and runs the suite on the
# `dev` preset; blocks the turn from ending if either fails.
#
# Contract (see https://code.claude.com/docs/en/hooks):
#   exit 0  -> success, turn may end (quiet on success).
#   exit 2  -> blocking error: stderr is fed back to Claude to act on.
# Fresh clone with no build/ : exit 0 (nothing configured yet; nothing to gate).
#
# The build+test must stay fast (suite is < 1 min by design) so it can gate
# every turn. Set CTEST_GATE_ARGS to pass extra args to ctest (used to exercise
# the failure path in tests, e.g. CTEST_GATE_ARGS='-R __no_such_test__').
set -uo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
cd "$repo_root" || exit 0

# Fresh clone / never configured: no-op cleanly.
if [[ ! -d build/dev ]]; then
    exit 0
fi

log="$(mktemp -t stop-gate.XXXXXX)"
trap 'rm -f "$log"' EXIT

if ! cmake --build --preset dev --target daw_tests >"$log" 2>&1; then
    {
        echo "Stop gate: build of daw_tests FAILED (cmake --build --preset dev --target daw_tests)."
        echo "--- last 30 lines ---"
        tail -n 30 "$log"
    } >&2
    exit 2
fi

# shellcheck disable=SC2086
if ! ctest --preset dev ${CTEST_GATE_ARGS:-} >"$log" 2>&1; then
    {
        echo "Stop gate: ctest --preset dev FAILED. Fix the failing test(s) before ending the turn."
        echo "--- last 40 lines ---"
        tail -n 40 "$log"
    } >&2
    exit 2
fi

exit 0
