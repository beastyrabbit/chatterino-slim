#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
#
# SPDX-License-Identifier: MIT
#
# Starts the real chatterino binary without a display and checks the
# chatterino-slim launch flow end to end: the slim flags exist, a named
# instance opens tabs for later launches, closes them on --remove and quits
# after its last tab.
#
# Usage: tests/slim/smoke.sh path/to/chatterino

set -euo pipefail

BIN=$(realpath "${1:?usage: smoke.sh path/to/chatterino}")
WORK=$(mktemp -d)
INSTANCE=smoke-$$
APP_PID=

cleanup() {
    if [[ -n "$APP_PID" ]] && kill -0 "$APP_PID" 2>/dev/null; then
        kill -9 "$APP_PID" 2>/dev/null || true
    fi
    rm -rf "$WORK"
}
trap cleanup EXIT

fail() {
    echo "FAIL: $*" >&2
    if [[ -f "$WORK/app.log" ]]; then
        echo "--- app log (last 40 lines) ---" >&2
        tail -n 40 "$WORK/app.log" >&2
    fi
    exit 1
}

pass() {
    echo "ok: $*"
}

# Private runtime dir, so the socket can't collide with a real instance
export XDG_RUNTIME_DIR="$WORK/runtime"
mkdir -m 700 "$XDG_RUNTIME_DIR"
# Not inherited from the desktop session. Static Qt builds only have the
# xcb and minimal platforms: run those with SMOKE_QPA_PLATFORM=minimal
export QT_QPA_PLATFORM=${SMOKE_QPA_PLATFORM:-offscreen}
export QT_FORCE_STDERR_LOGGING=1
# Without a session bus, D-Bus calls wait for their timeout
export DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent
export QT_QPA_PLATFORMTHEME=none
SOCKET="$XDG_RUNTIME_DIR/chatterino-instance-$INSTANCE"

# A window that fits the offscreen screen avoids the out-of-bounds dialog
PROFILE="$WORK/profile"
mkdir -p "$PROFILE/Settings"
cat >"$PROFILE/Settings/window-layout.json" <<'EOF'
{"windows":[{"type":"main","x":20,"y":20,"width":600,"height":400,"tabs":[]}]}
EOF

app() {
    timeout 20 "$BIN" --portable --portable-dir "$PROFILE" "$@"
}

alive() {
    kill -0 "$APP_PID" 2>/dev/null
}

wait_until() {
    local tries=$1
    shift
    for ((i = 0; i < tries; i++)); do
        if "$@"; then
            return 0
        fi
        sleep 0.2
    done
    return 1
}

help=$(app --help 2>&1 || true)
for flag in --slim --hide-split-header --hide-header-buttons \
    --hide-add-tab-button --hide-single-tab --fill-tabs --instance --remove; do
    grep -q -- "$flag" <<<"$help" || fail "--help does not list $flag"
done
pass "--help lists every slim flag"

app --instance "$INSTANCE" --remove "t:nobody" || fail "--remove without a running instance exited $?"
[[ ! -e "$SOCKET" ]] || fail "--remove without a running instance left a socket"
pass "--remove without a running instance exits cleanly"

"$BIN" --portable --portable-dir "$PROFILE" --slim --instance "$INSTANCE" \
    -c "t:first" >"$WORK/app.log" 2>&1 &
APP_PID=$!
wait_until 150 test -S "$SOCKET" || fail "instance socket did not appear"
[[ "$(stat -c %a "$SOCKET")" == 700 ]] || fail "socket is not private: $(stat -c %a "$SOCKET")"
pass "instance listens on a private socket"

app --instance "$INSTANCE" -c "t:second" || fail "forwarding a channel exited $?"
alive || fail "instance quit after a channel was added"
pass "second launch hands its channel to the instance"

app --instance "$INSTANCE" --remove "t:FIRST" || fail "--remove exited $?"
sleep 1
alive || fail "instance quit while the forwarded tab was still open"
pass "removing one of two tabs keeps the instance running"

app --instance "$INSTANCE" --remove "t:#Second" || fail "--remove exited $?"
wait_until 50 eval '! alive' || fail "instance did not quit after its last tab"
wait "$APP_PID" || fail "instance exited with $?"
APP_PID=
[[ ! -e "$SOCKET" ]] || fail "instance left its socket behind"
pass "instance quits after its last tab and removes its socket"
