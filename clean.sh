#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "${ROOT_DIR}"

# Remove common CMake build directories and generated artifacts.
rm -rf "${ROOT_DIR}/build" \
       "${ROOT_DIR}/debug" \
       "${ROOT_DIR}/CMakeFiles" \
       "${ROOT_DIR}/CMakeCache.txt" \
       "${ROOT_DIR}/cmake_install.cmake" \
       "${ROOT_DIR}/compile_commands.json" \
       "${ROOT_DIR}/Makefile" \
       "${ROOT_DIR}/Testing"

# Remove DRAMsim3 local build directories if present.
rm -rf "${ROOT_DIR}/DRAMsim3/build" \
       "${ROOT_DIR}/DRAMsim3/CMakeFiles" \
       "${ROOT_DIR}/DRAMsim3/CMakeCache.txt" \
       "${ROOT_DIR}/DRAMsim3/cmake_install.cmake" \
       "${ROOT_DIR}/DRAMsim3/Makefile" \
       "${ROOT_DIR}/DRAMsim3/Testing"

echo "Clean finished."
