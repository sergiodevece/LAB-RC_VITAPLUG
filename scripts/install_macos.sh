#!/bin/bash
set -euo pipefail
script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
exec python3 -B "${script_dir}/bundle_tool.py" install "$@"
