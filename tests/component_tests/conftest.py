"""Fixtures for component tests."""

from pathlib import Path
import sys

# Add package root to python path
here = Path(__file__).parent
package_root = here.parent.parent
sys.path.insert(0, package_root.as_posix())

import pytest  # noqa: E402

from esphome.__main__ import generate_cpp_contents  # noqa: E402
from esphome.config import read_config  # noqa: E402
from esphome.core import CORE  # noqa: E402


@pytest.fixture
def generate_main():
    """Generates the C++ main.cpp file and returns it in string form."""

    def generator(path: str) -> str:
        CORE.config_path = path
        CORE.config = read_config({})
        generate_cpp_contents(CORE.config)
        print(CORE.cpp_main_section)
        return CORE.cpp_main_section

    yield generator

    CORE.reset()
