"""Helpers for letting a component choose where a preference is persisted.

Preferences can be stored either in flash (durable across power loss) or in RTC
memory (fast, survives deep sleep and soft resets but not power loss). The
flash-vs-RTC choice is only meaningful on platforms whose preferences backend
honors the ``in_flash`` flag — currently ESP32 and ESP8266. On other platforms
the value is accepted only as ``flash`` (the sole supported backend).

Components include :func:`storage_schema` in their config and convert the chosen
value with :func:`is_in_flash` when calling ``make_preference``.
"""

import esphome.config_validation as cv
from esphome.const import CONF_STORAGE
from esphome.core import CORE

STORAGE_FLASH = "flash"
STORAGE_RTC = "rtc"


def _rtc_supported() -> bool:
    """Whether the active platform has an RTC-backed preferences backend."""
    return CORE.is_esp32 or CORE.is_esp8266


def _default_storage() -> str:
    """Default that preserves each platform's historic behavior.

    ESP8266 has always stored these preferences in RTC memory; every other
    platform effectively used flash. Evaluated at validation time.
    """
    return STORAGE_RTC if CORE.is_esp8266 else STORAGE_FLASH


def _validate_storage(value):
    value = cv.one_of(STORAGE_FLASH, STORAGE_RTC, lower=True)(value)
    if value == STORAGE_RTC and not _rtc_supported():
        raise cv.Invalid(
            f"'{STORAGE_RTC}' storage is not supported on this platform; only "
            f"'{STORAGE_FLASH}' is available"
        )
    return value


def storage_schema():
    """Return an Optional(CONF_STORAGE) entry for merging into a component schema."""
    return {cv.Optional(CONF_STORAGE, default=_default_storage): _validate_storage}


def is_in_flash(value: str) -> bool:
    """Map a CONF_STORAGE value to the ``in_flash`` argument of make_preference."""
    return value == STORAGE_FLASH
