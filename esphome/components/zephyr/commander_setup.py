"""Download and install Silicon Labs' Simplicity Commander CLI.

Only needed for the `silabs` framework (see SILABS in variants/__init__.py):
that SDK's MCUboot signing hook (zephyr-silabs/zephyr/cmake/commander_sign.cmake)
shells out to the `commander` binary for secure-boot image signing whenever
CONFIG_MCUBOOT + CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256 are both set. Commander's
own release cadence is independent of the SDK version (build "1v24p1b1980" vs.
SDK "2026.6.0"), so it's tracked and cached separately here, the same way
sdk_setup_west.py tracks the Zephyr SDK/toolchain independently of the
framework version.
"""

from __future__ import annotations

import hashlib
import logging
from pathlib import Path
import platform
import shutil
import sys
import tarfile
import urllib.request

from esphome.core import CORE

_LOGGER = logging.getLogger(__name__)

_BASE_URL = (
    "https://updates.silabs.com/studio/v6/updates/update_site/archives/commander"
)

# version -> (build string embedded in the archive filename, {arch: sha256}).
# Verified against a real published build (github.com/NabuCasa/silabs-firmware-builder's
# Dockerfile, which fetches Commander the same way for its own CI). Only Linux archives
# have been confirmed to exist at this URL scheme -- add macOS/Windows entries once their
# filenames/checksums are verified the same way.
_RELEASES: dict[str, dict] = {
    "1.24.1": {
        "build": "1v24p1b1980",
        "x86_64": "3ba24eeaeb560e9db306a4d070e2bbe40b456701b4b87c53643a93ab1101b2c4",
        "aarch64": "99fd45e5064b00ace957b4d12c00bb3c3b33845b4e65793fde99d4960004e091",
    },
}
DEFAULT_VERSION = "1.24.1"


def _install_dir(version: str) -> Path:
    return CORE.data_dir / "sdk-silabs" / "commander" / version


def check_and_install(version: str | None) -> Path:
    """Ensure Simplicity Commander is installed. Returns its bin dir (to prepend to PATH)."""
    version = version or DEFAULT_VERSION
    release = _RELEASES.get(version)
    if release is None:
        raise RuntimeError(
            f"Unknown Simplicity Commander version '{version}' -- known versions: "
            f"{', '.join(_RELEASES)}. Check "
            "https://www.silabs.com/developers/mcu-programming-options for newer "
            "releases and their archive filenames/checksums."
        )

    if not sys.platform.startswith("linux"):
        raise RuntimeError(
            "Automatic Simplicity Commander installation is only supported on Linux "
            "currently. Install Commander manually and ensure it's on PATH: "
            "https://www.silabs.com/developers/mcu-programming-options"
        )
    machine = platform.machine().lower()
    if machine in ("x86_64", "amd64"):
        arch = "x86_64"
    elif machine in ("aarch64", "arm64"):
        arch = "aarch64"
    else:
        raise RuntimeError(
            f"Unsupported CPU architecture for Simplicity Commander: {machine}"
        )
    checksum = release[arch]

    install_dir = _install_dir(version)
    sentinel = install_dir / ".esphome_complete"
    if sentinel.exists():
        _LOGGER.debug("Simplicity Commander v%s already at %s", version, install_dir)
        return install_dir

    filename = f"Commander_linux_{arch}_{release['build']}.tar.bz"
    url = f"{_BASE_URL}/{version}/{filename}"

    parent = install_dir.parent
    parent.mkdir(parents=True, exist_ok=True)
    tmp_file = parent / f"{filename}.tmp"
    extract_tmp = parent / f"{version}.tmp"

    try:
        _LOGGER.info("Downloading Simplicity Commander v%s ...", version)
        urllib.request.urlretrieve(url, tmp_file)

        digest = hashlib.sha256(tmp_file.read_bytes()).hexdigest()
        if digest != checksum:
            raise RuntimeError(
                f"Simplicity Commander v{version} download checksum mismatch: "
                f"expected {checksum}, got {digest}"
            )

        extract_tmp.mkdir(parents=True, exist_ok=True)
        with tarfile.open(tmp_file, "r:bz2") as tar:
            tar.extractall(extract_tmp)

        entries = list(extract_tmp.iterdir())
        if len(entries) != 1 or not entries[0].is_dir():
            raise RuntimeError(
                f"Unexpected archive layout for Simplicity Commander v{version}"
            )
        shutil.rmtree(install_dir, ignore_errors=True)
        entries[0].rename(install_dir)
    except Exception:
        shutil.rmtree(install_dir, ignore_errors=True)
        raise
    finally:
        tmp_file.unlink(missing_ok=True)
        shutil.rmtree(extract_tmp, ignore_errors=True)

    sentinel.touch()
    return install_dir
