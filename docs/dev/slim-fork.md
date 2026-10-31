# chatterino-slim developer guide

This page covers what the fork changes in Chatterino, how it is tested, and how it follows upstream. For building and the code base in general, see upstream's [BUILDING_ON_LINUX.md](../../BUILDING_ON_LINUX.md), [test-and-benchmark.md](../test-and-benchmark.md) and [CONTRIBUTING.md](../../CONTRIBUTING.md).

## Rules for fork changes

The fork has to merge upstream releases without help, so its changes follow a few rules:

- **Opt-in only.** With no slim setting or flag set, Chatterino behaves exactly like upstream. Settings default to off.
- **Setting plus flag.** Every option is a `BoolSetting` and a field in `Args::Slim`. The two are ORed where the option is used. Flags are never written back to settings.
- **Small edits in upstream files.** New logic goes into new files (like `singletons/InstanceServer.*`) or into short blocks. Don't reformat or move upstream code.
- **Fork-only files for fork-only text.** Fork changes go into [SLIM_CHANGELOG.md](../../SLIM_CHANGELOG.md), not upstream's `CHANGELOG.md`. `README.md` belongs to the fork. `.gitattributes` marks it `merge=ours`, so upstream README edits are dropped during merges.
- **Keep upstream's workflows.** Upstream's files in `.github/workflows` stay unchanged so merges stay clean. They are disabled in the GitHub repository. Fork workflows are named `slim-*.yml`.

## What the fork changes

| Area | Files | Change |
| --- | --- | --- |
| Settings | `src/singletons/Settings.hpp` | `hideAllTabs`, `hideSingleTab`, `fillTabWidth`, `hideAddTabButton`, `hideSplitHeader` |
| Settings page | `src/widgets/settingspages/GeneralPage.cpp` | Checkboxes for the new settings |
| Flags | `src/common/Args.{hpp,cpp}` | `--slim`, the five single flags, `--instance`, `--remove`; `Args(QStringList)` for tests. Slim flags and `--instance` survive restarts through `currentArguments()`; `--remove` doesn't |
| Tabs | `src/widgets/Notebook.{hpp,cpp}` | Hide the tab row with one tab, equal-width tabs, `+` button independent of tab visibility, page fills the window without a tab row, Ctrl+U state saved. `SplitNotebook` connects settings and flags |
| Window | `src/widgets/Window.cpp` | `+` button follows `hideAddTabButton` |
| Chat header | `src/widgets/splits/Split.cpp` | Header follows `hideSplitHeader` |
| Shared instance | `src/singletons/InstanceServer.{hpp,cpp}`, `src/main.cpp`, `src/Application.{hpp,cpp}` | Local socket server; `main()` forwards to a running instance before starting the app |
| Startup | `src/Application.cpp` | No "show changelog?" prompt in sessions that can't save its answer (`-c`) |

`git diff <upstream fork point> -- src` shows the whole patch. Find the fork point with `git merge-base HEAD <upstream master>`.

### Shared instance protocol

`InstanceServer::forward()` runs in `main()` right after the arguments are parsed. If `--instance` is set, it connects to `InstanceServer::socketName(name)`: `$XDG_RUNTIME_DIR/chatterino-instance-<name>` on Linux. If an instance answers, it sends one JSON message and closes the connection:

```json
{"add": ["channel"], "remove": ["channel"]}
```

Names are cleaned the way Chatterino cleans channel names (lower case, no leading `#`), on both ends. The receiving instance reads until the connection closes, removes first, then adds, and closes the main window when no tab is left. The server listens with `QLocalServer::UserAccessOption`, so only the same user can connect. It removes a stale socket left behind by a crashed instance before it listens.

Known limit: two first launches with the same name at the same moment can both start an instance. The second one takes over the socket. Launchers should serialize their launches.

## Tests

