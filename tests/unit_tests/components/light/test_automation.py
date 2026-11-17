"""Tests for validate_light_state -- a light on/off value that prioritizes ON/OFF
string forms over generic boolean forms, while still accepting the latter."""

import pytest
import yaml

from esphome.components.light.automation import validate_light_state
import esphome.config_validation as cv
from esphome.schema_extractors import SCHEMA_EXTRACT


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("ON", True),
        ("on", True),
        ("On", True),
        ("OFF", False),
        ("off", False),
        ("Off", False),
        (True, True),
        (False, False),
        ("true", True),
        ("false", False),
        ("yes", True),
        ("no", False),
    ],
)
def test_validate_light_state_accepts_on_off_and_booleans(
    value: str | bool, expected: bool
) -> None:
    assert validate_light_state(value) is expected


def test_validate_light_state_rejects_invalid_string() -> None:
    # The error must mention both accepted forms (ON/OFF and boolean), not just
    # whichever validator happened to run last.
    with pytest.raises(cv.Invalid, match="ON.*OFF.*boolean"):
        validate_light_state("maybe")


def test_validate_light_state_schema_extractor_reports_on_off() -> None:
    assert validate_light_state(SCHEMA_EXTRACT) == ("ON", "OFF")


def test_validate_light_state_quoted_on_off_survive_yaml_parsing() -> None:
    """The default PyYAML resolver treats bareword on/off/yes/no as booleans, so a real
    YAML config must quote 'ON'/'OFF' for validate_light_state's string-matching branch
    to ever see a string at all -- an unquoted `state: on` already arrives as a native
    bool. Both forms must still validate to the same result.
    """
    quoted = yaml.safe_load('state: "ON"')["state"]
    assert quoted == "ON"
    assert validate_light_state(quoted) is True

    unquoted = yaml.safe_load("state: on")["state"]
    assert unquoted is True  # PyYAML already converted it before validation runs
    assert validate_light_state(unquoted) is True
