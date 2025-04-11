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

here = Path(__file__).parent

# Configure location of package root
package_root = here.parent.parent
sys.path.insert(0, package_root.as_posix())


@pytest.fixture
def fixture_path() -> Path:
    """
    Location of all fixture files.
    """
    return here / "fixtures"
