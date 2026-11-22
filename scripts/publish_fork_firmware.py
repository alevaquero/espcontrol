#!/usr/bin/env python3
"""Build one device's firmware and publish it to this fork's firmware branch.

The panels check
https://raw.githubusercontent.com/alevaquero/espcontrol/firmware/<slug>/manifest.json
for online updates, so publishing only needs a git push, not GitHub Actions.

Usage:
  ESPHOME_BIN=... python3 scripts/publish_fork_firmware.py \
      devices/<slug>/dev-local.yaml --version v1.0.1 [--remote fork] [--no-push]
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import firmware_release  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
BRANCH = "firmware"


def git(*args: str, cwd: Path = ROOT, check: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(["git", *args], cwd=cwd, check=check, text=True, capture_output=True)


def compile_firmware(yaml_path: Path, version: str) -> None:
    env = dict(os.environ, ESPCONTROL_FIRMWARE_VERSION=version)
    command = [sys.executable, str(ROOT / "scripts" / "local_esphome.py"), str(yaml_path), "compile"]
    subprocess.run(command, cwd=ROOT, env=env, check=True)


def newest_build_dir(yaml_path: Path) -> Path:
    candidates = list((yaml_path.parent / ".esphome" / "build").glob("*/build/firmware.ota.bin"))
    if not candidates:
        raise SystemExit(f"No firmware.ota.bin found under {yaml_path.parent / '.esphome' / 'build'}")
    return max(candidates, key=lambda path: path.stat().st_mtime).parent


def secret_values(yaml_path: Path) -> list[bytes]:
    secrets = yaml_path.parent / "secrets.yaml"
    if not secrets.is_file():
        return []
    values = []
    for line in secrets.read_text(encoding="utf-8").splitlines():
        key, sep, value = line.partition(":")
        value = value.strip().strip("'\"")
        if sep and not key.lstrip().startswith("#") and len(value) >= 6:
            values.append(value.encode())
    return values


def assert_no_secrets(paths: list[Path], yaml_path: Path) -> None:
    values = secret_values(yaml_path)
    for path in paths:
        data = path.read_bytes()
        if any(value in data for value in values):
            raise SystemExit(f"{path.name} contains a value from secrets.yaml; not publishing it")


def assert_version(ota: Path, version: str) -> None:
    # Local builds lack the dashboard import metadata that
    # firmware_release.assert_binary_version also expects.
    expected = f"Project {firmware_release.PROJECT_NAME} version {version}"
    if expected not in firmware_release.printable_strings(ota):
        raise SystemExit(f"{ota} does not contain firmware version {version}")


def write_manifest(slug: str, version: str, ota: Path, out: Path) -> None:
    data = {
        "name": "Espcontrol",
        "version": version,
        "home_assistant_domain": "esphome",
        "builds": [
            {
                "chipFamily": firmware_release.expected_chip_family_for_slug(slug),
                "parts": [{"path": f"{slug}.factory.bin", "offset": 0}],
                "ota": {"path": f"{slug}.ota.bin", "md5": firmware_release.md5sum(ota)},
            },
        ],
    }
    out.write_text(json.dumps(data, indent=2) + "\n")


def write_versions(slug: str, version: str, ota: Path, out: Path) -> None:
    # The web page's firmware list; only the published version is kept.
    entry = {"version": version, "release_url": "", "ota": {"path": f"{slug}.ota.bin", "md5": firmware_release.md5sum(ota)}}
    out.write_text(json.dumps({"device": slug, "versions": [entry]}, indent=2) + "\n")


def publish(slug: str, version: str, build_dir: Path, remote: str, push: bool) -> None:
    fetched = git("fetch", remote, BRANCH, check=False).returncode == 0
    with tempfile.TemporaryDirectory(prefix="espcontrol-firmware-") as tmp:
        work = Path(tmp) / "firmware"
        if fetched:
            git("worktree", "add", "--detach", str(work), f"{remote}/{BRANCH}")
        else:
            git("worktree", "add", "--detach", str(work))
            git("rm", "-rq", ".", cwd=work)
        try:
            # Keep a single commit so old binaries do not pile up in the history.
            git("checkout", "-q", "--orphan", f"publish-{slug}", cwd=work)
            target = work / slug
            target.mkdir(exist_ok=True)
            shutil.copyfile(build_dir / "firmware.ota.bin", target / f"{slug}.ota.bin")
            shutil.copyfile(build_dir / "firmware.factory.bin", target / f"{slug}.factory.bin")
            write_manifest(slug, version, target / f"{slug}.ota.bin", target / "manifest.json")
            write_versions(slug, version, target / f"{slug}.ota.bin", target / "versions.json")
            git("add", "-A", cwd=work)
            git("commit", "-qm", f"Publish {slug} {version}", cwd=work)
            if push:
                git("push", "--force", remote, f"HEAD:refs/heads/{BRANCH}", cwd=work)
                print(f"Published {slug} {version} to {remote}/{BRANCH}")
            else:
                print(f"Built {slug} {version}; not pushed (--no-push)")
        finally:
            git("worktree", "remove", "--force", str(work), check=False)
            git("branch", "-D", f"publish-{slug}", check=False)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("yaml", help="devices/<slug>/dev-local.yaml or another build YAML")
    parser.add_argument("--version", required=True)
    parser.add_argument("--remote", default="fork")
    parser.add_argument("--no-push", action="store_true")
    parser.add_argument("--skip-compile", action="store_true", help="Publish the last build as is")
    args = parser.parse_args()

    # The web page shows anything not shaped like v1.2.3 as "Dev build".
    if not re.fullmatch(r"v[0-9]+(\.[0-9]+){2}([-+][0-9A-Za-z.-]+)?", args.version):
        parser.error("--version must look like v1.2.3")
    yaml_path = Path(args.yaml).resolve()
    slug = yaml_path.parent.name
    if not args.skip_compile:
        compile_firmware(yaml_path, args.version)
    build_dir = newest_build_dir(yaml_path)
    ota = build_dir / "firmware.ota.bin"
    assert_version(ota, args.version)
    assert_no_secrets([ota, build_dir / "firmware.factory.bin"], yaml_path)
    publish(slug, args.version, build_dir, args.remote, not args.no_push)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
