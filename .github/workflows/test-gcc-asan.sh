#!/bin/bash

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
CURDIR=$(realpath $(dirname "$0"))
source ${CURDIR}/_test_common.sh

startTest "GCC compilation + address sanitizer"

# Create a temporary directory to store results between runs
BUILDDIR="build/gh-checks/gcc-asan/"
mkdir -p "${CURDIR}/../../${BUILDDIR}"

# Run docker with action-cxx-toolkit to check our code
docker run ${DOCKER_RUN_PARAMS} \
    -e INPUT_CHECKS='build test' \
    -e INPUT_BUILDDIR="/github/workspace/${BUILDDIR}" \
    -e INPUT_CC='gcc' \
    -e INPUT_CMAKEFLAGS="-DCMAKE_BUILD_TYPE=Debug -DWITH_TESTS=On" \
    -e INPUT_CXXFLAGS="-fsanitize=address -fsanitize=undefined" \
    -e INPUT_CTESTFLAGS="--test-dir /github/workspace/${BUILDDIR}/test" \
    $IMAGENAME
status=$?
printStatus $status
