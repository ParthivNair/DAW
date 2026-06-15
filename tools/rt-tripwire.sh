#!/usr/bin/env bash
#
# rt-tripwire.sh — cheap pre-RTSan grep gate for the audio-processing code.
#
# Scans src/engine/dsp/ for the obvious real-time hazards (mutexes, malloc, raw
# `new`) that must never appear in code reachable from the audio callback. This
# is a fast first line of defence; RTSan (rtsan preset) is the authoritative one.
#
# Exit codes:
#   0  clean — no hits, OR the directory does not exist yet (nothing to scan)
#   1  one or more hits — prints the offending file:line so CI/Claude can act
#
# Wired into CI in Chunk 5. See docs/realtime-rules.md.
set -euo pipefail

# --- Locate the repo root regardless of where this is invoked from --------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

readonly SCAN_DIR="${REPO_ROOT}/src/engine/dsp"

# Hazards: std::mutex / any malloc / a standalone `new` keyword. The patterns are
# deliberately broad — false positives are cheaper than a live audio glitch, and
# anything legitimately needing them belongs behind a src/rt/ wrapper, not here.
readonly PATTERN='std::mutex|malloc|\bnew\b'

log() { printf '%s\n' "[rt-tripwire] $*"; }

if [[ ! -d "${SCAN_DIR}" ]]; then
    log "src/engine/dsp/ does not exist yet — nothing to scan. clean."
    exit 0
fi

# Prefer ripgrep (fast, line:col output); fall back to grep -rn if rg is absent.
if command -v rg >/dev/null 2>&1; then
    if hits="$(rg --line-number --no-heading "${PATTERN}" "${SCAN_DIR}")"; then
        log "FAIL: real-time hazard(s) found in src/engine/dsp/:"
        printf '%s\n' "${hits}"
        log "move the offending code behind a src/rt/ wrapper (see docs/realtime-rules.md)."
        exit 1
    fi
else
    if hits="$(grep -rnE "${PATTERN}" "${SCAN_DIR}")"; then
        log "FAIL: real-time hazard(s) found in src/engine/dsp/:"
        printf '%s\n' "${hits}"
        log "move the offending code behind a src/rt/ wrapper (see docs/realtime-rules.md)."
        exit 1
    fi
fi

log "clean — no real-time hazards in src/engine/dsp/."
exit 0
