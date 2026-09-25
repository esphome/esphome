"""Tests for time component – ha-timezone branch changes.

Covers:
- detect_tz() platform guard (returns None for unsupported platforms)
- detect_tz() result caching (avoids duplicate log messages)
- detect_tz() error paths (tzlocal None, tzdata missing)
- validate_tz() accepts/rejects POSIX timezone strings and IANA keys
- TIME_SCHEMA: timezone is now truly optional (was SplitDefault)
- homeassistant/time: USE_HOMEASSISTANT_TIMEZONE define emitted iff
  CONF_TIMEZONE is absent from the config
"""

from __future__ import annotations

from unittest import mock

import pytest

from esphome.components.time import DOMAIN, TIME_SCHEMA, detect_tz, validate_tz
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TIMEZONE,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    Platform,
    PlatformFramework,
)
from esphome.core import CORE, EsphomeError
from tests.component_tests.types import SetCoreConfigCallable

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

# A minimal TZif v2/v3 file that encodes "EST5EDT" as the footer line.
# The binary content is not validated at this level – what matters is that
# _extract_tz_string() picks up the last-but-one newline-terminated line.
_FAKE_TZFILE = b"\x00" * 44 + b"TZif2\x00" * 1 + b"\n" + b"EST5EDT,M3.2.0,M11.1.0\n"


def _set_platform(platform: Platform) -> None:
    """Set CORE.data so that CORE.target_platform returns *platform*."""
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: platform,
        KEY_TARGET_FRAMEWORK: "arduino",
    }


# ---------------------------------------------------------------------------
# detect_tz – platform guard
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    "platform_framework",
    [
        PlatformFramework.NRF52_ZEPHYR,
    ],
)
def test_detect_tz_returns_none_for_unsupported_platform(
    platform_framework: PlatformFramework,
    set_core_config: SetCoreConfigCallable,
) -> None:
    """detect_tz() must return None for platforms that do not support TZ auto-detection."""
    set_core_config(platform_framework)
    result = detect_tz()
    assert result is None


@pytest.mark.parametrize(
    "platform_framework",
    [
        PlatformFramework.ESP32_IDF,
        PlatformFramework.ESP32_ARDUINO,
        PlatformFramework.ESP8266_ARDUINO,
        PlatformFramework.RP2040_ARDUINO,
        PlatformFramework.BK72XX_ARDUINO,
        PlatformFramework.RTL87XX_ARDUINO,
        PlatformFramework.LN882X_ARDUINO,
        PlatformFramework.HOST_NATIVE,
    ],
)
def test_detect_tz_calls_tzlocal_for_supported_platform(
    platform_framework: PlatformFramework,
    set_core_config: SetCoreConfigCallable,
) -> None:
    """detect_tz() must call tzlocal for every supported platform."""
    set_core_config(platform_framework)
    with (
        mock.patch(
            "esphome.components.time.tzlocal.get_localzone_name",
            return_value="America/New_York",
        ),
        mock.patch(
            "esphome.components.time._load_tzdata",
            return_value=_FAKE_TZFILE,
        ),
    ):
        result = detect_tz()
    assert result is not None
    assert isinstance(result, str)
    assert len(result) > 0


# ---------------------------------------------------------------------------
# detect_tz – caching
# ---------------------------------------------------------------------------


