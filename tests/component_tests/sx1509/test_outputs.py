"""SX1509 LED outputs are Apply outputs the hub sets up with the duty turn_off() wrote."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.sx1509 import output as sx1509_output
from esphome.config import Config
from esphome.const import CONF_PIN, CONF_PLATFORM, PlatformFramework
from esphome.core import CORE, ID

from ..types import SetCoreConfigCallable


def test_led_outputs_are_apply_outputs(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("outputs.yaml"))

    assert "SX1509FloatOutputChannel" not in main_cpp
    assert "sx_a->write_led(index, state)" in main_cpp
    assert "sx_b->write_led(index, state)" in main_cpp
    # Every channel of a hub shares one apply function, with the pin as its index
    assert "new(plain) output::ApplyFloatOutput<esphome__plain__fn>(5);" in main_cpp
    assert "new(inverted) output::ApplyFloatOutput<esphome__plain__fn>(3);" in main_cpp
    assert "inverted->set_inverted(true);" in main_cpp
    for call in (
        "sx_a->register_led_channel(5, 0);",
        "sx_a->register_led_channel(3, 255);",
        "sx_a->register_led_channel(7, 26);",
        "sx_a->register_led_channel(8, 230);",
        "sx_a->register_led_channel(9, 255);",
        "sx_b->register_led_channel(5, 0);",
    ):
        assert call in main_cpp
    # The hub sets the channels up in config order, like the channel components did
    assert main_cpp.index("register_led_channel(5, 0)") < main_cpp.index(
        "register_led_channel(3, 255)"
    )
    defines = {d.name: d.value for d in CORE.defines}
    assert str(defines["SX1509_LED_CHANNEL_COUNT"]) == "5"


@pytest.mark.parametrize(
    ("config", "duty"),
    [
        ({}, 0),
        ({"inverted": True}, 255),
        ({"min_power": 0.1}, 26),
        ({"inverted": True, "min_power": 0.1}, 230),
        ({"min_power": 0.1, "zero_means_zero": True}, 0),
        ({"inverted": True, "min_power": 0.1, "zero_means_zero": True}, 255),
        # set_min_power() clamps to max_power
        ({"min_power": 0.5, "max_power": 0.2}, 51),
        ({"inverted": True, "min_power": 0.002}, 254),
    ],
)
def test_initial_duty(config: dict, duty: int) -> None:
    """The duty FloatOutput::turn_off() wrote through the old channel's setup()."""
    assert sx1509_output.initial_duty({"zero_means_zero": False, **config}) == duty


def test_pin_must_be_unique(set_core_config: SetCoreConfigCallable) -> None:
    entries = [
        {CONF_PLATFORM: "sx1509", "sx1509_id": ID(hub), CONF_PIN: pin}
        for hub, pin in (("a", 5), ("a", 6), ("b", 5), ("a", 5))
    ]
    full = Config()
    full["output"] = entries
    set_core_config(PlatformFramework.ESP8266_ARDUINO, full_config=full)
    for entry in entries[1:3]:
        sx1509_output.FINAL_VALIDATE_SCHEMA(entry)
    with pytest.raises(cv.Invalid, match="Channel '5' of 'a' is already used"):
        sx1509_output.FINAL_VALIDATE_SCHEMA(entries[3])
