"""Download and install the Zephyr SDK and a cross-compiler toolchain."""

from __future__ import annotations

import logging
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

from esphome.framework_helpers import (
    download_and_extract,
    install_lock,
    str_to_lst_of_str,
)

from .framework_west import get_zephyr_tools_path

_LOGGER = logging.getLogger(__name__)

# The sdk-ng minimal archive and its override, as in nrf52/framework.py's
# SDK_NG_MINIMAL_MIRRORS; defined here so zephyr does not import nrf52. nrf52 and
# Zephyr toolchain builds keep separate caches.
SDK_NG_MINIMAL_MIRRORS = str_to_lst_of_str(
    os.environ.get(
        "ESPHOME_SDK_NG_MINIMAL_MIRRORS",
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
    return get_zephyr_tools_path() / "toolchains" / f"zephyr-sdk-{sdk_version}"


def _sdk_platform_info() -> tuple[str, str, str]:
    """Return (sysname, machine, extension) of the SDK minimal archive for this host.

    Mirrors the names of the zephyr-sdk-ng release assets
    (``zephyr-sdk-<ver>_<sysname>-<machine>_minimal.<extension>``). Windows ships a
    single ``windows-x86_64`` ``.7z`` build (``aarch64`` Windows runs it under x64
    emulation), the other hosts a ``.tar.xz``.
    """
    machine = platform.machine().lower()
    if machine in ("x86_64", "amd64"):
        arch = "x86_64"
    elif machine in ("aarch64", "arm64"):
        arch = "aarch64"
    else:
        raise RuntimeError(f"Unsupported CPU architecture: {machine}")

    if sys.platform.startswith("linux"):
        return "linux", arch, "tar.xz"
    if sys.platform == "darwin":
        return "macos", arch, "tar.xz"
    if sys.platform == "win32":
        return "windows", "x86_64", "7z"
    raise RuntimeError(f"Unsupported OS for Zephyr SDK: {sys.platform}")


def _sdk_setup_command(sdk_path: Path, toolchain: str) -> list[str]:
    """Return the command that installs ``toolchain`` into the extracted minimal SDK.

    The Windows archive ships ``setup.cmd`` (options introduced by ``/``), the other
    hosts ``setup.sh`` (options introduced by ``-``), as in ``west sdk install``.
    """
    if sys.platform == "win32":
        return [str(sdk_path / "setup.cmd"), "/t", toolchain]
    return [str(sdk_path / "setup.sh"), "-t", toolchain]


def check_and_install(framework_path: Path, toolchain: str) -> Path:
    """Ensure the Zephyr SDK is installed.

    Args:
        framework_path: Path to the West workspace root.
        toolchain: Zephyr SDK toolchain name to install (e.g. ``"riscv64-zephyr-elf"``).

    Returns the SDK install dir.
    """
    sdk_version = _read_sdk_version(framework_path)
    sdk_path = _sdk_install_dir(sdk_version)
    # Sentinel encodes which toolchains are installed so adding a new toolchain
    # (e.g. riscv64-zephyr-elf) re-runs setup even if the minimal archive is already present.
    sentinel = sdk_path / f".esphome_complete_{toolchain}"

    if sentinel.exists():
        _LOGGER.debug("Zephyr SDK v%s already at %s", sdk_version, sdk_path)
        return sdk_path

    with install_lock(get_zephyr_tools_path(), sdk_path.name):
        if sentinel.exists():  # another build installed it while this one waited
            return sdk_path

        # Wipe only an unfinished extraction; an intact base is shared across toolchains.
        base_marker = sdk_path / ".esphome_base_complete"
        if not base_marker.exists():
            shutil.rmtree(sdk_path, ignore_errors=True)
            os_tag, arch, extension = _sdk_platform_info()

            toolchains_dir = sdk_path.parent
            substitutions = {
                "VERSION": sdk_version,
                "sysname": os_tag,
                "machine": arch,
                "extension": extension,
            }
            _LOGGER.info("Downloading Zephyr SDK minimal v%s ...", sdk_version)
            # Not a temp dir, so an interrupted download's .part file resumes next run.
            download_and_extract(
                SDK_NG_MINIMAL_MIRRORS,
                substitutions,
                toolchains_dir / f"zephyr-sdk-{sdk_version}.minimal.archive",
                sdk_path,
                progress_header="Extracting",
            )
            base_marker.touch()

        setup_cmd = _sdk_setup_command(sdk_path, toolchain)
        setup_script = Path(setup_cmd[0])
        if not setup_script.exists():
            raise RuntimeError(f"Zephyr SDK at {sdk_path} has no {setup_script.name}")
        _LOGGER.info(
            "Installing Zephyr SDK %s toolchain v%s ...", toolchain, sdk_version
        )
        result = subprocess.run(
            setup_cmd,
            env={**os.environ, "ZEPHYR_SDK_INSTALL_DIR": str(sdk_path)},
            check=False,
        )
        if result.returncode != 0:
            raise RuntimeError(
                f"{setup_script.name} failed with exit code {result.returncode}"
            )

        sentinel.touch()
    return sdk_path
