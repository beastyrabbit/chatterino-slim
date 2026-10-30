# chatterino-slim changelog

Changes of the fork only. Upstream changes are in [CHANGELOG.md](CHANGELOG.md).

## Unreleased

## v2.5.6-slim-alpha.1

First alpha. Based on upstream Chatterino `master` as of 2026-10-28 (`ccfee0fa`), which reports version 2.5.6.

- Added a setting and the `--hide-split-header` flag to hide the channel header above each chat.
- Added the `--hide-header-buttons` flag to hide the settings and account buttons for one session.
- Added a setting and the `--hide-add-tab-button` flag to hide the `+` button after the tabs.
- Added a setting and the `--hide-single-tab` flag to show the tab row only from the second tab on.
- Added a setting and the `--fill-tabs` flag to stretch tabs to the full window width, with equal widths.
- Added `--slim`, which turns on all of the above for one session.
- Added shared instances: `--instance <name>` lets later launches open channels as tabs in a running window, and `--remove` closes them again. The window quits after its last tab.
- Hiding all tabs with Ctrl+U is remembered across restarts.
- Sessions started with `-c` no longer ask whether to show the changelog on every launch.
