#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
#
# SPDX-License-Identifier: MIT
#
# Merges upstream Chatterino into a new branch off the current HEAD. Without
# UPSTREAM_REF it merges the master commit the latest upstream release was
# cut from (see docs/dev/slim-fork.md for why).
#
# Usage: UPSTREAM_REF=<branch or tag> scripts/slim/merge-upstream.sh
#            [--push] [--branch <name>]
#   --push    push the new branch to origin. A branch that already exists
#             there is never overwritten
#   --branch  branch name (default: upstream-sync/<commit>)
#
# Writes result=up-to-date|exists|merged|conflict|new-workflow, plus branch,
# target and label, to $GITHUB_OUTPUT in GitHub Actions. Exits 1 on conflict
# and new-workflow.

set -euo pipefail

UPSTREAM_URL=${UPSTREAM_URL:-https://github.com/Chatterino/chatterino2.git}
push=false
branch=
while [[ $# -gt 0 ]]; do
    case "$1" in
        --push) push=true ;;
        --branch)
            branch=${2:?--branch needs a name}
            shift
            ;;
        *)
            echo "unknown argument: $1" >&2
            exit 1
            ;;
    esac
    shift
done

summary() {
    echo "$*"
    if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
        echo "$*" >>"$GITHUB_STEP_SUMMARY"
    fi
}

output() {
    if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
        echo "$1=$2" >>"$GITHUB_OUTPUT"
    fi
}

# README.md is marked merge=ours in .gitattributes; the driver must exist
git config merge.ours.driver true

git fetch --quiet --no-tags "$UPSTREAM_URL" "+refs/heads/master:refs/upstream/master"

if [[ -n "${UPSTREAM_REF:-}" ]]; then
    if [[ ! "$UPSTREAM_REF" =~ ^[A-Za-z0-9._/-]+$ ]]; then
        echo "invalid upstream ref: $UPSTREAM_REF" >&2
        exit 1
    fi
    git fetch --quiet --no-tags "$UPSTREAM_URL" "$UPSTREAM_REF"
    target=$(git rev-parse FETCH_HEAD)
    label="upstream $UPSTREAM_REF"
else
    release=$(git ls-remote --tags --refs "$UPSTREAM_URL" 'v*' |
        sed 's#.*refs/tags/##' | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' |
        sort -V | tail -n 1)
    git fetch --quiet --no-tags "$UPSTREAM_URL" \
        "+refs/tags/$release:refs/upstream/release"
    release_sha=$(git rev-parse 'refs/upstream/release^{commit}')
    target=$(git merge-base refs/upstream/release refs/upstream/master)
    label="upstream $release (fork point on master)"
    summary "Upstream release $release is ${release_sha:0:10}, cut from master ${target:0:10}."
fi
branch=${branch:-upstream-sync/${target:0:10}}
base=$(git rev-parse HEAD)
output target "$target"
output label "$label"
output branch "$branch"

if git merge-base --is-ancestor "$target" HEAD; then
    summary "Up to date: HEAD already contains $label (${target:0:10})."
    output result up-to-date
    exit 0
fi

# Never overwrite a pushed branch: it may hold fixes for this merge
if $push && git ls-remote --exit-code --heads origin "$branch" >/dev/null; then
    summary "Branch $branch already exists; leaving it as it is."
    output result exists
    exit 0
fi

git switch --quiet -C "$branch"
if ! git merge --no-ff --no-edit -m "Merge $label" "$target"; then
    summary "### Merging $label (${target:0:10}) conflicts"
    summary ""
    summary "Merge \`$target\` into \`$base\` to reproduce. Conflicting files:"
    git diff --name-only --diff-filter=U | while read -r file; do summary "- $file"; done
    git merge --abort
    output result conflict
    exit 1
fi
# A new upstream workflow would start running as soon as it's pushed,
# before it can be disabled through the API. Renames count as new files
added=$(git diff --no-renames --name-only --diff-filter=A HEAD^1 HEAD -- .github/workflows)
if [[ -n "$added" ]]; then
    summary "### Merging $label (${target:0:10}) adds workflows"
    summary ""
    summary "Merge by hand with Actions turned off (docs/dev/slim-fork.md):"
    while read -r file; do summary "- $file"; done <<<"$added"
    git checkout --quiet -
    git branch --quiet -D "$branch"
    output result new-workflow
    exit 1
fi

summary "Merged $label (${target:0:10}) into $branch."

if $push; then
    # The empty lease only creates the branch, even if it appeared meanwhile
    git push --quiet --force-with-lease="refs/heads/$branch:" origin "$branch:refs/heads/$branch"
fi
output result merged
