"""RMT budget check on full configs, with schema defaults and every component's FINAL_VALIDATE_SCHEMA."""

from collections.abc import Callable
import logging
from pathlib import Path

import pytest

from esphome.config import read_config
from esphome.core import CORE


@pytest.fixture
def validate(
    component_fixture_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
    capsys: pytest.CaptureFixture[str],
) -> Callable[[str], tuple[bool, list[str]]]:
    """Validate a fixture, return whether it is valid and the RMT warnings and errors.

    Warnings are logged, validation errors are printed with the failed config.
    """

    def run(file_name: str) -> tuple[bool, list[str]]:
        CORE.config_path = component_fixture_path(file_name)
        with caplog.at_level(logging.INFO):
            valid = read_config({}) is not None
        output = caplog.messages + capsys.readouterr().out.splitlines()
        return valid, [line.strip() for line in output if "RMT" in line]

    return run


def test_default_strips_on_s2_warn(
    validate: Callable[[str], tuple[bool, list[str]]],
) -> None:
    """Two strips at the default 192 symbols need 6 of the 4 shared S2 blocks."""
    valid, messages = validate("fan_controller_s2_defaults.yaml")
    assert valid
    assert len(messages) == 1
    assert "'Status LEDs' (192), 'NeoPixel Port' (192)" in messages[0]
    assert "Set 'rmt_symbols: 128'" in messages[0]


def test_fitting_strips_on_c6_pass(
    validate: Callable[[str], tuple[bool, list[str]]],
) -> None:
    """Two strips at 48 symbols fill the two C6 TX blocks exactly."""
    assert validate("fan_controller_c6_fixed.yaml") == (True, [])


@pytest.mark.parametrize(
    ("file_name", "direction"),
    [("c3_three_transmitters.yaml", "TX"), ("c3_three_receivers.yaml", "RX")],
)
def test_more_components_than_channels_fail(
    validate: Callable[[str], tuple[bool, list[str]]], file_name: str, direction: str
) -> None:
    """Three remote_transmitter or remote_receiver need more than the two C3 channels."""
    valid, messages = validate(file_name)
    assert not valid
    assert any(f"has 2 RMT {direction} channels" in message for message in messages)
    # Components without a name or id are named by their position in the config
    domain = file_name.removeprefix("c3_three_").removesuffix("s.yaml")
    assert any(f"'remote_{domain}[2]'" in message for message in messages)


def test_strip_below_one_block_fails(
    component_fixture_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    """The error is shown on rmt_symbols, also for a single component."""
    CORE.config_path = component_fixture_path("esp32_strip_below_one_block.yaml")
    assert read_config({}) is None
    output = capsys.readouterr().out
    assert "Must be an even number and at least 64 on ESP32" in output
