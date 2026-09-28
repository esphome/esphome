"""Config-validation tests for the aqi sensor component."""

import pytest
from voluptuous import Invalid

from esphome.components.aqi import CONF_CALCULATION_TYPE, CONF_EXTENDED_RANGE
from esphome.components.aqi.sensor import _validate_extended_range


def test_extended_range_rejected_with_caqi():
    """extended_range has no meaning for CAQI (no spec maximum) and must be rejected."""
    with pytest.raises(Invalid, match="CAQI"):
        _validate_extended_range(
            {CONF_CALCULATION_TYPE: "CAQI", CONF_EXTENDED_RANGE: True}
        )


def test_extended_range_rejected_with_caqi_even_when_false():
    """The option is not allowed at all with CAQI, regardless of its value."""
    with pytest.raises(Invalid, match="CAQI"):
        _validate_extended_range(
            {CONF_CALCULATION_TYPE: "CAQI", CONF_EXTENDED_RANGE: False}
        )


def test_extended_range_allowed_with_aqi():
    """extended_range is valid for the US AQI calculation."""
    config = {CONF_CALCULATION_TYPE: "AQI", CONF_EXTENDED_RANGE: True}
    assert _validate_extended_range(config) is config


def test_caqi_without_extended_range_ok():
    """CAQI is fine as long as extended_range is not set."""
    config = {CONF_CALCULATION_TYPE: "CAQI"}
    assert _validate_extended_range(config) is config
