"""Optional MCP251XFD interrupt configuration."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.mcp251xfd import canbus as mcp251xfd
import esphome.config_validation as cv
from esphome.const import PlatformFramework

from ..types import SetCoreConfigCallable


@pytest.mark.parametrize("pin", [4, {"number": 4, "mode": "INPUT_PULLUP"}])
def test_interrupt_pin(
    pin: int | dict[str, object], set_core_config: SetCoreConfigCallable
) -> None:
    """Accept an optional internal input pin."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={"variant": "ESP32", "board": "esp32dev"},
    )
    config = mcp251xfd.CONFIG_SCHEMA({"can_id": 1, "cs_pin": 21, "interrupt_pin": pin})
    assert config["interrupt_pin"]["number"] == 4
    assert config["interrupt_pin"]["mode"]["input"]
    assert "interrupt_pin" not in mcp251xfd.CONFIG_SCHEMA({"can_id": 1, "cs_pin": 21})


@pytest.mark.parametrize("options", [{"inverted": True}, {"allow_other_uses": True}])
def test_invalid_interrupt_pin(
    options: dict[str, object], set_core_config: SetCoreConfigCallable
) -> None:
    """The active-low interrupt disallows inversion and pin sharing."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={"variant": "ESP32", "board": "esp32dev"},
    )
    with pytest.raises(cv.Invalid):
        mcp251xfd.CONFIG_SCHEMA(
            {"can_id": 1, "cs_pin": 21, "interrupt_pin": {"number": 4, **options}}
        )


def test_interrupt_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The configured pin reaches the driver."""
    main = generate_main(component_config_path("../../canbus/config/send_fd.yaml"))
    assert "fd_bus->set_interrupt_pin(" in main


def test_polling_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Polling remains the default when no interrupt pin is configured."""
    main = generate_main(component_config_path("mcp251xfd_timing.yaml"))
    assert "set_interrupt_pin(" not in main
