#!/bin/bash

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
CURDIR=$(realpath $(dirname "$0"))
source ${CURDIR}/_test_common.sh

startTest "Static checks test"

SRCFILES="$@"

# Create a temporary directory to store results between runs
BUILDDIR="build/gh-checks/static-checks/"
mkdir -p "${CURDIR}/../../${BUILDDIR}"

# Run docker with action-cxx-toolkit to check our code
docker run ${DOCKER_RUN_PARAMS} \
    -e INPUT_CHECKS='cppcheck clang-tidy' \
    -e INPUT_BUILDDIR="/github/workspace/${BUILDDIR}" \
    -e INPUT_CMAKEFLAGS="-DCMAKE_BUILD_TYPE=Release -DWITH_TESTS=On" \
    -e INPUT_CPPCHECKFLAGS='--enable=warning,style,performance,portability --inline-suppr' \
    -e INPUT_CLANGTIDYFLAGS="-quiet ${SRCFILES} -j 4" \
    $IMAGENAME
status=$?
printStatus $status

