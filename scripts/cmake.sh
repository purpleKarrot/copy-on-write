#!/bin/bash

set -eu -o pipefail

# Get the workspace root
WORKSPACE_ROOT=$(git rev-parse --show-toplevel)

# Change directory to the workspace root and run the python script
cd "$WORKSPACE_ROOT"
python3 scripts/cmake.py "$@"
