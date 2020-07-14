""" Tests for the binary sensor component """

from esphome.core import CORE
from esphome.config import read_config
from esphome.__main__ import generate_cpp_contents


def test_binary_sensor_config_value_internal_set():
    """
    Test that the "internal" config value is correctly set
    """
    # Given
    CORE.config_path = "tests/component_tests/binary_sensor/test_binary_sensor.yaml"
    CORE.config = read_config({})

    # When
    generate_cpp_contents(CORE.config)
    # print(CORE.cpp_main_section)

    # Then
    assert "bs_1->set_internal(true);" in CORE.cpp_main_section
    assert "bs_2->set_internal(false);" in CORE.cpp_main_section

    CORE.reset()
