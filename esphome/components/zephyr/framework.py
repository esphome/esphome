"""Install the Zephyr framework for Zephyr toolchain builds."""

from pathlib import Path
import time

from esphome.build_helpers.tools_cache import ZEPHYR_TOOLS_CACHE, tools_cache_path
from esphome.core import EsphomeError
from esphome.framework_helpers import install_lock

from .framework_helpers import ensure_venv
from .sdk_install import ensure_sdk

_REQUIREMENTS: Path = Path(__file__).parent / "requirements.txt"
# The SDK versions in use are unknown; a partial download untouched this long has no owner
_STALE_DOWNLOAD_AGE: int = 24 * 60 * 60


def get_zephyr_tools_path() -> Path:
    # Machine-global (OS user cache dir) so all projects share one install;
    # see espidf.framework.get_idf_tools_path for the location rationale.
    return tools_cache_path(*ZEPHYR_TOOLS_CACHE)


def _get_python_env_path(version: str) -> Path:
    return get_zephyr_tools_path() / "penvs" / version


def _get_toolchain_path(sdk_version: str, toolchain: str) -> Path:
    return (
        get_zephyr_tools_path() / "toolchains" / f"zephyr-sdk-{sdk_version}-{toolchain}"
    )


def check_and_install(version: str) -> None:
    with install_lock(get_zephyr_tools_path(), f"penv-{version}"):
        _check_and_install(version)


def _check_and_install(version: str) -> None:
    ensure_venv(_get_python_env_path(version), _REQUIREMENTS, version)


def _read_sdk_version(framework_path: Path) -> str:
    sdk_version_file = framework_path / "zephyr" / "SDK_VERSION"
    try:
        sdk_version = sdk_version_file.read_text(encoding="utf-8").strip()
    except OSError as err:
        raise EsphomeError(
            f"Can't read the Zephyr SDK version from {sdk_version_file}: {err}"
        ) from err
    if not sdk_version:
        raise EsphomeError(f"{sdk_version_file} names no Zephyr SDK version")
    return sdk_version


def install_toolchain(framework_path: Path, toolchain: str) -> Path:
    """Install the SDK named by the workspace's ``zephyr/SDK_VERSION``; returns its folder."""
    sdk_version = _read_sdk_version(framework_path)
    sdk_path = _get_toolchain_path(sdk_version, toolchain)
    cutoff = time.time() - _STALE_DOWNLOAD_AGE
    ensure_sdk(
        sdk_path,
        sdk_version,
        toolchain,
        keep_leftover=lambda leftover: leftover.stat().st_mtime >= cutoff,
        lock_root=get_zephyr_tools_path(),
    )
    return sdk_path
