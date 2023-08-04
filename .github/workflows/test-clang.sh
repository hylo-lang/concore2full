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

# Run docker with action-cxx-toolkit to check our code
docker run ${DOCKER_RUN_PARAMS} \
    -e INPUT_BUILDDIR="/github/workspace/${BUILDDIR}" \
    -e INPUT_CC='clang-13' \
    -e INPUT_CHECKS='build test' \
    -e INPUT_CONANFLAGS="--output-folder /github/workspace/${BUILDDIR}" \
    -e INPUT_CMAKEFLAGS="-DCMAKE_TOOLCHAIN_FILE=/github/workspace/${BUILDDIR}/conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_BUILD_TYPE=Release" \
    $IMAGENAME
status=$?
printStatus $status
