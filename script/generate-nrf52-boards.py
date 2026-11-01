#!/usr/bin/env python3
"""Update the generated board data in esphome/components/nrf52/boards.py.

For each nRF Connect SDK version below, the board files of that SDK and of the Zephyr
version it uses are fetched and every nRF52 board device tree is read.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

import yaml

from esphome.components.nrf52.generate_boards import (
    BoardInfo,
    load_boards,
    render,
    replace_generated,
)
from esphome.helpers import write_file_if_changed

# nRF Connect SDK versions to read the boards of. Code that uses the data picks the newest
# version that is not newer than the configured framework version.
SDK_VERSIONS = ("2.9.2", "3.2.0", "3.4.0")
SDK_NRF_REPO = "https://github.com/nrfconnect/sdk-nrf"
# Only what the device trees and their includes need
SPARSE_PATHS = ("/west.yml", "/boards/", "/dts/", "/include/")

root = Path(__file__).parent.parent
boards_file_path = root / "esphome" / "components" / "nrf52" / "boards.py"


def fetch(repo: str, revision: str, path: Path) -> None:
    """Check out the device tree parts of one revision, without history."""

    def git(*args: str) -> None:
        subprocess.run(["git", "-C", str(path), *args], check=True)

    path.mkdir(parents=True)
    git("init", "-q")
    git("remote", "add", "origin", repo)
    git("sparse-checkout", "set", "--no-cone", *SPARSE_PATHS)
    git("fetch", "-q", "--depth", "1", "--filter=blob:none", "origin", revision)
    git("-c", "advice.detachedHead=false", "checkout", "-q", "FETCH_HEAD")


def zephyr_project(west_yml: Path) -> tuple[str, str]:
    """Return the repository URL and revision of Zephyr from an SDK west.yml manifest."""
    manifest = yaml.safe_load(west_yml.read_text(encoding="utf-8"))["manifest"]
    remotes = {remote["name"]: remote["url-base"] for remote in manifest["remotes"]}
    default_remote = manifest.get("defaults", {}).get("remote")
    for project in manifest["projects"]:
        if project["name"] == "zephyr":
            url_base = remotes[project.get("remote", default_remote)]
            return f"{url_base}/{project.get('repo-path', 'zephyr')}", project[
                "revision"
            ]
    raise ValueError(f"{west_yml} has no zephyr project")


def load_version(version: str, tempdir: Path) -> dict[str, BoardInfo]:
    nrf = tempdir / f"nrf-{version}"
    fetch(SDK_NRF_REPO, f"v{version}", nrf)
    zephyr = tempdir / f"zephyr-{version}"
    fetch(*zephyr_project(nrf / "west.yml"), zephyr)
    return load_boards([zephyr, nrf])


def main(check: bool) -> None:
    with tempfile.TemporaryDirectory() as tempdir:
        versions = {
            version: load_version(version, Path(tempdir)) for version in SDK_VERSIONS
        }

    existing_content = boards_file_path.read_text(encoding="utf-8")
    content = replace_generated(existing_content, render(versions))

    if check:
        if existing_content != content:
            print("esphome/components/nrf52/boards.py is not up to date.")
            print("Please run `script/generate-nrf52-boards.py`")
            sys.exit(1)
        print("esphome/components/nrf52/boards.py is up to date")
    elif write_file_if_changed(boards_file_path, content):
        print("nRF52 boards updated successfully.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--check",
        help="Check if the boards.py file is up to date.",
        action="store_true",
    )
    args = parser.parse_args()
    main(args.check)
