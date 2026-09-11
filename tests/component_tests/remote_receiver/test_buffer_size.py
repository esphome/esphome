"""buffer_size is bytes on the pulse ring targets, with a floor and a 1000 pulse default."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components import remote_receiver
from esphome.components.esp8266 import gpio as esp8266_gpio  # noqa: F401  registers the pin schema
from esphome.config_validation import Invalid
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.parametrize(
    "target", ["esp8266", "rp2", "bk72xx", "rtl87xx", "ln882x", "esp32_c2", "esp32_c61"]
)
def test_pulse_ring_default_holds_1000_pulses(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    target: str,
) -> None:
    main_cpp = generate_main(component_config_path(f"receiver_{target}.yaml"))
    assert "rcvr->set_buffer_size(4000);" in main_cpp


@pytest.mark.parametrize(
    ("value", "accepted"),
    [("32b", False), ("64b", True), ("65b", True), ("1mb", False)],
)
def test_buffer_size_range(
    set_core_config: SetCoreConfigCallable, value: str, accepted: bool
) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    config = {"pin": "GPIO4", "buffer_size": value}
    if accepted:
        assert remote_receiver.CONFIG_SCHEMA(config)["buffer_size"] >= 64
    else:
        with pytest.raises(Invalid):
            remote_receiver.CONFIG_SCHEMA(config)