def test_detect_tz_caches_result(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """detect_tz() must cache the TZ string after the first call so that
    subsequent invocations (e.g. when multiple time platforms are configured)
    skip tzlocal and avoid duplicate INFO messages."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with (
        mock.patch(
            "esphome.components.time.tzlocal.get_localzone_name",
            return_value="America/New_York",
        ) as mock_tz,
        mock.patch(
            "esphome.components.time._load_tzdata",
            return_value=_FAKE_TZFILE,
        ) as mock_load,
    ):
        first = detect_tz()
        second = detect_tz()

    assert first == second
    # tzlocal and _load_tzdata must be called exactly once despite two detect_tz() calls
    mock_tz.assert_called_once()
    mock_load.assert_called_once()


def test_detect_tz_cache_stored_in_core_data(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The cached TZ string should be stored under CORE.data[DOMAIN][CONF_TIMEZONE]."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with (
        mock.patch(
            "esphome.components.time.tzlocal.get_localzone_name",
            return_value="Europe/London",
        ),
        mock.patch(
            "esphome.components.time._load_tzdata",
            return_value=_FAKE_TZFILE,
        ),
    ):
        result = detect_tz()

    assert CORE.data.get(DOMAIN, {}).get(CONF_TIMEZONE) == result


def test_detect_tz_returns_pre_seeded_cache(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """If CORE.data already has a cached TZ string, detect_tz() must return it
    without calling tzlocal at all."""
    set_core_config(PlatformFramework.ESP32_IDF)
    CORE.data[DOMAIN] = {CONF_TIMEZONE: "CET-1CEST,M3.5.0,M10.5.0/3"}

    with mock.patch("esphome.components.time.tzlocal.get_localzone_name") as mock_tz:
        result = detect_tz()

    assert result == "CET-1CEST,M3.5.0,M10.5.0/3"
    mock_tz.assert_not_called()


# ---------------------------------------------------------------------------
# detect_tz – error paths
# ---------------------------------------------------------------------------


def test_detect_tz_raises_when_tzlocal_returns_none(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """detect_tz() must raise EsphomeError when the local timezone cannot be determined."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with (
        mock.patch(
            "esphome.components.time.tzlocal.get_localzone_name",
            return_value=None,
        ),
        pytest.raises(EsphomeError, match="Could not automatically determine timezone"),
    ):
        detect_tz()


