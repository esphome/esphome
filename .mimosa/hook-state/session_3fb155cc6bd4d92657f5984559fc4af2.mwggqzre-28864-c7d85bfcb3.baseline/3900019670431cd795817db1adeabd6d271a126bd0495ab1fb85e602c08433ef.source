"""Download and install the Zephyr SDK and a cross-compiler toolchain."""

from __future__ import annotations

import logging
import os
from pathlib import Path
import platform

import esphome.config_validation as cv
from esphome.core import EsphomeError
from esphome.framework_helpers import download_and_extract, rmdir, str_to_lst_of_str

_LOGGER = logging.getLogger(__name__)

ARM_TOOLCHAIN: str = "arm-zephyr-eabi"

# SDK 0.x archives
_TOOLCHAIN_DEFAULT_MIRROR: str = "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v{VERSION}/toolchain_{sysname}-{machine}_{toolchain}.{extension}"
SDK_NG_TOOLCHAIN_MIRRORS: list[str] = str_to_lst_of_str(
    os.environ.get("ESPHOME_SDK_NG_TOOLCHAIN_MIRRORS", _TOOLCHAIN_DEFAULT_MIRROR)
)
# SDK 1.0+ archives
_TOOLCHAIN_GNU_DEFAULT_MIRROR: str = "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v{VERSION}/toolchain_gnu_{sysname}-{machine}_{toolchain}.{extension}"
SDK_NG_TOOLCHAIN_GNU_MIRRORS: list[str] = str_to_lst_of_str(
    os.environ.get(
        "ESPHOME_SDK_NG_TOOLCHAIN_GNU_MIRRORS", _TOOLCHAIN_GNU_DEFAULT_MIRROR
    )
)

# Minimal SDK provides cmake discovery files (Zephyr-sdkConfig.cmake) and
# host tools (dtc etc.) required by the Zephyr cmake build system.
SDK_NG_MINIMAL_MIRRORS: list[str] = str_to_lst_of_str(
    os.environ.get(
        "ESPHOME_SDK_NG_MINIMAL_MIRRORS",
        "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v{VERSION}/zephyr-sdk-{VERSION}_{sysname}-{machine}_minimal.{extension}",
    )
)


def get_sdk_host_info() -> tuple[str, str, str]:
    """Return (sysname, machine, extension) for the current host."""
    extension = "tar.xz"
    sysname = platform.system().lower()
    machine = platform.machine()
    if machine == "arm64":
        machine = "aarch64"
    if sysname == "darwin":
        sysname = "macos"
    elif sysname == "windows":
        machine = "x86_64"
        extension = "7z"
    return sysname, machine, extension


def _is_sdk_ng_1(version: str) -> bool:
    """SDK 1.0 moved the GNU toolchains under gnu/ and renamed their archives to toolchain_gnu_*."""
    return cv.Version.parse(version) >= cv.Version(1, 0, 0)


def sdk_toolchain_path(sdk_path: Path, version: str, toolchain: str) -> Path:
    """The directory of ``toolchain`` inside the SDK at ``sdk_path``."""
    if _is_sdk_ng_1(version):
        return sdk_path / "gnu" / toolchain
    return sdk_path / toolchain


def _toolchain_mirrors(version: str, toolchain: str) -> list[str]:
    """The toolchain mirrors for SDK ``version``.

    Overrides written before ``{toolchain}`` existed download the Arm toolchain,
    so they are refused for any other toolchain.
    """
    if _is_sdk_ng_1(version):
        env_name, mirrors, default = (
            "ESPHOME_SDK_NG_TOOLCHAIN_GNU_MIRRORS",
            SDK_NG_TOOLCHAIN_GNU_MIRRORS,
            _TOOLCHAIN_GNU_DEFAULT_MIRROR,
        )
    else:
        env_name, mirrors, default = (
            "ESPHOME_SDK_NG_TOOLCHAIN_MIRRORS",
            SDK_NG_TOOLCHAIN_MIRRORS,
            _TOOLCHAIN_DEFAULT_MIRROR,
        )
    if toolchain == ARM_TOOLCHAIN:
        return mirrors
    for template in mirrors:
        if "{toolchain}" not in template:
            raise EsphomeError(
                f"{env_name} has no {{toolchain}}, so it can't download {toolchain}: "
                f"{template}\n"
                "Put {toolchain} in place of the toolchain name in the URL, as in "
                f"the default {default}, or unset {env_name}."
            )
    return mirrors


def install_sdk(sdk_path: Path, version: str, toolchain: str) -> None:
    """Install the minimal SDK ``version`` and one GNU toolchain into ``sdk_path``.

    Anything already at ``sdk_path`` is removed first. The caller holds the install
    lock and writes its own marker once this returns.
    """
    toolchain_mirrors = _toolchain_mirrors(version, toolchain)
    rmdir(sdk_path, msg=f"Clean up {version} toolchain environment")
    sysname, machine, extension = get_sdk_host_info()
    substitutions = {
        "VERSION": version,
        "sysname": sysname,
        "machine": machine,
        "extension": extension,
        "toolchain": toolchain,
    }
    # Downloaded next to the destination (not a temp file) so an
    # interrupted download's .part file resumes on the next run.
    # Extraction strips the archive's single root.
    for mirrors, extract_dir, what, slug in (
        (SDK_NG_MINIMAL_MIRRORS, sdk_path, "Zephyr SDK minimal", "minimal"),
        (
            toolchain_mirrors,
            sdk_toolchain_path(sdk_path, version, toolchain),
            "toolchain",
            "toolchain",
        ),
    ):
        _LOGGER.info("Downloading %s %s ...", version, what)
        download_and_extract(
            mirrors,
            substitutions,
            sdk_path.with_name(f"{sdk_path.name}.{slug}.archive"),
            extract_dir,
            progress_header="Extracting",
        )
