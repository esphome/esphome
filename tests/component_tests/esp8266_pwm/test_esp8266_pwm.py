"""Tests for the esp8266_pwm output codegen."""

from collections.abc import Callable
from pathlib import Path


def test_default_frequency_is_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The 1 kHz default already lives in the C++ initializer."""
    main_cpp = generate_main(component_config_path("frequency.yaml"))

    assert "default_frequency->set_frequency(" not in main_cpp
    assert "custom_frequency->set_frequency(2000.0f);" in main_cpp
