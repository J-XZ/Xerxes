#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-${ROOT_DIR}/build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
PARALLEL="${PARALLEL:-$(nproc)}"

cd "${ROOT_DIR}"

# Ensure nested submodules (DRAMsim3 and its submodules) are present.
git submodule update --init --recursive

mkdir -p "${ROOT_DIR}/output"
mkdir -p "${BUILD_DIR}"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build "${BUILD_DIR}" --target Xerxes -j "${PARALLEL}"

echo "Build finished: ${BUILD_DIR}/Xerxes"
