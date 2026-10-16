"""Tests for at581x power_consumption validation."""

from decimal import Decimal

import pytest

from esphome import config_validation as cv
from esphome.components.at581x import CONF_POWER_CONSUMPTION, RADAR_SETTINGS_SCHEMA
from esphome.const import CONF_FREQUENCY, CONF_ID
from esphome.core import Lambda
from esphome.types import ConfigType

ALLOWED_UA = [48, 56, 63, 70, 77, 91, 105, 115, 40, 44, 47, 51, 54, 61, 68, 78]


def _settings(**extra: object) -> ConfigType:
    return RADAR_SETTINGS_SCHEMA(
        {CONF_ID: "waveradar", CONF_FREQUENCY: "5800MHz", **extra}
    )


def _power(value: object) -> object:
    return _settings(**{CONF_POWER_CONSUMPTION: value})[CONF_POWER_CONSUMPTION]


UNIT_SPELLINGS = [
    lambda ua: f"{ua}uA",
    lambda ua: f"{ua}µA",
    lambda ua: f"{ua} uA",
    lambda ua: f"{Decimal(ua) / 1000}mA",
    lambda ua: f"{Decimal(ua) / 1000000}A",
    lambda ua: f"{ua * 1000}nA",
]


@pytest.mark.parametrize("spell", UNIT_SPELLINGS)
@pytest.mark.parametrize("microamps", ALLOWED_UA)
def test_every_listed_value_in_every_unit(spell, microamps: int) -> None:
    result = _power(spell(microamps))
    assert result == microamps
    assert type(result) is int


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("91uA", 91),
        ("105uA", 105),
        ("115uA", 115),
        ("40uA", 40),
        ("0.07mA", 70),
        ("0.091mA", 91),
        ("0.000091A", 91),
        ("0.000000091kA", 91),
        ("+91uA", 91),
    ],
)
def test_previously_broken_and_equivalent_spellings(value: str, expected: int) -> None:
    assert _power(value) == expected


def test_default_is_70() -> None:
    assert _settings()[CONF_POWER_CONSUMPTION] == 70


def test_lambda_is_passed_through() -> None:
    assert isinstance(_power(Lambda("return 70;")), Lambda)


@pytest.mark.parametrize(
    "value",
    [
        # Accepted by the rel_tol=1e-12 proposal, rejected by the original.
        "91.00000000001uA",
        "90.99999999999uA",
        "91.00000000009uA",
        "91.0000000001uA",
        "90.9999999999uA",
        "91.000000001uA",
        "91.0001uA",
        "91.4uA",
        "0.09100000000001mA",
    ],
)
def test_fractional_microamps_are_rejected(value: str) -> None:
    with pytest.raises(cv.Invalid):
        _power(value)


def test_sub_ulp_digits_are_indistinguishable_parse_noise() -> None:
    # 1e-15 is below half an ulp of 91.0, so the mantissa parses to exactly 91.0.
    assert float("91.000000000000001") == 91.0
    assert _power("91.000000000000001uA") == 91


@pytest.mark.parametrize(
    "value",
    ["92uA", "0uA", "-91uA", "-0.091mA", "9" * 400 + "uA", "infuA", "nanuA", "nan"],
)
def test_unsupported_and_nonfinite_are_rejected(value: str) -> None:
    with pytest.raises(cv.Invalid):
        _power(value)
