#!/bin/bash

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
CURDIR=$(realpath $(dirname "$0"))
source ${CURDIR}/_test_common.sh

startTest "Conan package usage test"

SRCFILES="$@"

# Create a temporary directory to store results between runs
BUILDDIR="build/gh-checks/package/"
mkdir -p "${CURDIR}/../../${BUILDDIR}"

# Run docker with action-cxx-toolkit to check our code
docker run ${DOCKER_RUN_PARAMS} \
    -e INPUT_CHECKS='build' \
    -e INPUT_DIRECTORY="/github/workspace" \
    -e INPUT_BUILDDIR="/github/workspace/${BUILDDIR}" \
    -e INPUT_BUILD_COMMAND='conan profile detect -f && conan create /github/workspace/external/context-core-api -s compiler.cppstd=17 && conan create /github/workspace/ -s compiler.cppstd=20' \
    -e INPUT_CTESTFLAGS="--test-dir /github/workspace/${BUILDDIR}/test/" \
    $IMAGENAME
status=$?
printStatus $status

