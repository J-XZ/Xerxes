#!/usr/bin/env bash
# 功能：检查当前 Xerxes 分支与 upstream 分支之间的差异，辅助同步上游更新。
# 适合在准备 rebase、merge 或评估落后提交时使用。
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

if ! git rev-parse --git-dir >/dev/null 2>&1; then
  echo "[error] not a git repository: $repo_root" >&2
  exit 1
fi

if ! git remote get-url upstream >/dev/null 2>&1; then
  echo "[error] remote 'upstream' not found."
  echo "Run: git remote add upstream https://github.com/ChaseLab-PKU/Xerxes.git"
  exit 1
fi

local_branch="${1:-$(git symbolic-ref --quiet --short HEAD || true)}"
if [[ -z "$local_branch" ]]; then
  echo "[error] detached HEAD; pass local branch explicitly."
  echo "Usage: $0 [local_branch] [upstream_branch]"
  exit 1
fi

echo "[info] fetching upstream..."
git fetch upstream --prune

upstream_default_branch="$(git symbolic-ref --quiet --short refs/remotes/upstream/HEAD 2>/dev/null | sed 's#^upstream/##' || true)"
upstream_branch="${2:-${upstream_default_branch:-main}}"

if ! git show-ref --verify --quiet "refs/heads/${local_branch}"; then
  echo "[error] local branch not found: ${local_branch}" >&2
  exit 1
fi

if ! git show-ref --verify --quiet "refs/remotes/upstream/${upstream_branch}"; then
  echo "[error] upstream branch not found: upstream/${upstream_branch}" >&2
  exit 1
fi

read -r local_only upstream_only < <(git rev-list --left-right --count "${local_branch}...upstream/${upstream_branch}")

echo "[info] compare: local '${local_branch}' vs upstream/${upstream_branch}"
echo "[info] local-only commits   : ${local_only}"
echo "[info] upstream-only commits: ${upstream_only}"

if [[ "$upstream_only" -gt 0 ]]; then
  echo "[result] There ARE upstream updates you can merge."
  echo "[hint]   git checkout ${local_branch}"
  echo "[hint]   git merge --no-ff upstream/${upstream_branch}"
  echo ""
  echo "[upstream commits not in local]"
  git --no-pager log --oneline "${local_branch}..upstream/${upstream_branch}"
else
  echo "[result] No upstream updates to merge."
fi
