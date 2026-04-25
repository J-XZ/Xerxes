#!/usr/bin/env bash
# 功能：撤销已经应用到 third_party/gem5 的 Xerxes overlay。
# 会按 manifest 恢复被覆盖文件，并删除 overlay 新增文件与 ext/xerxes 链接。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OVERLAY_DIR="${ROOT_DIR}/gem5"
GEM5_DIR="${ROOT_DIR}/third_party/gem5"
MANIFEST="${OVERLAY_DIR}/MANIFEST.txt"

if [ ! -d "${GEM5_DIR}" ]; then
  echo "ERROR: gem5 submodule not found: ${GEM5_DIR}" >&2
  exit 1
fi

if [ ! -f "${MANIFEST}" ]; then
  echo "ERROR: overlay manifest not found: ${MANIFEST}" >&2
  exit 1
fi

if ! git -C "${GEM5_DIR}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "ERROR: ${GEM5_DIR} is not a git work tree" >&2
  exit 1
fi

restore_path() {
  local relpath="$1"
  local dst="${GEM5_DIR}/${relpath}"

  if git -C "${GEM5_DIR}" ls-files --error-unmatch "${relpath}" >/dev/null 2>&1; then
    git -C "${GEM5_DIR}" checkout -- "${relpath}"
    return
  fi

  rm -rf "${dst}"
}

while IFS= read -r relpath; do
  [ -z "${relpath}" ] && continue
  [ "${relpath#\#}" != "${relpath}" ] && continue
  restore_path "${relpath}"
done < "${MANIFEST}"

rm -f "${GEM5_DIR}/XERXES_INTEGRATION.md" "${GEM5_DIR}/XERXES_MANIFEST.txt"
rm -rf "${GEM5_DIR}/ext/xerxes"

echo "Removed Xerxes gem5 overlay from ${GEM5_DIR}"
