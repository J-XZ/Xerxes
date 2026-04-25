#!/usr/bin/env bash
# 功能：一键编译带 Xerxes overlay 的 gem5 debug 版本。
# 这是面向调试断言、backtrace 和问题定位的便捷入口，内部转发到通用 gem5 构建脚本。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${1:-${GEM5_TARGET:-X86_MESI_Two_Level_Xerxes}}"

exec "${ROOT_DIR}/scripts/build_gem5_xerxes.sh" "${TARGET}" gem5.debug
