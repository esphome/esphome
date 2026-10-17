"""Tests for the show_test_card and update_interval check in the display schema."""

import logging

import pytest

from esphome.components.display import FULL_DISPLAY_SCHEMA, _validate_test_card
from esphome.types import ConfigType

WARNING = "`show_test_card: True` is used with `update_interval: never`"


def test_test_card_with_update_interval_never_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The test card is only drawn when something else updates the display."""
    with caplog.at_level(logging.WARNING, logger="esphome.components.display"):
        FULL_DISPLAY_SCHEMA({"show_test_card": True, "update_interval": "never"})
    assert WARNING in caplog.text


@pytest.mark.parametrize(
    "config",
    [
        pytest.param(
            {"show_test_card": True, "update_interval": "1s"}, id="test_card_1s"
        ),
        pytest.param({"show_test_card": True}, id="test_card_default_interval"),
        pytest.param(
            {"show_test_card": False, "update_interval": "never"},
            id="no_test_card_never",
        ),
        pytest.param({"update_interval": "never"}, id="never_only"),
    ],
)
def test_other_combinations_do_not_warn(
    config: ConfigType, caplog: pytest.LogCaptureFixture
) -> None:
    """Only the test card together with `update_interval: never` warns."""
    with caplog.at_level(logging.WARNING, logger="esphome.components.display"):
        FULL_DISPLAY_SCHEMA(config)
    assert WARNING not in caplog.text


def test_test_card_without_update_interval_accepted() -> None:
    """Platforms such as epaper_spi may leave update_interval unset at this stage."""
    config = {"show_test_card": True}
    assert _validate_test_card(config) is config
