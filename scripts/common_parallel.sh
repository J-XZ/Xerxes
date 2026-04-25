#!/usr/bin/env bash
# 功能：提供按 CPU 核数和当前可用内存估算并行度的公共函数。
# 供 Xerxes 与 gem5 的构建脚本统一复用。

choose_parallelism() {
  local mem_per_job_mb="${1:-1024}"
  local cpu_count mem_available_kb mem_available_mb jobs_by_mem jobs

  cpu_count="$(nproc 2>/dev/null || echo 1)"
  mem_available_kb="$(awk '/^MemAvailable:/ { print $2 }' /proc/meminfo 2>/dev/null || true)"
  if [[ -z "${mem_available_kb}" ]]; then
    mem_available_kb="$(awk '/^MemTotal:/ { print $2 }' /proc/meminfo 2>/dev/null || echo 2097152)"
  fi

  mem_available_mb=$((mem_available_kb / 1024))
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
