#!/usr/bin/env bash
#
# bootstrap.sh — idempotent fresh-clone initializer for this repo's submodules.
#
# What it does:
#   1. Warns (does not fail) if Git LFS filters are not installed, since golden WAV
#      fixtures live in Git LFS.
#   2. Initializes the top-level submodules under libs/.
#   3. Rewrites the Tracktion Engine's JUCE submodule URL from SSH to HTTPS (the engine
#      pins JUCE via git@github.com:..., which breaks recursive init on machines without
#      GitHub SSH keys — e.g. fresh clones and CI), then initializes recursively.
#   4. Prints the resolved SHA of every submodule, including the nested JUCE, and fails
#      non-zero if libs/tracktion_engine or its JUCE do not match the pinned SHAs.
#
# Safe to run repeatedly on an already-initialized checkout. Works locally and in CI.
#
set -euo pipefail

# --- Pinned SHAs we hard-assert (see THIRD_PARTY_LICENSES.md) -------------------------
readonly ENGINE_PIN="2877b621f2fbee564d0696a616b86bf8ba8c8ab0"   # develop, engine 3.2.0
readonly JUCE_PIN="7c89e11f6b7316c369f3d3f22227c60e816e738b"     # JUCE 8.0.12

# --- Locate the repo root regardless of where this is invoked from --------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${REPO_ROOT}"

# --- Colorless logging helpers (CI-friendly) ------------------------------------------
log()  { printf '%s\n' "[bootstrap] $*"; }
warn() { printf '%s\n' "[bootstrap] WARNING: $*" >&2; }
die()  { printf '%s\n' "[bootstrap] ERROR: $*" >&2; exit 1; }

log "repo root: ${REPO_ROOT}"

# --- (a) Git LFS check ----------------------------------------------------------------
# Golden render fixtures live in Git LFS. We warn rather than fail so the build deps can
# still be fetched; fixtures simply won't materialize without LFS filters installed.
if ! command -v git-lfs >/dev/null 2>&1 && ! git lfs version >/dev/null 2>&1; then
    warn "Git LFS does not appear to be installed."
    warn "  Golden WAV fixtures under tests/fixtures/ will NOT be checked out correctly."
    warn "  Install it (e.g. 'brew install git-lfs') and run 'git lfs install', then re-run."
elif ! git config --get filter.lfs.clean >/dev/null 2>&1; then
    warn "Git LFS is installed but its filters are not registered for git."
    warn "  Run 'git lfs install' so fixtures under tests/fixtures/ check out correctly."
else
    log "Git LFS filters present."
fi

# --- (b) Three-step submodule sequence (the proven JUCE SSH->HTTPS fix) ----------------
# Step 1: init top-level submodules (libs/*). This clones tracktion_engine but does NOT
#         recurse into its JUCE submodule yet.
log "Step 1/3: git submodule update --init (top level)"
git submodule update --init

# Step 2: point the engine's nested JUCE submodule at HTTPS instead of SSH. The engine's
#         own .gitmodules uses git@github.com:..., which fails without SSH keys. This
#         config override is local to this checkout and is idempotent.
if [ -d "libs/tracktion_engine" ]; then
    log "Step 2/3: rewrite tracktion_engine JUCE submodule URL to HTTPS"
    git -C libs/tracktion_engine config \
        submodule.modules/juce.url \
        https://github.com/juce-framework/JUCE.git
else
    die "libs/tracktion_engine not present after step 1 — submodule init failed."
fi

# Step 3: recursive init now that JUCE resolves over HTTPS.
log "Step 3/3: git submodule update --init --recursive"
git submodule update --init --recursive

# --- (c) Print resolved SHAs and assert the two hard pins -----------------------------
echo
log "Resolved submodule SHAs:"
git submodule status --recursive | while IFS= read -r line; do
    printf '%s\n' "  ${line}"
done
echo

resolve_sha() { git -C "$1" rev-parse HEAD; }

[ -d "libs/tracktion_engine" ] || die "libs/tracktion_engine missing."
[ -d "libs/tracktion_engine/modules/juce" ] || \
    die "libs/tracktion_engine/modules/juce missing — recursive init failed."

engine_sha="$(resolve_sha libs/tracktion_engine)"
juce_sha="$(resolve_sha libs/tracktion_engine/modules/juce)"

ok=0
if [ "${engine_sha}" != "${ENGINE_PIN}" ]; then
    warn "tracktion_engine at ${engine_sha}, expected ${ENGINE_PIN}"
    ok=1
else
    log "tracktion_engine pinned correctly: ${engine_sha}"
fi
if [ "${juce_sha}" != "${JUCE_PIN}" ]; then
    warn "JUCE at ${juce_sha}, expected ${JUCE_PIN}"
    ok=1
else
    log "JUCE pinned correctly: ${juce_sha}"
fi

if [ "${ok}" -ne 0 ]; then
    die "Pinned-SHA verification failed. See warnings above."
fi

echo
log "Bootstrap complete — all pinned SHAs verified."
