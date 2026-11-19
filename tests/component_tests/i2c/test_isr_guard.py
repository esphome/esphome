"""The I2C ISR guard define and its linker wrap are emitted together, only for classic ESP32."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

# Spelled out so a typo in the component's flag fails here
_WRAP_FLAG = "-Wl,--wrap=esp_intr_alloc_intrstatus"


@pytest.mark.parametrize(
    ("config_file", "expected"),
    [
        pytest.param("isr_guard_esp32.yaml", True, id="esp32"),
        pytest.param("isr_guard_esp32c3.yaml", False, id="esp32c3"),
    ],
)
def test_guard_define_and_wrap_flag_go_together(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    expected: bool,
) -> None:
    generate_main(component_config_path(config_file))
    defines = {define.name for define in CORE.defines}
    # A define without the flag leaves the guard uninstalled; a flag without the define fails to link
    assert ("USE_I2C_ISR_GUARD" in defines) is expected
    assert (_WRAP_FLAG in CORE.build_flags) is expected
