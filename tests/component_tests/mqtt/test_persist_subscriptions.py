"""Tests for the mqtt persist_subscriptions option."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.esp8266.const import KEY_ESP8266, KEY_PREFERENCES
from esphome.components.mqtt import CONF_PERSIST_SUBSCRIPTIONS, validate_config
from esphome.const import CONF_CLEAN_SESSION, CONF_TOPIC_PREFIX
from esphome.core import CORE


def _defines() -> set[str]:
    return {define.name for define in CORE.defines}


def test_rejected_with_clean_session() -> None:
    with pytest.raises(cv.Invalid, match="requires 'clean_session: false'"):
        validate_config(
            {
                CONF_TOPIC_PREFIX: "node",
                CONF_CLEAN_SESSION: True,
                CONF_PERSIST_SUBSCRIPTIONS: True,
            }
        )


@pytest.mark.parametrize(("clean_session", "persist"), [(False, True), (True, False)])
def test_accepted(clean_session: bool, persist: bool) -> None:
    out = validate_config(
        {
            CONF_TOPIC_PREFIX: "node",
            CONF_CLEAN_SESSION: clean_session,
            CONF_PERSIST_SUBSCRIPTIONS: persist,
        }
    )
    assert out[CONF_PERSIST_SUBSCRIPTIONS] is persist


@pytest.mark.parametrize(
    ("config_file", "enabled"),
    [("persist_subscriptions.yaml", True), ("no_mac_suffix.yaml", False)],
)
def test_define(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    enabled: bool,
) -> None:
    main_cpp = generate_main(component_config_path(config_file))
    assert ("USE_MQTT_SESSION_PERSISTENCE" in _defines()) is enabled
    assert "set_clean_session(false)" in main_cpp


@pytest.mark.parametrize(
    ("config_file", "reserved"),
    [("persist_subscriptions.yaml", 1), ("no_mac_suffix.yaml", 0)],
)
def test_session_fingerprint_reserved_in_esp8266_rtc_budget(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    reserved: int,
) -> None:
    generate_main(component_config_path(config_file))
    reservations = CORE.data[KEY_ESP8266].get(KEY_PREFERENCES, [])
    # One uint32_t in RTC memory, a data word plus the checksum word, and nothing in flash
    assert reservations.count((2, False)) == reserved
    assert not any(in_flash for _, in_flash in reservations)