def test_detect_tz_raises_when_tzdata_not_found(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """detect_tz() must raise EsphomeError when tzdata has no entry for the IANA key."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with (
        mock.patch(
            "esphome.components.time.tzlocal.get_localzone_name",
            return_value="Antarctica/Troll",
        ),
        mock.patch(
            "esphome.components.time._load_tzdata",
            return_value=None,
        ),
        pytest.raises(EsphomeError, match="Could not automatically determine timezone"),
    ):
        detect_tz()


# ---------------------------------------------------------------------------
# validate_tz
# ---------------------------------------------------------------------------


def test_validate_tz_accepts_valid_posix_string() -> None:
    """validate_tz() must accept a syntactically valid POSIX TZ string."""
    result = validate_tz("UTC0")
    assert result == "UTC0"


def test_validate_tz_accepts_posix_string_with_dst() -> None:
    """validate_tz() must accept a full POSIX TZ string with DST rules."""
    tz = "EST5EDT,M3.2.0,M11.1.0"
    result = validate_tz(tz)
    assert result == tz


def test_validate_tz_accepts_iana_key_and_converts() -> None:
    """validate_tz() must accept an IANA timezone key and return the POSIX string."""
    with mock.patch(
        "esphome.components.time._load_tzdata",
        return_value=_FAKE_TZFILE,
    ):
        result = validate_tz("America/New_York")

    # Should have been converted from IANA to POSIX via _extract_tz_string
    assert result == "EST5EDT,M3.2.0,M11.1.0"


def test_validate_tz_rejects_invalid_posix_string() -> None:
    """validate_tz() must raise cv.Invalid for a malformed POSIX TZ string."""
    with pytest.raises(cv.Invalid, match="Invalid POSIX timezone string"):
        validate_tz("NOTAVALIDTZ!!!")


def test_validate_tz_accepts_empty_string() -> None:
    """An empty string is accepted by validate_tz() and signals 'disable timezone'."""
    result = validate_tz("")
    assert result == ""


# ---------------------------------------------------------------------------
# TIME_SCHEMA – timezone is now cv.Optional (no SplitDefault)
# ---------------------------------------------------------------------------


def test_time_schema_timezone_is_optional(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """TIME_SCHEMA must accept a config with no timezone key on a supported platform."""
    set_core_config(PlatformFramework.ESP32_IDF)
    # Should not raise
    config = TIME_SCHEMA({})
    assert CONF_TIMEZONE not in config


def test_time_schema_explicit_timezone_accepted(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """TIME_SCHEMA must accept an explicit valid POSIX timezone on Arduino/IDF."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = TIME_SCHEMA({CONF_TIMEZONE: "UTC0"})
    assert config[CONF_TIMEZONE] == "UTC0"


def test_time_schema_explicit_empty_timezone_accepted(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """An empty timezone string (timezone-disable sentinel) must pass TIME_SCHEMA."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = TIME_SCHEMA({CONF_TIMEZONE: ""})
    assert config[CONF_TIMEZONE] == ""


def test_time_schema_timezone_rejected_on_zephyr(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """TIME_SCHEMA must reject a timezone value on Zephyr with the framework error.

    The platform check (cv.only_with_framework) must run BEFORE validate_tz so
    that users receive an actionable "unsupported framework" message rather than a
    confusing TZ-parsing error.
    """
    set_core_config(PlatformFramework.NRF52_ZEPHYR)
    with pytest.raises(cv.Invalid, match="only available with framework"):
        TIME_SCHEMA({CONF_TIMEZONE: "UTC0"})


def test_time_schema_invalid_tz_on_zephyr_gives_framework_error(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Even a syntactically invalid TZ string must produce the framework error on Zephyr.

    This specifically tests that cv.only_with_framework is evaluated before
    validate_tz: if the order were reversed, an invalid POSIX string would
    generate a misleading TZ-parsing error instead.
    """
    set_core_config(PlatformFramework.NRF52_ZEPHYR)
    with pytest.raises(cv.Invalid, match="only available with framework"):
        TIME_SCHEMA({CONF_TIMEZONE: "NOTAVALIDTZ!!!"})


# ---------------------------------------------------------------------------
# homeassistant/time: USE_HOMEASSISTANT_TIMEZONE define
# ---------------------------------------------------------------------------


@pytest.fixture
def mock_ha_cg():
    """Mock codegen functions used by homeassistant/time to_code."""
    with (
        mock.patch(
            "esphome.components.homeassistant.time.cg.new_Pvariable",
            return_value=mock.MagicMock(),
        ),
        mock.patch(
            "esphome.components.homeassistant.time.cg.add_define",
        ) as mock_add_define,
        mock.patch(
            "esphome.components.homeassistant.time.cg.register_component",
            new_callable=mock.AsyncMock,
        ),
        mock.patch(
            "esphome.components.homeassistant.time.time_.register_time",
            new_callable=mock.AsyncMock,
        ),
    ):
        yield mock_add_define


@pytest.mark.asyncio
async def test_ha_time_defines_ha_timezone_when_no_explicit_tz(mock_ha_cg) -> None:
    """When CONF_TIMEZONE is absent from the config, to_code() must call
    cg.add_define('USE_HOMEASSISTANT_TIMEZONE')."""
    from esphome.components.homeassistant.time import to_code

    await to_code({CONF_ID: mock.MagicMock()})

    mock_ha_cg.assert_any_call("USE_HOMEASSISTANT_TIMEZONE")


@pytest.mark.asyncio
async def test_ha_time_no_ha_timezone_define_when_explicit_tz(mock_ha_cg) -> None:
    """When CONF_TIMEZONE is present in the config, to_code() must NOT call
    cg.add_define('USE_HOMEASSISTANT_TIMEZONE')."""
    from esphome.components.homeassistant.time import to_code

    await to_code({CONF_ID: mock.MagicMock(), CONF_TIMEZONE: "UTC0"})

    define_calls = [call.args[0] for call in mock_ha_cg.call_args_list]
    assert "USE_HOMEASSISTANT_TIME" in define_calls
    assert "USE_HOMEASSISTANT_TIMEZONE" not in define_calls
