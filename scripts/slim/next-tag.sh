#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
#
# SPDX-License-Identifier: MIT
#
# Prints the next chatterino-slim release tag for the checked-out commit:
# v<Chatterino version>-slim-<channel>.<n>, for example v2.5.6-slim-alpha.1.
# The channel defaults to alpha. Run from the repository root after fetching
# tags.
#
# Usage: scripts/slim/next-tag.sh [channel]

set -euo pipefail

channel=${1:-alpha}
if [[ ! "$channel" =~ ^[a-z]+$ ]]; then
    echo "channel must be lower-case letters: $channel" >&2
    exit 1
fi

# The project() call in CMakeLists.txt carries the Chatterino version
version=$(sed -n 's/^    VERSION \([0-9]\+\.[0-9]\+\.[0-9]\+\)$/\1/p' CMakeLists.txt | head -n 1)
if [[ -z "$version" ]]; then
    echo "could not read the version from CMakeLists.txt" >&2
    exit 1
fi

prefix="v$version-slim-$channel."
last=$(git tag --list "$prefix*" | sed "s/^${prefix//./\\.}//" | grep -E '^[0-9]+$' | sort -n | tail -n 1 || true)
echo "$prefix$((${last:-0} + 1))"
