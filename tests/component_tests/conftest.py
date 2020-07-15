""" Fixtures for component tests """

import pytest

from esphome.core import CORE
from esphome.config import read_config
from esphome.__main__ import generate_cpp_contents


@pytest.fixture
def generate_main():
    """ Generates the C++ main.cpp file and returns it in string form """

    def generator(path: str) -> str:
        CORE.config_path = path
        CORE.config = read_config({})
        generate_cpp_contents(CORE.config)
        print(CORE.cpp_main_section)
        return CORE.cpp_main_section

    yield generator

    CORE.reset()
