#!/bin/bash

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
CURDIR=$(realpath $(dirname "$0"))
source ${CURDIR}/_test_common.sh

startTest "Clang compilation"

# Create a temporary directory to store results between runs
BUILDDIR="build/gh-checks/clang/"
mkdir -p "${CURDIR}/../../${BUILDDIR}"

# Remove any cached version of conan profiles
rm -fR ${CURDIR}/../../build/gh-checks/conan-cache/profiles

# Run docker with action-cxx-toolkit to check our code
docker run ${DOCKER_RUN_PARAMS} \
    -e INPUT_CHECKS='build test install warnings' \
    -e INPUT_BUILDDIR="/github/workspace/${BUILDDIR}" \
    -e INPUT_CC='clang-13' \
    -e INPUT_CMAKEFLAGS="-DCMAKE_BUILD_TYPE=Release -DWITH_TESTS=On" \
    -e INPUT_CTESTFLAGS="--test-dir /github/workspace/${BUILDDIR}/test" \
    $IMAGENAME
status=$?
printStatus $status
