#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GEM5_DIR="${ROOT_DIR}/third_party/gem5"
TARGET="${1:-X86_MESI_Two_Level_Xerxes}"

choose_parallelism() {
  local cpu_count mem_available_kb mem_available_mb mem_per_job_mb jobs_by_mem jobs

  cpu_count="$(nproc 2>/dev/null || echo 1)"
  mem_available_kb="$(awk '/^MemAvailable:/ { print $2 }' /proc/meminfo 2>/dev/null || true)"
  if [[ -z "${mem_available_kb}" ]]; then
    mem_available_kb="$(awk '/^MemTotal:/ { print $2 }' /proc/meminfo 2>/dev/null || echo 2097152)"
  fi

  mem_available_mb=$((mem_available_kb / 1024))
  mem_per_job_mb="${GEM5_JOB_MEM_MB:-2200}"
  jobs_by_mem=$((mem_available_mb / mem_per_job_mb))
  if (( jobs_by_mem < 1 )); then
    jobs_by_mem=1
  fi

  jobs="${cpu_count}"
  if (( jobs_by_mem < jobs )); then
    jobs="${jobs_by_mem}"
  fi
  if (( jobs < 1 )); then
    jobs=1
  fi

  echo "${jobs}"
}

if [[ -z "${PARALLEL:-}" ]]; then
  PARALLEL="$(choose_parallelism)"
  echo "Auto-selected PARALLEL=${PARALLEL} based on CPU count and available memory."
  echo "Override with PARALLEL=<jobs>; tune memory budget with GEM5_JOB_MEM_MB=<MiB per job>."
else
  echo "Using user-provided PARALLEL=${PARALLEL}."
fi

cd "${ROOT_DIR}"
git submodule update --init --recursive third_party/gem5

"${ROOT_DIR}/scripts/apply_gem5_overlay.sh"

if ! command -v scons >/dev/null 2>&1; then
  echo "scons not found; installing SCons into the user Python environment..."
  python3 -m pip install --user scons
  export PATH="${HOME}/.local/bin:${PATH}"
fi

cd "${GEM5_DIR}"

scons defconfig "build/${TARGET}" "build_opts/${TARGET}"
scons -j "${PARALLEL}" "build/${TARGET}/gem5.opt" --ignore-style

# Let SCons generate the compilation database. Do not hand-edit it.
scons "build/${TARGET}/compile_commands.json" --ignore-style

echo "gem5 build finished: ${GEM5_DIR}/build/${TARGET}/gem5.opt"
echo "compile_commands.json: ${GEM5_DIR}/build/${TARGET}/compile_commands.json"
