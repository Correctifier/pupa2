#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PYTHONPATH="${project_root}/pc/src${PYTHONPATH:+:${PYTHONPATH}}"
exec python3 -m pickup_analyzer.gui "$@"

