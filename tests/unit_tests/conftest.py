"""
ESPHome Unittests
~~~~~~~~~~~~~~~~~

Configuration file for unit tests.

If adding unit tests ensure that they are fast. Slower integration tests should
not be part of a unit test suite.

"""

from collections.abc import Generator
from pathlib import Path
import sys
from unittest.mock import Mock, patch

import pytest

from esphome.core import CORE

here = Path(__file__).parent

# Configure location of package root
package_root = here.parent.parent
sys.path.insert(0, package_root.as_posix())


@pytest.fixture(autouse=True)
def reset_core():
    """Reset CORE after each test."""
    yield
    CORE.reset()


@pytest.fixture
def fixture_path() -> Path:
    """
    Location of all fixture files.
    """
    return here / "fixtures"


@pytest.fixture
def setup_core(tmp_path: Path) -> Path:
    """Set up CORE with test paths."""
    CORE.config_path = tmp_path / "test.yaml"
    return tmp_path


@pytest.fixture
def mock_write_file_if_changed() -> Generator[Mock, None, None]:
    """Mock write_file_if_changed for storage_json."""
    with patch("esphome.storage_json.write_file_if_changed") as mock:
        yield mock


@pytest.fixture
def mock_copy_file_if_changed() -> Generator[Mock, None, None]:
    """Mock copy_file_if_changed for core.config."""
    with patch("esphome.core.config.copy_file_if_changed") as mock:
        yield mock


@pytest.fixture
def mock_run_platformio_cli() -> Generator[Mock, None, None]:
    """Mock run_platformio_cli for platformio_api."""
    with patch("esphome.platformio_api.run_platformio_cli") as mock:
        yield mock


@pytest.fixture
def mock_run_platformio_cli_run() -> Generator[Mock, None, None]:
    """Mock run_platformio_cli_run for platformio_api."""
    with patch("esphome.platformio_api.run_platformio_cli_run") as mock:
        yield mock


@pytest.fixture
def mock_decode_pc() -> Generator[Mock, None, None]:
    """Mock _decode_pc for platformio_api."""
    with patch("esphome.platformio_api._decode_pc") as mock:
        yield mock


@pytest.fixture
def mock_run_external_command() -> Generator[Mock, None, None]:
    """Mock run_external_command for platformio_api."""
    with patch("esphome.platformio_api.run_external_command") as mock:
        yield mock


@pytest.fixture
def mock_run_git_command() -> Generator[Mock, None, None]:
    """Mock run_git_command for git module."""
    with patch("esphome.git.run_git_command") as mock:
        yield mock


@pytest.fixture
def mock_subprocess_run() -> Generator[Mock, None, None]:
    """Mock subprocess.run for testing."""
    with patch("subprocess.run") as mock:
        yield mock


@pytest.fixture
def mock_get_idedata() -> Generator[Mock, None, None]:
    """Mock get_idedata for platformio_api."""
    with patch("esphome.platformio_api.get_idedata") as mock:
        yield mock


@pytest.fixture
def mock_get_component() -> Generator[Mock, None, None]:
    """Mock get_component for config module."""
    with patch("esphome.config.get_component") as mock:
        yield mock
