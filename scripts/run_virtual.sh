#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  echo "Usage: $0 [--headless] [--wayland] [port]"
  echo "Build and run the virtual target and PC app. Arguments go to the target."
  echo "The PC app automatically connects to 127.0.0.1 on the selected port."
  echo "Closing either app or pressing Ctrl+C stops both."
  exit 0
fi

port=8765
for argument in "$@"; do
  case "$argument" in
    --headless|--wayland) ;;
    *)
      if [[ ! "$argument" =~ ^[0-9]{1,5}$ ]] || (( 10#$argument < 1 || 10#$argument > 65535 )); then
        echo "Invalid argument: $argument (expected --headless, --wayland, or port 1-65535)" >&2
        exit 2
      fi
      port=$((10#$argument))
      ;;
  esac
done

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
"$project_root/scripts/run_pc.sh" --connect "127.0.0.1:$port" &
pids+=("$!")

wait -n "${pids[@]}"
