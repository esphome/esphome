"""Install the Zephyr framework for Zephyr toolchain builds."""

from pathlib import Path
import time

from esphome.build_helpers.tools_cache import ZEPHYR_TOOLS_CACHE, tools_cache_path
from esphome.core import EsphomeError
from esphome.framework_helpers import get_python_env_executable_path, install_lock

from .framework_helpers import (
    ensure_venv,
    ensure_zephyr_requirements,
    fetch_missing_west_projects,
    install_framework,
    patch_gen_defines_dts_path,
    patch_west_build_source_dir_relpath,
)
from .sdk_install import ensure_sdk
from .variants import ZephyrSDK

_REQUIREMENTS: Path = Path(__file__).parent / "requirements.txt"
# The SDK versions in use are unknown; a partial download untouched this long has no owner,
# so a download interrupted more than a day ago starts over
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


def _get_framework_path(sdk: ZephyrSDK, version: str) -> Path:
    return get_zephyr_tools_path() / "frameworks" / f"{sdk.workspace_prefix}-{version}"


def check_and_install(sdk: ZephyrSDK, version: str, hals: frozenset[str]) -> None:
    """Install the workspace with the vendor ``hals`` the build needs; a workspace
    only gains HALs, since the variants of one version share it."""
    framework_path = _get_framework_path(sdk, version)
    with install_lock(get_zephyr_tools_path(), framework_path.name):
        _check_and_install(sdk, version, framework_path, hals)


def _check_and_install(
    sdk: ZephyrSDK, version: str, framework_path: Path, hals: frozenset[str]
) -> None:
    python_env_path = _get_python_env_path(version)
    env_python_path = get_python_env_executable_path(python_env_path, "python")
    # Every workspace of this version shares the venv
    with install_lock(get_zephyr_tools_path(), f"penv-{version}"):
        install_venv = ensure_venv(python_env_path, _REQUIREMENTS, version)

    sentinel = framework_path / ".ready"
    zephyr_reqs = framework_path / "zephyr" / "scripts" / "requirements.txt"
    projects = set(hals | sdk.source.required)
    if not sentinel.exists() or not zephyr_reqs.exists():
        install_framework(
            env_python_path, framework_path, sdk.source, version, projects
        )
        sentinel.touch()
    else:
        fetch_missing_west_projects(
            env_python_path, framework_path, sdk.source, version, projects
        )
    # Every run: existing installs need these too, and they are no-ops once applied
    patch_gen_defines_dts_path(framework_path)
    patch_west_build_source_dir_relpath(framework_path)

    with install_lock(get_zephyr_tools_path(), f"penv-{version}"):
        ensure_zephyr_requirements(
            python_env_path, framework_path, install_venv, version
        )


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

    def _keep_leftover(leftover: Path) -> bool:
        part = leftover.with_name(leftover.name.removesuffix(".meta"))
        for candidate in (part, part.with_name(f"{part.name}.meta")):
            try:
                if candidate.stat().st_mtime >= cutoff:
                    return True
            except FileNotFoundError:
                continue
        return False

    ensure_sdk(
        sdk_path,
        sdk_version,
        toolchain,
        keep_leftover=_keep_leftover,
        lock_root=get_zephyr_tools_path(),
    )
    return sdk_path
