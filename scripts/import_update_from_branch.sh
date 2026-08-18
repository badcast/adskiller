#!/usr/bin/env bash
set -euo pipefail

# import_update_from_branch.sh
# Safely copy only the `update` directory from branch `new_version_v2_beta`
# into the current branch working tree. Does not push by default.

BRANCH="new_version_v2_beta"
PATH_TO_COPY="update"

usage() {
  cat <<EOF
Usage: $0 [--push]
  --push   Commit and push the imported `update` folder to the current remote branch.
EOF
}

PUSH=false
while [[ $# -gt 0 ]]; do
  case "$1" in
    --push) PUSH=true; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown arg: $1"; usage; exit 1 ;;
  esac
done

if ! command -v git >/dev/null 2>&1; then
  echo "git not found; install git to run this script." >&2
  exit 2
fi

if [ ! -d .git ]; then
  echo "This script must be run from the repository root." >&2
  exit 2
fi

echo "Fetching remote refs..."
git fetch origin "$BRANCH":"refs/remotes/origin/${BRANCH}" || true

if ! git show-ref --verify --quiet "refs/remotes/origin/${BRANCH}"; then
  echo "Branch ${BRANCH} not found on remote 'origin'." >&2
  exit 3
fi

if [ -e "${PATH_TO_COPY}" ]; then
  echo "Backing up existing '${PATH_TO_COPY}' -> ${PATH_TO_COPY}.bak"
  rm -rf "${PATH_TO_COPY}.bak" || true
  mv "${PATH_TO_COPY}" "${PATH_TO_COPY}.bak"
fi

echo "Checking out '${PATH_TO_COPY}' from ${BRANCH}..."
# Use git checkout of the path from the remote branch
git checkout "refs/remotes/origin/${BRANCH}" -- "${PATH_TO_COPY}"

if [ ! -e "${PATH_TO_COPY}" ]; then
  echo "Failed to retrieve ${PATH_TO_COPY} from ${BRANCH}. Restoring backup." >&2
  if [ -d "${PATH_TO_COPY}.bak" ]; then
    mv "${PATH_TO_COPY}.bak" "${PATH_TO_COPY}"
  fi
  exit 4
fi

echo "Staging '${PATH_TO_COPY}' for commit..."
git add "${PATH_TO_COPY}"

MSG="Import '${PATH_TO_COPY}' from ${BRANCH}"
git commit -m "$MSG" || echo "Nothing to commit (no changes)."

if [ "$PUSH" = true ]; then
  BRANCH_CURRENT=$(git rev-parse --abbrev-ref HEAD)
  echo "Pushing commit to origin/${BRANCH_CURRENT}..."
  git push origin "$BRANCH_CURRENT"
fi

echo "Done."
