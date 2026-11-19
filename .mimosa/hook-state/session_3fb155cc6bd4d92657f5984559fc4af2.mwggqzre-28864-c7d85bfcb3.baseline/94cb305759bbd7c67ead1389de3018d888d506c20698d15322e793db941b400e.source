"""Install the Zephyr framework for Zephyr toolchain builds."""

from pathlib import Path

from esphome.build_helpers.tools_cache import ZEPHYR_TOOLS_CACHE, tools_cache_path
from esphome.framework_helpers import install_lock

from .framework_helpers import ensure_venv

_REQUIREMENTS = Path(__file__).parent / "requirements.txt"


def get_zephyr_tools_path() -> Path:
    # Machine-global (OS user cache dir) so all projects share one install;
    # see espidf.framework.get_idf_tools_path for the location rationale.
    return tools_cache_path(*ZEPHYR_TOOLS_CACHE)


def _get_python_env_path(version: str) -> Path:
    return get_zephyr_tools_path() / "penvs" / version


def check_and_install(version: str) -> None:
    with install_lock(get_zephyr_tools_path(), f"penv-{version}"):
        _check_and_install(version)


def _check_and_install(version: str) -> None:
    ensure_venv(_get_python_env_path(version), _REQUIREMENTS, version)
