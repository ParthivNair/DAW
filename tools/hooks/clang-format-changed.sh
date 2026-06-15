#!/usr/bin/env bash
#
# PostToolUse hook: run clang-format -i on a file Claude Code just edited, but
# ONLY if it is one of OUR C++ source files (.h/.cpp/.mm under src/ or tests/).
# Never touches libs/ (pinned engine/JUCE submodules) or non-C++ files.
#
# Reads the PostToolUse JSON payload on stdin and pulls .tool_input.file_path.
# Always exits 0 — formatting a file is advisory; a parse miss or a non-match is
# a no-op, never a turn-blocking error.
set -euo pipefail

CLANG_FORMAT="${CLANG_FORMAT:-/opt/homebrew/opt/llvm/bin/clang-format}"

payload="$(cat)"

# Pull the edited file's path out of the hook payload. Tolerate any shape:
# fall back to empty (-> no-op) rather than failing the hook.
file="$(printf '%s' "$payload" | jq -r '.tool_input.file_path // empty' 2>/dev/null || true)"

[[ -z "$file" || ! -f "$file" ]] && exit 0

# Repo root = two levels up from this script (tools/hooks/ -> repo).
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

# Normalise to a repo-relative path so the src/ , tests/ , libs/ checks are robust.
case "$file" in
    "$repo_root"/*) rel="${file#"$repo_root"/}" ;;
    /*)             exit 0 ;;            # absolute path outside the repo: skip
    *)              rel="$file" ;;        # already relative
esac

# Our code only: src/ or tests/, C++ extensions, never libs/.
case "$rel" in
    libs/*) exit 0 ;;
esac
case "$rel" in
    src/*.h | src/*.cpp | src/*.mm | tests/*.h | tests/*.cpp | tests/*.mm) ;;
    *) exit 0 ;;
esac

command -v "$CLANG_FORMAT" >/dev/null 2>&1 || exit 0
"$CLANG_FORMAT" -i "$file" || true
exit 0
