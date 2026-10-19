"""The config-time fit check must agree with make_preference() in preferences.cpp."""

import asyncio

import pytest

from esphome import preferences
from esphome.components.esp8266.const import (
    KEY_ESP8266,
    KEY_PREFERENCES,
    check_preferences_fit,
    preference_words,
    reserve_preferences,
)
from esphome.const import KEY_CORE, KEY_TARGET_PLATFORM
from esphome.core import CORE, EsphomeError


@pytest.fixture(autouse=True)
def esp8266_core() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp8266"}
    CORE.data[KEY_ESP8266] = {}
    CORE.config = {KEY_ESP8266: {"restore_from_flash": False}}


def _check() -> None:
    asyncio.run(check_preferences_fit())


@pytest.mark.parametrize(
    ("value_bytes", "words"), [(0, 1), (1, 2), (4, 2), (5, 3), (94, 25)]
)
def test_preference_words_rounds_up_and_adds_the_checksum(
    value_bytes: int, words: int
) -> None:
    assert preference_words(value_bytes) == words


@pytest.mark.parametrize(
    ("restore_from_flash", "in_flash", "fitting", "match"),
    [
        # 25 words each: 3 in the 78-word region and 1 in the 32-word one
        (False, False, 4, "RTC memory has 110"),
        (False, None, 4, "RTC memory has 110"),
        # an explicit RTC value stays in RTC whatever restore_from_flash says
        (True, False, 4, "RTC memory has 110"),
        # 125 of 128 flash words, by the two-argument default or explicitly
        (True, None, 5, "flash store has 128"),
        (True, True, 5, "flash store has 128"),
        # explicit flash values without restore_from_flash use the 64-word store
        (False, True, 2, "flash store has 64"),
    ],
)
def test_reservations_add_up_against_the_store_they_land_in(
    restore_from_flash: bool, in_flash: bool | None, fitting: int, match: str
) -> None:
    CORE.config[KEY_ESP8266]["restore_from_flash"] = restore_from_flash
    reserve_preferences(fitting - 1, 94, in_flash)  # one component
    reserve_preferences(1, 94, in_flash)  # another component
    _check()
    reserve_preferences(1, 94, in_flash)
    with pytest.raises(EsphomeError, match=match):
        _check()


def test_values_that_cannot_split_between_the_regions_fail() -> None:
    # 5 x 20 words is 100 of 110, but the normal region takes 3 and the eboot region 1
    reserve_preferences(5, 76)
    with pytest.raises(EsphomeError, match="cannot span"):
        _check()


def test_a_value_larger_than_either_region_fails() -> None:
    reserve_preferences(1, 400)
    with pytest.raises(EsphomeError, match="cannot span"):
        _check()


def test_a_split_that_works_in_some_order_passes_with_a_warning(
    caplog: pytest.LogCaptureFixture,
) -> None:
    # 78 + 32: one value fills each region exactly
    reserve_preferences(1, 308)
    reserve_preferences(1, 124)
    _check()
    assert "depends on setup order" in caplog.text


def test_nothing_reserved_passes_quietly(caplog: pytest.LogCaptureFixture) -> None:
    _check()
    assert caplog.text == ""


def test_the_check_runs_as_a_scheduled_job() -> None:
    reserve_preferences(3, 400)
    CORE.add_job(check_preferences_fit)
    with pytest.raises(EsphomeError, match="cannot span"):
        CORE.flush_tasks()


def test_components_reserve_through_esphome_preferences() -> None:
    preferences.reserve_preferences(2, 94, in_flash=True)
    assert CORE.data[KEY_ESP8266][KEY_PREFERENCES] == [(25, True), (25, True)]
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    CORE.data[KEY_ESP8266] = {}
    preferences.reserve_preferences(2, 94)
    assert KEY_PREFERENCES not in CORE.data[KEY_ESP8266]
