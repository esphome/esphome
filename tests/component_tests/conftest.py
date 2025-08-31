"""Fixtures for component tests."""

from __future__ import annotations

from collections.abc import Callable, Generator
from pathlib import Path
import sys
from typing import Any

import pytest

from esphome import config, final_validate
from esphome.const import (
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PlatformFramework,
)
from esphome.types import ConfigType

# Add package root to python path
here = Path(__file__).parent
package_root = here.parent.parent
sys.path.insert(0, package_root.as_posix())

from esphome.__main__ import generate_cpp_contents  # noqa: E402
from esphome.config import Config, read_config  # noqa: E402
from esphome.core import CORE  # noqa: E402

from .types import SetCoreConfigCallable  # noqa: E402


@pytest.fixture(autouse=True)
def config_path(request: pytest.FixtureRequest) -> Generator[None]:
    """Set CORE.config_path to the component's config directory and reset it after the test."""
    original_path = CORE.config_path
    config_dir = Path(request.fspath).parent / "config"

    # Check if config directory exists, if not use parent directory
    if config_dir.exists():
        # Set config_path to a dummy yaml file in the config directory
        # This ensures CORE.config_dir points to the config directory
        CORE.config_path = str(config_dir / "dummy.yaml")
    else:
        CORE.config_path = str(Path(request.fspath).parent / "dummy.yaml")

    yield
    CORE.config_path = original_path


@pytest.fixture(autouse=True)
def reset_core() -> Generator[None]:
    """Reset CORE after each test."""
    yield
    CORE.reset()


@pytest.fixture
def set_core_config() -> Generator[SetCoreConfigCallable]:
    """Fixture to set up the core configuration for tests."""

    def setter(
        platform_framework: PlatformFramework,
        /,
        *,
        core_data: ConfigType | None = None,
        platform_data: ConfigType | None = None,
        full_config: dict[str, ConfigType] | None = None,
    ) -> None:
        platform, framework = platform_framework.value

        # Set base core configuration
        CORE.data[KEY_CORE] = {
            KEY_TARGET_PLATFORM: platform.value,
            KEY_TARGET_FRAMEWORK: framework.value,
        }

        # Update with any additional core data
        if core_data:
            CORE.data[KEY_CORE].update(core_data)

        # Set platform-specific data
        if platform_data:
            CORE.data[platform.value] = platform_data

        config.path_context.set([])
        final_validate.full_config.set(full_config or Config())

    yield setter


@pytest.fixture
def set_component_config() -> Callable[[str, Any], None]:
    """
    Fixture to set a component configuration in the mock config.
    This must be used after the core configuration has been set up.
    """

    def setter(name: str, value: Any) -> None:
        final_validate.full_config.get()[name] = value

    return setter


@pytest.fixture
def component_fixture_path(request: pytest.FixtureRequest) -> Callable[[str], Path]:
    """Return a function to get absolute paths relative to the component's fixtures directory."""

    def _get_path(file_name: str) -> Path:
        """Get the absolute path of a file relative to the component's fixtures directory."""
        return (Path(request.fspath).parent / "fixtures" / file_name).absolute()

    return _get_path


@pytest.fixture
def component_config_path(request: pytest.FixtureRequest) -> Callable[[str], Path]:
    """Return a function to get absolute paths relative to the component's config directory."""

    def _get_path(file_name: str) -> Path:
        """Get the absolute path of a file relative to the component's config directory."""
        return (Path(request.fspath).parent / "config" / file_name).absolute()

    return _get_path


@pytest.fixture
def generate_main() -> Generator[Callable[[str | Path], str]]:
    """Generates the C++ main.cpp from a given yaml file and returns it in string form."""

    def generator(path: str | Path) -> str:
        CORE.config_path = str(path)
        CORE.config = read_config({})
        generate_cpp_contents(CORE.config)
        return CORE.cpp_main_section

    yield generator
