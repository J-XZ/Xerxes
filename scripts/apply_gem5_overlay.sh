#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OVERLAY_DIR="${ROOT_DIR}/gem5"
GEM5_DIR="${ROOT_DIR}/third_party/gem5"
MANIFEST="${OVERLAY_DIR}/MANIFEST.txt"

if [ ! -d "${GEM5_DIR}" ]; then
  echo "ERROR: gem5 submodule not found: ${GEM5_DIR}" >&2
  echo "Run: git submodule update --init --recursive third_party/gem5" >&2
  exit 1
fi

if [ ! -f "${MANIFEST}" ]; then
  echo "ERROR: overlay manifest not found: ${MANIFEST}" >&2
  exit 1
fi

while IFS= read -r relpath; do
  [ -z "${relpath}" ] && continue
  [ "${relpath#\#}" != "${relpath}" ] && continue

  src="${OVERLAY_DIR}/${relpath}"
  dst="${GEM5_DIR}/${relpath}"

  if [ ! -e "${src}" ]; then
    echo "ERROR: manifest entry missing from overlay: ${relpath}" >&2
    exit 1
  fi

  mkdir -p "$(dirname "${dst}")"
  cp -a "${src}" "${dst}"
done < "${MANIFEST}"

cp -a "${OVERLAY_DIR}/README.md" "${GEM5_DIR}/XERXES_INTEGRATION.md"
cp -a "${OVERLAY_DIR}/MANIFEST.txt" "${GEM5_DIR}/XERXES_MANIFEST.txt"

mkdir -p "${GEM5_DIR}/ext"
rm -rf "${GEM5_DIR}/ext/xerxes"
ln -s ../../.. "${GEM5_DIR}/ext/xerxes"

echo "Applied Xerxes gem5 overlay to ${GEM5_DIR}"
echo "Linked ${GEM5_DIR}/ext/xerxes -> ../../.."

