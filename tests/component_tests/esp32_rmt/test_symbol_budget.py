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
    valid, messages = validate("s2_two_default_strips.yaml")
    assert valid
    assert len(messages) == 1
    assert "'Status LEDs' (192), 'NeoPixel Port' (192)" in messages[0]
    assert "Set 'rmt_symbols: 128'" in messages[0]


def test_fitting_strips_and_receiver_on_c6_pass(
    validate: Callable[[str], tuple[bool, list[str]]],
) -> None:
    """Two strips at 48 symbols fill the two C6 TX blocks; the receiver uses RX blocks."""
    assert validate("c6_two_strips_and_receiver_fit.yaml") == (True, [])


@pytest.mark.parametrize(
    ("file_name", "domain", "direction"),
    [
        ("c3_three_transmitters.yaml", "remote_transmitter", "TX"),
        ("c3_three_receivers.yaml", "remote_receiver", "RX"),
    ],
)
def test_more_components_than_channels_fail(
    validate: Callable[[str], tuple[bool, list[str]]],
    file_name: str,
    domain: str,
    direction: str,
) -> None:
    """Three remote_transmitter or remote_receiver need more than the two C3 channels."""
    valid, messages = validate(file_name)
    assert not valid
    assert any(f"has 2 RMT channels for {direction}" in line for line in messages)
    # Components without a name or id are named by their position in the config
    assert any(f"'{domain}[2]'" in line for line in messages)


def test_strip_below_one_block_fails(
    component_fixture_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    """The error is shown on rmt_symbols, also for a single component."""
    CORE.config_path = component_fixture_path("esp32_strip_below_one_block.yaml")
    assert read_config({}) is None
    lines = [line.strip() for line in capsys.readouterr().out.splitlines()]
    index = next(
        i for i, line in enumerate(lines) if "even number of at least 64" in line
    )
    # The failed config prints the error right above the key it belongs to
    assert lines[index + 1] == "rmt_symbols: 48"


def test_new_variant_gets_its_own_default(
    component_fixture_path: Callable[[str], Path],
) -> None:
    """The S31 uses the S3 defaults, not the ones of the original ESP32."""
    CORE.config_path = component_fixture_path("s31_default_strip.yaml")
    config = read_config({})
    assert config is not None
    assert config["light"][0]["rmt_symbols"] == 192


def test_strip_is_set_up_before_transmitter(
    validate: Callable[[str], tuple[bool, list[str]]],
) -> None:
    """The order comes from the setup priorities, not from the order in the config."""
    valid, messages = validate("s3_transmitter_before_strip.yaml")
    assert valid
    assert len(messages) == 1
    assert "'ir' will not fit: 'strip' (192), 'ir' (48)" in messages[0]


def test_large_strip_after_small_one_fits(
    validate: Callable[[str], tuple[bool, list[str]]],
) -> None:
    """Without receivers the second strip can use the RX blocks."""
    assert validate("s3_small_strip_first.yaml") == (True, [])
