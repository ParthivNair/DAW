#!/usr/bin/env bash
#
# run.sh — build and launch the EZStudio app.
#
# Builds the EZStudio target for a CMake preset (default: dev), then opens the resulting
# .app bundle. Configures the preset's build dir first if it hasn't been set up yet.
# Works from any checkout (it resolves the repo root itself), so it survives rebuilds and
# branch switches.
#
# Usage:
#   tools/run.sh            # build + launch the Debug (dev) build
#   tools/run.sh release    # build + launch the Release (universal2) build
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${REPO_ROOT}"

PRESET="${1:-dev}"
BUILD_DIR="build/${PRESET}"

log() { printf '%s\n' "[run] $*"; }
die() { printf '%s\n' "[run] ERROR: $*" >&2; exit 1; }

# Configure the preset's build dir on first use (fresh clone / new preset).
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    log "configuring preset '${PRESET}' (first run)..."
    cmake --preset "${PRESET}"
fi

log "building EZStudio (preset '${PRESET}')..."
cmake --build --preset "${PRESET}" --target EZStudio

# JUCE writes the bundle to <build>/EZStudio_artefacts/<Config>/EZStudio.app; glob for it
# so we don't hardcode the Debug/Release config subdir.
APP="$(find "${BUILD_DIR}/EZStudio_artefacts" -maxdepth 2 -name 'EZStudio.app' -type d 2>/dev/null | head -1)"
[ -n "${APP}" ] && [ -d "${APP}" ] || die "EZStudio.app not found under ${BUILD_DIR}/EZStudio_artefacts"

log "launching ${APP}"
open "${APP}"
