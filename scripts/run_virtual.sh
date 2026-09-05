#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  echo "Usage: $0 [--headless] [--wayland] [port]"
  echo "Build and run the virtual target and PC app. Arguments go to the target."
  echo "Connect the PC app to 127.0.0.1:8765 (or the specified port)."
  echo "Closing either app or pressing Ctrl+C stops both."
  exit 0
fi

cmake -S "$project_root" -B "$project_root/build" -DPICKUP_BUILD_VIRTUAL_TARGET=ON
cmake --build "$project_root/build" --target pickup_virtual_target -j 2

pids=()
cleanup() {
  trap - EXIT INT TERM
  if (( ${#pids[@]} )); then
    kill "${pids[@]}" 2>/dev/null || true
    wait "${pids[@]}" 2>/dev/null || true
  fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# Keep GUI state files in the workspace even when launched from elsewhere.
cd "$project_root"
"$project_root/build/target/targets/virtual/pickup_virtual_target" "$@" &
pids+=("$!")
"$project_root/scripts/run_pc.sh" &
pids+=("$!")

wait -n "${pids[@]}"
