#!/usr/bin/env bash
# 功能：初始化 gem5 子模块、应用 Xerxes overlay，并编译指定的 gem5 target。
# 同时调用 SCons 生成 compile_commands.json，供 clangd 等工具使用。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GEM5_DIR="${ROOT_DIR}/third_party/gem5"
TARGET="${1:-X86_MESI_Two_Level_Xerxes}"
BINARY="${2:-${GEM5_BINARY:-gem5.opt}}"
CCACHE_BIN="$(command -v ccache || true)"
source "${ROOT_DIR}/scripts/common_parallel.sh"

if [[ -z "${PARALLEL:-}" ]]; then
  PARALLEL="$(choose_parallelism "${GEM5_JOB_MEM_MB:-2200}")"
  echo "Auto-selected PARALLEL=${PARALLEL} based on CPU count and available memory."
  echo "Override with PARALLEL=<jobs>; tune memory budget with GEM5_JOB_MEM_MB=<MiB per job>."
else
  echo "Using user-provided PARALLEL=${PARALLEL}."
fi

cd "${ROOT_DIR}"
git submodule update --init --recursive third_party/gem5

"${ROOT_DIR}/scripts/apply_gem5_overlay.sh"

if [[ -z "${CCACHE_BIN}" ]]; then
  echo "ERROR: ccache not found in PATH" >&2
  echo "Install it first, e.g.: sudo apt-get install -y ccache" >&2
  exit 1
fi

if ! command -v scons >/dev/null 2>&1; then
  echo "scons not found; installing SCons into the user Python environment..."
  python3 -m pip install --user scons
  export PATH="${HOME}/.local/bin:${PATH}"
fi

cd "${GEM5_DIR}"

# SCons checks compiler identity by invoking the compiler directly, so avoid
# setting CXX="ccache g++". Instead use ccache's masquerade mode through
# compiler-name symlinks placed at the front of PATH.
CCACHE_WRAPPER_DIR="${XDG_CACHE_HOME:-${HOME}/.cache}/xerxes-gem5-ccache-wrappers"
mkdir -p "${CCACHE_WRAPPER_DIR}"
ln -sf "${CCACHE_BIN}" "${CCACHE_WRAPPER_DIR}/gcc"
ln -sf "${CCACHE_BIN}" "${CCACHE_WRAPPER_DIR}/g++"
ln -sf "${CCACHE_BIN}" "${CCACHE_WRAPPER_DIR}/clang"
ln -sf "${CCACHE_BIN}" "${CCACHE_WRAPPER_DIR}/clang++"

export PATH="${CCACHE_WRAPPER_DIR}:${PATH}"
export CCACHE_BASEDIR="${CCACHE_BASEDIR:-${GEM5_DIR}}"
export CCACHE_DIR="${CCACHE_DIR:-${HOME}/.cache/ccache}"
export CCACHE_COMPILERCHECK="${CCACHE_COMPILERCHECK:-content}"
export M5_IGNORE_STYLE="${M5_IGNORE_STYLE:-1}"

scons --ignore-style defconfig "build/${TARGET}" "build_opts/${TARGET}"
scons --ignore-style -j "${PARALLEL}" "build/${TARGET}/${BINARY}"

# Let SCons generate the compilation database. Do not hand-edit it.
scons --ignore-style "build/${TARGET}/compile_commands.json"

echo "gem5 build finished: ${GEM5_DIR}/build/${TARGET}/${BINARY}"
echo "compile_commands.json: ${GEM5_DIR}/build/${TARGET}/compile_commands.json"
