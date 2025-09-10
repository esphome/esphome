"""
ESPHome Unittests
~~~~~~~~~~~~~~~~~

Configuration file for unit tests.

If adding unit tests ensure that they are fast. Slower integration tests should
not be part of a unit test suite.

"""

from pathlib import Path
import sys

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
    CORE.config_path = str(tmp_path / "test.yaml")
    return tmp_path
