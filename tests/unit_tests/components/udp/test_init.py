"""Tests for the udp component configuration schema."""

from __future__ import annotations

import pytest

from esphome.components import udp
from esphome.components.packet_transport import (
    CONF_BINARY_SENSORS,
    CONF_ENCRYPTION,
    CONF_PING_PONG_ENABLE,
    CONF_PROVIDERS,
    CONF_ROLLING_CODE_ENABLE,
    CONF_SENSORS,
)
import esphome.config_validation as cv


@pytest.mark.parametrize(
    "option",
    [
        CONF_PROVIDERS,
        CONF_ENCRYPTION,
        CONF_PING_PONG_ENABLE,
        CONF_ROLLING_CODE_ENABLE,
        CONF_SENSORS,
        CONF_BINARY_SENSORS,
    ],
)
def test_relocated_option_rejected(option: str) -> None:
    """Options that moved to packet_transport raise a pointing error."""
    with pytest.raises(cv.Invalid) as exc_info:
        udp.CONFIG_SCHEMA({option: True})
    assert (
        f"The '{option}' option should now be configured in the 'packet_transport' component"
        in str(exc_info.value)
    )
