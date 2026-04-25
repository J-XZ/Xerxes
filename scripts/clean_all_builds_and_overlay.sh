#!/usr/bin/env bash
# 功能：一键清理 Xerxes、DRAMsim3、gem5 的构建产物，并移除已应用的 overlay。
# 不区分 debug 或 release，所有常见编译结果都会一起清理。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GEM5_DIR="${ROOT_DIR}/third_party/gem5"

echo "Cleaning Xerxes standalone build artifacts..."
rm -rf \
  "${ROOT_DIR}/build" \
  "${ROOT_DIR}/debug" \
  "${ROOT_DIR}/debug_build" \
  "${ROOT_DIR}/CMakeFiles" \
  "${ROOT_DIR}/CMakeCache.txt" \
  "${ROOT_DIR}/cmake_install.cmake" \
  "${ROOT_DIR}/compile_commands.json" \
  "${ROOT_DIR}/Makefile" \
  "${ROOT_DIR}/Testing"

echo "Cleaning DRAMsim3 build artifacts..."
rm -rf \
  "${ROOT_DIR}/DRAMsim3/build" \
  "${ROOT_DIR}/DRAMsim3/CMakeFiles" \
  "${ROOT_DIR}/DRAMsim3/CMakeCache.txt" \
  "${ROOT_DIR}/DRAMsim3/cmake_install.cmake" \
  "${ROOT_DIR}/DRAMsim3/compile_commands.json" \
  "${ROOT_DIR}/DRAMsim3/Makefile" \
  "${ROOT_DIR}/DRAMsim3/Testing"

if [ -d "${GEM5_DIR}" ]; then
  echo "Cleaning gem5 build artifacts..."
  rm -rf \
    "${GEM5_DIR}/build" \
    "${GEM5_DIR}/m5out"
  rm -f \
    "${GEM5_DIR}/.sconsign.dblite" \
    "${GEM5_DIR}/.sconsign"

  echo "Removing Xerxes overlay from gem5..."
  "${ROOT_DIR}/scripts/remove_gem5_overlay.sh"
else
  echo "Skipping gem5 cleanup because ${GEM5_DIR} does not exist."
fi

echo "Clean finished."
