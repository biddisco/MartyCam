# This is the spack environment that will be used for this project
SPACK_ENV=home

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
echo script dir is $SCRIPT_DIR

# ----------------------------------------------------------------------------
# source spack environment (which also sets python version)
# ----------------------------------------------------------------------------
echo checking for $SPACK_ENV environment
if spack env list | grep -q "$SPACK_ENV"; then
  echo Activating spack environment in $SCRIPT_DIR
  spack env activate --prompt $SPACK_ENV
fi

# ----------------------------------------------------------------------------
# source python environment if present
# ----------------------------------------------------------------------------
if [[ -f "$SCRIPT_DIR/python/.venv/bin/activate" ]]; then
  echo Activating python environment in $SCRIPT_DIR
  source "$SCRIPT_DIR/python/.venv/bin/activate"
fi

