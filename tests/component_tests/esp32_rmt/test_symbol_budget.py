"""RMT budget check on full configs.

Channel errors come from every component's FINAL_VALIDATE_SCHEMA, the memory warning
from code generation, which knows the order the components are set up in.
"""

from collections.abc import Callable
import logging
from pathlib import Path

import pytest

from esphome.config import read_config
from esphome.core import CORE

Validate = Callable[[str], tuple[bool, list[str]]]
Generate = Callable[[str], list[str]]


def _rmt_lines(lines: list[str]) -> list[str]:
    return [line.strip() for line in lines if "RMT" in line]


@pytest.fixture
def validate(
    component_fixture_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
    capsys: pytest.CaptureFixture[str],
) -> Validate:
    """Validate a fixture, return whether it is valid and the RMT warnings and errors.

    Warnings are logged, validation errors are printed with the failed config.
    """

    def run(file_name: str) -> tuple[bool, list[str]]:
        CORE.config_path = component_fixture_path(file_name)
        with caplog.at_level(logging.INFO):
            valid = read_config({}) is not None
        return valid, _rmt_lines(caplog.messages + capsys.readouterr().out.splitlines())

    return run


@pytest.fixture
def generate(
    component_fixture_path: Callable[[str], Path],
    generate_main: Callable[[str | Path], str],
    caplog: pytest.LogCaptureFixture,
) -> Generate:
    """Validate a fixture and generate its code, return the RMT warnings of each step."""

    def run(file_name: str) -> list[str]:
        with caplog.at_level(logging.INFO):
            generate_main(component_fixture_path(file_name))
        return _rmt_lines(caplog.messages)

    return run


def test_validation_does_not_warn(validate: Validate) -> None:
    """The warning depends on the setup order, so validation alone does not show it."""
    assert validate("s2_two_default_strips.yaml") == (True, [])


def test_default_strips_on_s2_warn(generate: Generate) -> None:
    """Two strips at the default 192 symbols need 6 of the 4 shared S2 blocks."""
    messages = generate("s2_two_default_strips.yaml")
    assert len(messages) == 1
    assert "'Status LEDs' (192), 'NeoPixel Port' (192)" in messages[0]
    assert "Set 'rmt_symbols: 128'" in messages[0]


def test_fitting_strips_and_receiver_on_c6_pass(generate: Generate) -> None:
    """Two strips at 48 symbols fill the two C6 TX blocks; the receiver uses RX blocks."""
    assert generate("c6_two_strips_and_receiver_fit.yaml") == []


def test_large_strip_first_on_c6_warns(generate: Generate) -> None:
    """96 then 48 symbols on C6 fails on hardware: the second strip gets no channel."""
    messages = generate("c6_large_strip_first.yaml")
    assert len(messages) == 1
    assert "'Strip 1' will not fit: 'Strip 0' (96), 'Strip 1' (48)" in messages[0]
    assert (
        "or set 'setup_priority' so the smaller ones are set up first" in (messages[0])
    )


@pytest.mark.parametrize(
    ("file_name", "domain", "direction"),
    [
        ("c3_three_transmitters.yaml", "remote_transmitter", "TX"),
        ("c3_three_receivers.yaml", "remote_receiver", "RX"),
    ],
)
def test_more_components_than_channels_fail(
    validate: Validate,
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


def test_strip_is_set_up_before_transmitter(generate: Generate) -> None:
    """The order comes from the setup priorities, not from the order in the config."""
    messages = generate("s3_transmitter_before_strip.yaml")
    assert len(messages) == 1
    assert "'ir' will not fit: 'strip' (192), 'ir' (48)" in messages[0]


def test_large_strip_after_small_one_fits(generate: Generate) -> None:
    """Without receivers the second strip can use the RX blocks."""
    assert generate("s3_small_strip_first.yaml") == []
