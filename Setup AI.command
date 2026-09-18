#!/bin/zsh
set -euo pipefail
cd -- "${0:A:h}"
sunroom_python="/opt/homebrew/opt/python@3.12/bin/python3.12"
if [[ ! -x "$sunroom_python" ]]; then
  print "Installing the Python runtime for SUNROOM local AI..."
  /opt/homebrew/bin/brew install python@3.12
fi
sunroom_runtime="$HOME/Library/SUNROOM/runtime"
"$sunroom_python" -m venv "$sunroom_runtime"
# Install the pinned transitive set so rebuilds stay reproducible.
"$sunroom_runtime/bin/python" -m pip install -r resources/sunroom/requirements-ai-lock.txt
"$sunroom_runtime/bin/python" scripts/setup_sunroom_ai.py
print "Local AI is ready. Open SUNROOM and choose This Mac / MLX."
