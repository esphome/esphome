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

from esphome.framework_helpers import download_and_extract, str_to_lst_of_str

from .framework_west import _tools_path

_LOGGER = logging.getLogger(__name__)

# Same upstream release as nrf52/framework.py's SDK_NG_MINIMAL_MIRRORS (both fetch the
# zephyr-sdk-ng minimal archive), kept as an independent constant/env override rather
# than imported from nrf52 -- the two platforms are architecturally independent (see
# issue #132's discussion of why their caches aren't shared) and this avoids a
# zephyr -> nrf52 import dependency.
SDK_NG_MINIMAL_MIRRORS = str_to_lst_of_str(
    os.environ.get(
        "ESPHOME_ZEPHYR_SDK_NG_MINIMAL_MIRRORS",
        "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v{VERSION}/zephyr-sdk-{VERSION}_{sysname}-{machine}_minimal.{extension}",
    )
)


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
    return _tools_path() / "toolchains" / f"zephyr-sdk-{sdk_version}"


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
        substitutions = {
            "VERSION": sdk_version,
            "sysname": os_tag,
            "machine": arch,
            "extension": "tar.xz",
        }
        _LOGGER.info("Downloading Zephyr SDK minimal v%s ...", sdk_version)
        try:
            # Downloaded next to the destination (not a temp dir) so an
            # interrupted download's .part file resumes on the next run;
            # extracted directly into sdk_path -- archive_extract_all strips
            # the archive's single top-level wrapper directory automatically.
            download_and_extract(
                SDK_NG_MINIMAL_MIRRORS,
                substitutions,
                toolchains_dir / f"zephyr-sdk-{sdk_version}.minimal.archive",
                sdk_path,
                progress_header="Extracting",
            )
        except Exception:
            # sdk_path isn't wiped on the happy path elsewhere (other
            # toolchains may share it), so a failed/partial extraction must
            # not leave it looking complete to the next run's existence check.
            shutil.rmtree(sdk_path, ignore_errors=True)
            raise

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