| Test | File | What it checks |
| --- | --- | --- |
| Unit | `tests/src/SlimArgs.cpp` | Flag parsing, `--slim` implications, instance name validation, restart arguments |
| Unit | `tests/src/SlimNotebook.cpp` | Single tab hidden, page fills the window, equal tab widths, default layout unchanged |
| Unit | `tests/src/InstanceServer.cpp` | `forward()` with and without a running instance, the JSON message |
| Smoke | `tests/slim/smoke.sh` | Runs the real binary headless: `--help` lists the flags, add and remove tabs in a running instance, quit after the last tab, private socket |

Run them locally:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DCHATTERINO_UPDATER=OFF
cmake --build build
QT_QPA_PLATFORM=minimal build/bin/chatterino-test --gtest_filter='Slim*:InstanceServer*:Notebook*'
bash tests/slim/smoke.sh build/bin/chatterino
```

The smoke test uses its own runtime directory and a throwaway portable profile, so it doesn't touch a running Chatterino or your settings. It runs on Qt's `offscreen` platform; static Qt builds only have `minimal`, so test those with `SMOKE_QPA_PLATFORM=minimal`. It needs the Qt WebP plugin installed (see the [user guide](../user/slim.md#troubleshooting)). Upstream's network tests also need `httpbox` and the PubSub test server; [slim-ci.yml](../../.github/workflows/slim-ci.yml) shows how to start them.

When you change layout code, also look at the result. Qt's VNC platform renders without touching your desktop: `QT_QPA_PLATFORM=vnc:size=640x720:port=5912 build/bin/chatterino --portable --portable-dir /tmp/profile --slim -c t:somechannel`, then connect a VNC viewer to `localhost:5912`. The VNC server has no password and listens on all interfaces, so only do this behind a firewall that blocks the port.

## CI

All fork workflows run on the self-hosted runner scale set `arc-chatterino-slim`, defined in kub-homelab (`cluster/homelab/infra/arc-runners/personal/helmreleases.yaml`, release `arc-runner-chatterino-slim`). The repository must be added to the personal ARC GitHub App's installation, and pull requests from outside collaborators need approval before they run.

| Workflow | Runs on | Does |
| --- | --- | --- |
| [Slim CI](../../.github/workflows/slim-ci.yml) | Pull requests, pushes to `main`, manual, called by the sync | Builds in upstream's Ubuntu 26.04 image, runs the slim tests, the smoke test and upstream's whole test suite |
| [Slim release](../../.github/workflows/slim-release.yml) | Tags `v*-slim-*`, manual | Builds the static Linux archive on Ubuntu 22.04, smoke-tests it there and on Ubuntu 26.04, publishes the GitHub release |
| [Slim upstream sync](../../.github/workflows/slim-upstream-sync.yml) | Daily, manual | Merges upstream, tests, fast-forwards `main`, tags a release |

**Fork pull requests.** The runner sits in the home LAN, and a pull request from a fork can change or add workflow files, so the `if:` that makes Slim CI skip fork pull requests is only a safety net. The real gate is the repository setting that requires approval before workflows run for outside contributors (Settings → Actions → General → "Require approval for all external contributors"). Never approve a workflow run on a fork pull request. Check such pull requests out locally instead.

## Following upstream

Upstream develops on `master` and cuts each release on its own `release/vX.Y.Z` branch, which gets a few extra commits. The fork's `main` branch is based on upstream `master`. Merging a release tag into it conflicts in upstream's own files, even where the fork changed nothing. The sync therefore merges the upstream `master` commit that the latest release was cut from (`git merge-base <release tag> upstream/master`). That is always a plain merge, and it only conflicts where the fork's own changes do. A bugfix release that was cut from an older commit is already contained, so it's skipped.

[scripts/slim/merge-upstream.sh](../../scripts/slim/merge-upstream.sh) does the merge. With `UPSTREAM_REF` set, it merges that upstream branch or tag instead, for example `master` for the newest upstream code.

The daily [sync workflow](../../.github/workflows/slim-upstream-sync.yml):

1. Runs the merge script on a new `upstream-sync/<commit>` branch. If HEAD already contains the target, it stops.
2. Runs Slim CI on that branch.
3. If CI passes, fast-forwards `main` to the branch, disables any enabled upstream workflow, pushes the next release tag and deletes the branch. The tag starts the release workflow.

If the merge conflicts or CI fails, `main` stays as it is, the run fails and GitHub emails the repository owner. The run summary lists conflicting files.

The sync also stops when the merge would add a new upstream workflow file. Once pushed, such a workflow could start before it can be disabled. Merge by hand, then disable it right away with `gh workflow disable <name> -R beastyrabbit/chatterino-slim`.

The sync branch is deleted only after `main` moved and the tag was pushed, and tagging skips a commit that already has a slim tag. If the publish job fails, use **Re-run failed jobs**.

### Merging by hand

```sh
git switch main && git pull
UPSTREAM_REF=master bash scripts/slim/merge-upstream.sh   # or no UPSTREAM_REF for the release fork point
# On conflicts: the script aborts the merge and lists the files. Merge yourself:
git merge <commit from the summary>
# Resolve, keeping the fork rules above, then build and run the tests
git push origin HEAD:main
```

When a conflict comes from upstream rewriting code the fork changes, port the fork change to the new code instead of keeping the old one. The unit tests describe the behavior to keep.

### Setup the sync needs

- GitHub App `chatterino-slim-sync`: private, no webhook, repository permissions **Contents** and **Workflows** read and write, installed on this repository only. The sync pushes with a token of this app that is valid for one hour. `GITHUB_TOKEN` can't push upstream's workflow changes, and its tag pushes don't start other workflows. The app's client ID is the repository variable `SYNC_APP_CLIENT_ID`.
- The app's private key, `SYNC_APP_PRIVATE_KEY`, lives in Infisical, not in GitHub secrets: project `kub-homelab`, environment `prod`, path `/projects/chatterino-slim/ci`. The sync jobs read it through GitHub OIDC with the Infisical identity `github-actions-chatterino-slim`, whose ID is in the repository variable `INFISICAL_GITHUB_ACTIONS_IDENTITY_ID`. The identity accepts only workflow runs on `main` of this repository on the self-hosted runner, and it can only read that path. To replace the key, generate a new one on the app's settings page, store it with `infisical secrets set SYNC_APP_PRIVATE_KEY=@<file>.pem`, and delete the old key on GitHub.
- Optional repository variable `SLIM_CHANNEL` (default `alpha`): the channel used in release tags.

## Releases

Tags have the form `v<Chatterino version>-slim-<channel>.<n>`, for example `v2.5.6-slim-alpha.1`. The Chatterino version comes from `project()` in `CMakeLists.txt`. `alpha` and `beta` tags become pre-releases; other channels, such as `stable`, become the latest release.

To release by hand:

```sh
git switch main && git pull --tags
tag=$(scripts/slim/next-tag.sh alpha)
git tag -a "$tag" -m "chatterino-slim $tag"
git push origin "$tag"
```

Before tagging, move the entries under **Unreleased** in [SLIM_CHANGELOG.md](../../SLIM_CHANGELOG.md) to a section named after the tag (`## v2.5.6-slim-alpha.1`). The release notes use that section, or **Unreleased** if there is none.

The Linux archive is built in upstream's `chatterino2-build-ubuntu-22.04` image with static Qt. Qt there links ICU dynamically, and other distributions ship other ICU versions, so the workflow copies `libicu*.so.70` into `lib/` and sets the binary's rpath to `$ORIGIN/../lib`. Static Qt has only the `xcb` and `minimal` platform plugins. For native Wayland, use [packaging/arch/PKGBUILD](../../packaging/arch/PKGBUILD), which builds against the system Qt.

The updater is disabled in fork builds (`-DCHATTERINO_UPDATER=OFF`). Upstream's updater would offer to replace the fork with upstream Chatterino.
