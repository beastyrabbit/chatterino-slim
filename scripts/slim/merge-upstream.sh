#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
#
# SPDX-License-Identifier: MIT
#
# Merges upstream Chatterino into a new upstream-sync/<commit> branch off the
# current HEAD. Without UPSTREAM_REF it merges the master commit the latest
# upstream release was cut from (see .github/workflows/slim-upstream-sync.yml
# for why). Prints nothing to merge when HEAD already contains it.
#
# Usage: UPSTREAM_REF=<branch or tag> scripts/slim/merge-upstream.sh [--push]
#   --push  push the branch to origin
# In GitHub Actions the branch name is written to $GITHUB_OUTPUT.

set -euo pipefail

UPSTREAM_URL=https://github.com/Chatterino/chatterino2.git
push=false
[[ "${1:-}" == "--push" ]] && push=true

summary() {
    echo "$*"
    if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
        echo "$*" >>"$GITHUB_STEP_SUMMARY"
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
    target=$(git merge-base refs/upstream/release refs/upstream/master)
    label="upstream $release (fork point on master)"
fi

if git merge-base --is-ancestor "$target" HEAD; then
    summary "Up to date: HEAD already contains $label (${target:0:10})."
    exit 0
fi

branch="upstream-sync/${target:0:10}"
git switch --quiet -C "$branch"
if ! git merge --no-ff --no-edit -m "Merge $label" "$target"; then
    summary "### Merging $label (${target:0:10}) needs a human"
    summary ""
    summary "Conflicting files:"
    git diff --name-only --diff-filter=U | sed 's/^/- /' | while read -r line; do summary "$line"; done
    git merge --abort
    exit 1
fi
# A new upstream workflow would start running here as soon as it's pushed,
# before it can be disabled through the API
added=$(git diff --name-only --diff-filter=A HEAD^1 HEAD -- .github/workflows |
    grep -v '^\.github/workflows/slim-' || true)
if [[ -n "$added" ]]; then
    summary "### Merging $label (${target:0:10}) adds upstream workflows"
    summary ""
    summary "Merge by hand and disable them right after pushing (docs/dev/slim-fork.md):"
    while read -r file; do summary "- $file"; done <<<"$added"
    git checkout --quiet -
    git branch --quiet -D "$branch"
    exit 1
fi

summary "Merged $label (${target:0:10}) into $branch."

if $push; then
    git push --force origin "$branch"
fi
if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
    echo "branch=$branch" >>"$GITHUB_OUTPUT"
fi
