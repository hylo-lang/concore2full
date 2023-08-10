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

# Remove any cached version of conan profiles
rm -fR ${CURDIR}/../../build/gh-checks/conan-cache/profiles

# Run docker with action-cxx-toolkit to check our code
docker run ${DOCKER_RUN_PARAMS} \
    -e INPUT_CHECKS='cppcheck clang-tidy' \
    -e INPUT_PREBUILD_COMMAND='conan profile detect && conan create /github/workspace/external/context-core-api --build=missing -s compiler.cppstd=17 ; rm -fR ~/.conan2/profiles' \
    -e INPUT_BUILDDIR="/github/workspace/${BUILDDIR}" \
    -e INPUT_CONANFLAGS="--output-folder /github/workspace/${BUILDDIR}" \
    -e INPUT_CMAKEFLAGS="-DCMAKE_TOOLCHAIN_FILE=/github/workspace/${BUILDDIR}/build/Release/generators/conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_BUILD_TYPE=Release" \
    -e INPUT_CPPCHECKFLAGS='--enable=warning,style,performance,portability --inline-suppr' \
    -e INPUT_CLANGTIDYFLAGS="-quiet ${SRCFILES} -j 4" \
    $IMAGENAME
status=$?
printStatus $status

