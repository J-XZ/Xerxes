#!/usr/bin/env bash
# 功能：一键执行 Xerxes 的常规构建，并可继续触发带 overlay 的 gem5 构建。
# 并行度会按 CPU 核数和当前可用内存自动估算。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-${ROOT_DIR}/build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
GEM5_TARGET="${GEM5_TARGET:-X86_MESI_Two_Level_Xerxes}"
GEM5_BINARY="${GEM5_BINARY:-gem5.opt}"
BUILD_GEM5="${BUILD_GEM5:-1}"
CC_BIN="/usr/bin/clang-18"
CXX_BIN="/usr/bin/clang++-18"
CCACHE_BIN="$(command -v ccache || true)"

source "${ROOT_DIR}/scripts/common_parallel.sh"

if [[ -z "${PARALLEL:-}" ]]; then
  PARALLEL="$(choose_parallelism "${XERXES_JOB_MEM_MB:-1200}")"
  echo "Auto-selected PARALLEL=${PARALLEL} based on CPU count and available memory."
  echo "Override with PARALLEL=<jobs>; tune memory budget with XERXES_JOB_MEM_MB=<MiB per job>."
else
  echo "Using user-provided PARALLEL=${PARALLEL}."
fi

cd "${ROOT_DIR}"

if [ ! -x "${CC_BIN}" ]; then
  echo "ERROR: C compiler not found: ${CC_BIN}" >&2
  exit 1
fi

if [ ! -x "${CXX_BIN}" ]; then
  echo "ERROR: C++ compiler not found: ${CXX_BIN}" >&2
  exit 1
fi

if [ -z "${CCACHE_BIN}" ]; then
  echo "ERROR: ccache not found in PATH" >&2
  echo "Install it first, e.g.: sudo apt-get install -y ccache" >&2
  exit 1
fi

git submodule update --init --recursive

mkdir -p "${ROOT_DIR}/output"
mkdir -p "${BUILD_DIR}"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_C_COMPILER="${CC_BIN}" \
  -DCMAKE_CXX_COMPILER="${CXX_BIN}" \
  -DCMAKE_C_COMPILER_LAUNCHER="${CCACHE_BIN}" \
  -DCMAKE_CXX_COMPILER_LAUNCHER="${CCACHE_BIN}" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build "${BUILD_DIR}" --target Xerxes -j "${PARALLEL}"

if [ "${BUILD_GEM5}" != "0" ]; then
  GEM5_BINARY="${GEM5_BINARY}" "${ROOT_DIR}/scripts/build_gem5_xerxes.sh" "${GEM5_TARGET}" "${GEM5_BINARY}"
fi

echo "Build finished: ${BUILD_DIR}/Xerxes"
if [ "${BUILD_GEM5}" != "0" ]; then
  echo "gem5 build finished: ${ROOT_DIR}/third_party/gem5/build/${GEM5_TARGET}/${GEM5_BINARY}"
fi
