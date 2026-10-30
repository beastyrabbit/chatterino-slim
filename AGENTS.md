# chatterino-slim

Fork of Chatterino 2 with an opt-in slim layout and shared instances. memex project page: `pages/projects/chatterino-slim.md`.

- Read [docs/dev/slim-fork.md](docs/dev/slim-fork.md) before changing code. Its fork rules keep upstream merges automatic: opt-in only, setting plus flag, small edits in upstream files, fork text in fork-only files.
- Don't edit upstream's `CHANGELOG.md` or `.github/workflows/*` other than `slim-*.yml`. Fork changes go into `SLIM_CHANGELOG.md` under **Unreleased**.
- Format C++ with the repository's `.clang-format`. `lefthook install` sets up gitleaks and clang-format checks.
- Smoke-test changes: build with `-DBUILD_TESTS=ON`, then run `build/bin/chatterino-test --gtest_filter='Slim*:InstanceServer*:Notebook*'` with `QT_QPA_PLATFORM=minimal` and `bash tests/slim/smoke.sh build/bin/chatterino`.
- Never run a test Chatterino against the user's own settings or windows. Use `--portable --portable-dir <tmp>`, and for visual checks Qt's VNC platform (see the developer guide).
- User-facing behavior is documented in [docs/user/slim.md](docs/user/slim.md); keep it in sync with flags and settings.
