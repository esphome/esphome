"""Tests for the zephyr_ble_server component."""

import logging

import pytest

from esphome.components.zephyr_ble_server import _warn_without_antenna
import esphome.final_validate as fv

ANTENNA_WARNING = "has no antenna of its own"


@pytest.mark.parametrize(
    ("board", "warns"),
    [("xiao_nrf54lm20a", True), ("nrf54lm20dk", False)],
)
def test_warns_about_a_board_without_antenna(
    caplog: pytest.LogCaptureFixture, board: str, warns: bool
) -> None:
    """A board with only an antenna connector needs an external antenna for Bluetooth."""
    token = fv.full_config.set({"nrf52": {"board": board}})
    try:
        with caplog.at_level(logging.WARNING):
            _warn_without_antenna({})
    finally:
        fv.full_config.reset(token)
    assert (ANTENNA_WARNING in caplog.text) is warns
