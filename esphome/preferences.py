"""Helpers for letting a component choose where a preference is persisted.

Preferences can be stored either in flash (durable across power loss) or in RTC
memory (fast, survives deep sleep and soft resets but not power loss). The
flash-vs-RTC choice is only meaningful on platforms whose preferences backend
honors the ``in_flash`` flag — currently ESP32, ESP8266 and the Beken BK7231T,
BK7231N and BK7238. LibreTiny chips have no RTC memory: there ``rtc`` keeps the
preference in flash and discards it after a reset that RTC memory would not
survive. On other platforms the value is accepted only as ``flash`` (the sole
supported backend).

Components include :func:`storage_schema` in their config and convert the chosen
value with :func:`is_in_flash` when calling ``make_preference``.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_STORAGE
from esphome.core import CORE

STORAGE_FLASH = "flash"
STORAGE_RTC = "rtc"


def rtc_supported() -> bool:
    """Whether the target has RTC-backed preferences (ESP8266, ESP32 except C2/C61, some Beken chips); call at validation time."""
    if CORE.is_esp8266:
        return True
    if CORE.is_esp32:
        from esphome.components.esp32 import get_esp32_variant
        from esphome.components.esp32.const import VARIANT_ESP32C2, VARIANT_ESP32C61

        # Mirrors the C++ SOC_RTC_MEM_SUPPORTED guard in the ESP32 backend
        return get_esp32_variant() not in (VARIANT_ESP32C2, VARIANT_ESP32C61)
    return _libretiny_rtc_supported()


def _libretiny_rtc_supported() -> bool:
    """Whether this LibreTiny family accepts ``rtc``, emulated with flash plus the reset reason.

    The emulation is only as good as the family's ``lt_get_reboot_reason()``, so families are
    listed once that reporting has been checked on hardware: a chip-enable reset and a power cut
    both report a power-on, a software, watchdog or crash reset does not.
    """
    if not CORE.is_libretiny:
        return False
    from esphome.components.libretiny.const import (
        FAMILY_BK7231N,
        FAMILY_BK7231T,
        FAMILY_BK7238,
        KEY_FAMILY,
        KEY_LIBRETINY,
    )

    return CORE.data.get(KEY_LIBRETINY, {}).get(KEY_FAMILY) in (
        FAMILY_BK7231T,
        FAMILY_BK7231N,
        FAMILY_BK7238,
    )


def _default_storage() -> str:
    """Default that preserves each platform's historic behavior.

    ESP8266 has always stored these preferences in RTC memory; every other
    platform effectively used flash. Evaluated at validation time.
    """
    return STORAGE_RTC if CORE.is_esp8266 else STORAGE_FLASH


def validate_storage(value: object) -> str:
    """Validate a ``storage:`` value, ``flash`` or ``rtc`` where supported."""
    value = cv.one_of(STORAGE_FLASH, STORAGE_RTC, lower=True)(value)
    if value == STORAGE_RTC and not rtc_supported():
        raise cv.Invalid(
            f"'{STORAGE_RTC}' storage is not supported on this platform; only "
            f"'{STORAGE_FLASH}' is available"
        )
    return value


def storage_schema():
    """Return an Optional(CONF_STORAGE) entry for merging into a component schema."""
    return {cv.Optional(CONF_STORAGE, default=_default_storage): validate_storage}


def request_rtc_storage() -> None:
    """Compile the RTC-backed storage into the ESP32 or LibreTiny preferences backend.

    It is left out of ESP32 and LibreTiny builds unless something asks for it, so
    unused builds don't reserve RTC memory or check the reset reason. Call this
    from ``to_code`` when a config option selects RTC storage. No-op on other
    platforms (ESP8266 always has its RTC backend).
    """
    if CORE.is_esp32:
        cg.add_define("USE_ESP32_RTC_PREFERENCES")
    elif _libretiny_rtc_supported():
        cg.add_define("USE_LIBRETINY_RTC_PREFERENCES")


def validate_rtc_storage(value):
    """Validate a boolean option that requests RTC-backed preference storage.

    ``false`` means "no request", not "disable": it never turns RTC storage off
    (another option selecting ``storage: rtc`` still compiles it in). On ESP8266
    the backend is integral and always enabled, so ``false`` is rejected rather
    than silently ignored; ``true`` is a tolerated no-op there so shared config
    packages work across mixed fleets.
    """
    value = cv.boolean(value)
    if not value:
        if CORE.is_esp8266:
            raise cv.Invalid(
                "RTC preference storage is always enabled on ESP8266 and cannot "
                "be disabled"
            )
        return value
    if not rtc_supported():
        raise cv.Invalid("RTC preference storage is not supported on this platform")
    return value


def is_in_flash(value: str) -> bool:
    """Map a CONF_STORAGE value to the ``in_flash`` argument of make_preference.

    Call this from ``to_code``: when RTC storage is selected on ESP32 it also emits
    the define that compiles the RTC storage buffer into the ESP32 backend (see
    :func:`request_rtc_storage`).
    """
    in_flash = value == STORAGE_FLASH
    if not in_flash:
        request_rtc_storage()
    return in_flash


def reserve_preferences(
    count: int, value_bytes: int, in_flash: bool | None = None
) -> None:
    """Reserve saved values from to_code so a platform with a small store can check they all fit.

    in_flash is the value's own storage (is_in_flash(config[CONF_STORAGE])); leave it None for a
    component that calls the two-argument make_preference() and takes the platform default.
    """
    if CORE.is_esp8266:
        from esphome.components.esp8266.const import reserve_preferences as reserve

        reserve(count, value_bytes, in_flash)
