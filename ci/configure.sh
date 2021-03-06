#!/bin/bash -e
# Copyright (c) 2018 Graphcore Ltd. All rights reserved.

set -e

if [ ! -f "view.txt" ]
then
  echo "Run 'bash popart/ci/configure.sh' from the poponnx_view directory."
  exit 1
fi

source ./popart/ci/utils.sh

if [ $# -gt 0 ]
then
  PYBIN=$1
else
  PYBIN=python3
fi

if command -v sphinx-build
then
  echo "Building docs"
  DOCS="-DPOPART_CMAKE_ARGS=-DBUILD_DOCS=ON"
else
  echo "Not building docs"
  DOCS=""
fi

case ${PYBIN} in
python2)
  PYPKG="python@2"
  ;;
python3)
  PYPKG="python@3"
  ;;
*)
  echo "configure [python2|python3]"
  ;;
esac

# Find the right executable on an OS/X homebrew platform
if [ -x "$(command -v brew)" ]
then
  PYTHON_BIN_PATH=`brew list ${PYPKG} | grep "python$" | head -n 1`
fi

if [ -z ${PYTHON_BIN_PATH} ]
then
  PYTHON_BIN_PATH=`which ${PYBIN}`
fi

echo "Using ${PYTHON_BIN_PATH}"

VE="${PWD}/../external/popart_build_python_${PYBIN}"

# Set up an independent python virtualenv
rm -rf ${VE}
virtualenv -p ${PYTHON_BIN_PATH} ${VE}
source ${VE}/bin/activate

# Install dependencies
pip install Pillow==6.1 # Pillow 6.1 needed for pytorch issue: https://github.com/python-pillow/Pillow/issues/4130
pip install numpy
pip install pytest
pip install yapf
pip install torchvision
pip install pyyaml
pip install requests
pip install protobuf
pip install ./view/ # Install the graphcore-view package

# Create a directory for building
rm -rf build
mkdir build
cd build

# Create the superpiroject
../cbt/cbt.py ..

# Number of processors
NUM_PROCS=$(get_processor_count)

# Get path to poplar
POPLAR_PATH=$(python -c "import os.path; print(os.path.realpath('../../external/poplar-install/'))")

# Configure cmake
#CC=clang CXX=clang++   cmake . -DPOPLAR_INSTALL_DIR=${POPLAR_PATH} -DEXTERNAL_PROJECT_NUM_JOBS=${NUM_PROCS} ${DOCS}
cmake . -DPOPLAR_INSTALL_DIR=${POPLAR_PATH} -DC10_DIR=${PWD} -DEXTERNAL_PROJECT_NUM_JOBS=${NUM_PROCS} ${DOCS}

echo "Done"

