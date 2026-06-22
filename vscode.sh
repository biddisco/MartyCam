#!/bin/bash

# ----------------------------------------------------------------------------
# this is the directory of the script, regardless of where it's called from
# ----------------------------------------------------------------------------
if [[ $0 != $BASH_SOURCE ]]; then
  # this script was sourced from somewhere, expand name if a symlink
  SCRIPT_DIR=$(dirname $(readlink -f $BASH_SOURCE))
else
  # this was executed directly
  SCRIPT_DIR="$(readlink -f $(dirname $0))"
fi
echo Launching vscode in $SCRIPT_DIR

# ----------------------------------------------------------------------------
# Source the spack environment we need for the project
# ----------------------------------------------------------------------------
source $SCRIPT_DIR/env-setup.sh

# ----------------------------------------------------------------------------
# Launch vscode using the right workspace root
# ----------------------------------------------------------------------------
code $SCRIPT_DIR
