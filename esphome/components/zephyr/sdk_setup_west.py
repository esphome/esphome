"""Download and install the Zephyr SDK for Zephyr-based builds.

native_sim uses the host GCC, so only host tools are installed.
Embedded targets (e.g. esp32_h2) additionally install the matching
cross-compiler toolchain from the Zephyr SDK.
"""

from __future__ import annotations

import logging
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.request

from esphome.core import CORE

_LOGGER = logging.getLogger(__name__)

_SDK_BASE_URL = "https://github.com/zephyrproject-rtos/sdk-ng/releases/download"


def _read_sdk_version(framework_path: Path) -> str:
    """Read the required SDK version from the framework's SDK_VERSION file."""
    sdk_version_file = framework_path / "zephyr" / "SDK_VERSION"
    try:
        return sdk_version_file.read_text().strip()
    except OSError as e:
        raise RuntimeError(
            f"Cannot read SDK_VERSION from {sdk_version_file}: {e}"
        ) from e


def _sdk_install_dir(sdk_version: str) -> Path:
    return CORE.data_dir / "sdk-zephyr" / "toolchains" / f"zephyr-sdk-{sdk_version}"


def _download_and_extract(
    url: str, tmp_file: Path, extract_tmp: Path, label: str
) -> None:
    try:
        _LOGGER.info("Downloading %s ...", label)
        urllib.request.urlretrieve(url, tmp_file)
    except Exception as e:
        tmp_file.unlink(missing_ok=True)
        raise RuntimeError(f"Can't download {label}: {e}") from e
    try:
        with tarfile.open(tmp_file, "r:xz") as tar:
            tar.extractall(extract_tmp)
    except Exception as e:
        raise RuntimeError(f"Can't extract {label}: {e}") from e
    finally:
        tmp_file.unlink(missing_ok=True)


def check_and_install(framework_path: Path, toolchain: str | None = None) -> Path:
    """Ensure the Zephyr SDK is installed.

    Args:
        framework_path: Path to the West workspace root.
        toolchain: Zephyr SDK toolchain name to install (e.g. ``"riscv64-zephyr-elf"``).
                   ``None`` installs host tools only (native_sim).

    Returns the SDK install dir.
    """
    sdk_version = _read_sdk_version(framework_path)
    sdk_path = _sdk_install_dir(sdk_version)
    # Sentinel encodes which toolchains are installed so adding a new toolchain
    # (e.g. riscv64-zephyr-elf) re-runs setup even if the minimal archive is already present.
    sentinel_name = (
        f".esphome_complete_{toolchain}" if toolchain else ".esphome_complete_host"
    )
    sentinel = sdk_path / sentinel_name

    if sentinel.exists():
        _LOGGER.debug("Zephyr SDK v%s already at %s", sdk_version, sdk_path)
        return sdk_path

    # sdk_path already existing means the base SDK is intact, just a different
    # toolchain/sentinel is missing -- don't wipe it, other toolchains may share this cache.
    if not sdk_path.exists():
        machine = platform.machine().lower()
        if machine in ("x86_64", "amd64"):
            arch = "x86_64"
        elif machine in ("aarch64", "arm64"):
            arch = "aarch64"
        else:
            raise RuntimeError(f"Unsupported CPU architecture: {machine}")

        if sys.platform.startswith("linux"):
            os_tag = "linux"
        elif sys.platform == "darwin":
            os_tag = "macos"
        else:
            raise RuntimeError(f"Unsupported OS for Zephyr SDK: {sys.platform}")

        toolchains_dir = sdk_path.parent
        toolchains_dir.mkdir(parents=True, exist_ok=True)
        extract_tmp = toolchains_dir / f"zephyr-sdk-{sdk_version}.tmp"
        extract_tmp.mkdir(parents=True, exist_ok=True)

        try:
            filename = f"zephyr-sdk-{sdk_version}_{os_tag}-{arch}_minimal.tar.xz"
            url = f"{_SDK_BASE_URL}/v{sdk_version}/{filename}"
            _download_and_extract(
                url,
                toolchains_dir / f"{filename}.tmp",
                extract_tmp,
                f"Zephyr SDK minimal v{sdk_version}",
            )

            entries = list(extract_tmp.iterdir())
            if len(entries) != 1 or not entries[0].is_dir():
                raise RuntimeError(
                    f"Unexpected archive layout in Zephyr SDK v{sdk_version}"
                )
            entries[0].rename(sdk_path)
        except Exception:
            shutil.rmtree(sdk_path, ignore_errors=True)
            raise
        finally:
            shutil.rmtree(extract_tmp, ignore_errors=True)

    setup_script = sdk_path / "setup.sh"
    if setup_script.exists():
        if toolchain:
            _LOGGER.info(
                "Installing Zephyr SDK %s toolchain v%s ...", toolchain, sdk_version
            )
            setup_args = [str(setup_script), "-t", toolchain]
        else:
            _LOGGER.info("Installing Zephyr SDK host tools v%s ...", sdk_version)
            setup_args = [str(setup_script), "-h"]
        result = subprocess.run(
            setup_args,
            env={**os.environ, "ZEPHYR_SDK_INSTALL_DIR": str(sdk_path)},
            check=False,
        )
        if result.returncode != 0:
            raise RuntimeError(f"setup.sh failed with exit code {result.returncode}")

    sentinel.touch()
    return sdk_path
