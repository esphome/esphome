"""Tests for the mqtt persist_subscriptions option."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.esp8266.const import KEY_ESP8266, KEY_PREFERENCES
from esphome.components.mqtt import CONF_PERSIST_SUBSCRIPTIONS, validate_config
from esphome.const import CONF_CLEAN_SESSION, CONF_TOPIC_PREFIX
from esphome.core import CORE


def test_rejected_with_clean_session() -> None:
    with pytest.raises(cv.Invalid, match="requires 'clean_session: false'"):
        validate_config(
            {
                CONF_TOPIC_PREFIX: "node",
                CONF_CLEAN_SESSION: True,
                CONF_PERSIST_SUBSCRIPTIONS: True,
            }
        )


def test_accepted_without_clean_session() -> None:
    out = validate_config(
        {
            CONF_TOPIC_PREFIX: "node",
            CONF_CLEAN_SESSION: False,
            CONF_PERSIST_SUBSCRIPTIONS: True,
        }
    )
    assert out[CONF_PERSIST_SUBSCRIPTIONS] is True


@pytest.mark.parametrize(
    ("config_file", "enabled"),
    [("persist_subscriptions.yaml", True), ("no_mac_suffix.yaml", False)],
)
def test_define_and_rtc_reservation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    enabled: bool,
) -> None:
    generate_main(component_config_path(config_file))
    assert ("USE_MQTT_SESSION_PERSISTENCE" in {d.name for d in CORE.defines}) is enabled
    # One uint32_t in ESP8266 RTC memory: a data word plus the checksum word, not in flash
    reservations = CORE.data[KEY_ESP8266].get(KEY_PREFERENCES, [])
    assert reservations.count((2, False)) == int(enabled)
