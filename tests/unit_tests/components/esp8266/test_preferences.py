"""The config-time fit check must agree with make_preference() in preferences.cpp."""

import asyncio

import pytest

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


def test_reservations_from_several_components_add_up() -> None:
    reserve_preferences(2, 94)  # 50 words
    reserve_preferences(2, 94)  # another component, 100 in total
    _check()
    reserve_preferences(1, 94)  # 125 > 110
    with pytest.raises(EsphomeError, match="RTC memory has 110"):
        _check()


def test_a_value_cannot_exceed_the_larger_rtc_region() -> None:
    reserve_preferences(1, 400)
    with pytest.raises(EsphomeError, match="RTC region"):
        _check()


def test_flash_store_follows_restore_from_flash() -> None:
    CORE.config[KEY_ESP8266]["restore_from_flash"] = True
    reserve_preferences(5, 94)  # 125 of 128 words
    _check()
    reserve_preferences(1, 94)
    with pytest.raises(EsphomeError, match="flash store has 128"):
        _check()


def test_explicit_flash_values_use_the_small_store_without_restore() -> None:
    reserve_preferences(2, 94, in_flash=True)  # 50 of 64 words
    _check()
    reserve_preferences(1, 94, in_flash=True)
    with pytest.raises(EsphomeError, match="flash store has 64"):
        _check()


def test_values_that_cannot_split_between_the_regions_fail() -> None:
    # 5 x 20 words is 100 of 110, but the normal region takes 3 and the eboot region 1
    reserve_preferences(5, 76)
    with pytest.raises(EsphomeError, match="cannot span"):
        _check()


def test_a_split_that_works_in_some_order_passes_with_a_warning(
    caplog: pytest.LogCaptureFixture,
) -> None:
    # 78 + 32: one value fills each region exactly
    reserve_preferences(1, 308)
    reserve_preferences(1, 124)
    _check()
    assert "may not all fit" in caplog.text


def test_the_check_runs_from_the_scheduled_job() -> None:
    reserve_preferences(3, 400)
    with pytest.raises(EsphomeError, match="RTC region"):
        CORE.flush_tasks()


def test_other_platforms_reserve_nothing() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    reserve_preferences(100, 400)
    assert KEY_PREFERENCES not in CORE.data[KEY_ESP8266]
