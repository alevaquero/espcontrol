# AGENTS.md

## Communication

- The user is technical but not development-oriented. Explain changes in approachable terms and avoid unnecessary implementation detail.
- Keep final updates concise: what changed, how it was checked, and what the user needs to test.

## Development workflow

- This repository is a standalone fork (alevaquero/espcontrol, git remote `fork`). It no longer syncs with the original jtenniswood/espcontrol project: never pull from or push to it.
- Work in the single checkout at the repository root; do not create extra git worktrees unless the user asks or two unfinished changes must be tested side by side.
- Treat `main` as the stable branch. Local `main` tracks `fork/main`.
- For normal code, firmware, configuration, UI, or documentation changes, create a short-lived branch from the latest `main` in that checkout (`git switch -c <name> main`).
- Use short, descriptive branch names like `fix-display-timeout` or `update-pr-workflow`; do not include `codex` in branch names or PR titles.
- Keep each branch focused on one bug fix, feature, device change, cleanup, or documentation change. If a request starts to include unrelated work, keep it out unless the user explicitly asks to include it.
- Build and flash the test panel from that branch. Use a real version so auto-update does not replace the test build: `ESPCONTROL_FIRMWARE_VERSION=vX.Y.Z ESPHOME_BIN=.venv/bin/esphome python3 scripts/local_esphome.py devices/esp32-p4-wifi6-touch-lcd-5/dev-local.yaml compile`, then the same command with `upload --device <panel IP>`. Components build from the committed `HEAD`, so commit before compiling; `upload` does not compile.
- Commit completed changes, push the branch, and open a pull request marked ready for review. GitHub Actions is disabled on this fork, so run the relevant checks locally and say in the PR which ones ran.
- Leave the pull request open until the user confirms they have tested it, then squash-merge it, delete the branch, and switch back to `main`.
- After merging, publish the tested build as the panel's online update: `ESPHOME_BIN=.venv/bin/esphome python3 scripts/publish_fork_firmware.py devices/esp32-p4-wifi6-touch-lcd-5/dev-local.yaml --version vX.Y.Z` (add `--skip-compile` to publish the build already on the panel).
- Do not close related GitHub issues until the user confirms the fix works.
- Only work directly on `main` when the user explicitly asks for it, or for a tiny documentation-only change where a PR would add no value.

## Pull requests

- PR descriptions should explain the purpose of the change, the practical impact, and how it was checked.
- Include clear testing notes so the user can test the branch independently of `main`.
- If firmware needs flashing, name the affected display or device in the PR body.
- Distinguish automated checks from physical device testing. A compile/build pass is not the same as user-confirmed device testing.
- Use the automated PR testing guidance as the starting point for the PR body whenever it is available.
