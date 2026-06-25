"""Tests for config_validation.py validator coverage."""

from pathlib import Path

import pytest
import voluptuous as vol

from esphome import config_validation as cv
from esphome.config_validation import Invalid
from esphome.const import (
    CONF_DAY,
    CONF_HOUR,
    CONF_ID,
    CONF_INTERNAL,
    CONF_MINUTE,
    CONF_MONTH,
    CONF_NAME,
    CONF_REF,
    CONF_SECOND,
    CONF_TYPE,
    CONF_VALUE,
    CONF_YEAR,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    TYPE_GIT,
    TYPE_LOCAL,
    Framework,
)
from esphome.core import (
    CORE,
    ID,
    HexInt,
    Lambda,
    MACAddress,
    TimePeriod,
    TimePeriodMicroseconds,
    TimePeriodMinutes,
    TimePeriodNanoseconds,
    TimePeriodSeconds,
)
from esphome.schema_extractors import SCHEMA_EXTRACT
from esphome.util import Registry
from esphome.yaml_util import ESPHomeDataBase, make_data_base


def _wrap_str(value: str) -> ESPHomeDataBase:
    """Wrap a raw string as an ESPHomeDataBase, mimicking a YAML-loaded value."""
    return make_data_base(value)


def _set_core_target(platform: str, framework: str) -> None:
    """Set CORE target platform/framework for validators that depend on them."""
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: platform,
        KEY_TARGET_FRAMEWORK: framework,
    }


def _set_framework_version(platform: str, framework: str, version: cv.Version) -> None:
    """Set CORE target platform/framework and framework version."""
    _set_core_target(platform, framework)
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = version


# ---------------------------------------------------------------------------
# Version
# ---------------------------------------------------------------------------


def test_version_str_with_extra() -> None:
    assert str(cv.Version(1, 2, 3, "b1")) == "1.2.3-b1"


def test_version_str_without_extra() -> None:
    assert str(cv.Version(1, 2, 3)) == "1.2.3"


def test_version_parse_valid() -> None:
    version = cv.Version.parse("2024.5.1")
    assert (version.major, version.minor, version.patch, version.extra) == (
        2024,
        5,
        1,
        "",
    )


def test_version_parse_with_extra() -> None:
    version = cv.Version.parse("2024.5.1-dev20240101")
    assert version.extra == "dev20240101"


def test_version_parse_invalid() -> None:
    with pytest.raises(ValueError, match="Not a valid version number"):
        cv.Version.parse("not.a.version")


def test_version_is_beta() -> None:
    assert cv.Version.parse("2024.5.0b1").is_beta is True
    assert cv.Version.parse("2024.5.0").is_beta is False


def test_version_is_dev() -> None:
    assert cv.Version.parse("2024.5.0-dev").is_dev is True
    assert cv.Version.parse("2024.5.0").is_dev is False


# ---------------------------------------------------------------------------
# alphanumeric / valid_name / validate_id_name
# ---------------------------------------------------------------------------


def test_alphanumeric_none() -> None:
    with pytest.raises(Invalid, match="string value is None"):
        cv.alphanumeric(None)


def test_valid_name_vscode_no_substitution() -> None:
    CORE.vscode = True
    assert cv.valid_name("plainname") == "plainname"


def test_validate_id_name_empty() -> None:
    with pytest.raises(Invalid, match="ID must not be empty"):
        cv.validate_id_name("")


def test_validate_id_name_digit_first() -> None:
    with pytest.raises(Invalid, match="First character in ID cannot be a digit"):
        cv.validate_id_name("1abc")


def test_validate_id_name_vscode_no_substitution() -> None:
    CORE.vscode = True
    assert cv.validate_id_name("validid") == "validid"


def test_validate_id_name_reserved() -> None:
    with pytest.raises(Invalid, match="reserved internally"):
        cv.validate_id_name("alarm")


def test_validate_id_name_integration_conflict() -> None:
    CORE.loaded_integrations = {"mqtt"}
    with pytest.raises(
        Invalid, match="conflicts with the name of an esphome integration"
    ):
        cv.validate_id_name("mqtt")


# ---------------------------------------------------------------------------
# sub_device_id
# ---------------------------------------------------------------------------


def test_sub_device_id_schema_extract() -> None:
    from esphome.core.config import Device

    assert cv.sub_device_id(SCHEMA_EXTRACT) is Device


def test_sub_device_id_empty() -> None:
    assert cv.sub_device_id(None) is None
    assert cv.sub_device_id("") is None


def test_sub_device_id_valid() -> None:
    result = cv.sub_device_id("my_device")
    assert isinstance(result, ID)
    assert result.id == "my_device"


# ---------------------------------------------------------------------------
# boolean_false / ensure_list
# ---------------------------------------------------------------------------


def test_boolean_false_valid() -> None:
    assert cv.boolean_false(False) is False
    assert cv.boolean_false("no") is False


def test_boolean_false_invalid() -> None:
    with pytest.raises(Invalid, match="Expected boolean value to be false"):
        cv.boolean_false(True)


def test_ensure_list_none() -> None:
    assert cv.ensure_list(cv.int_)(None) == []


def test_ensure_list_empty_dict() -> None:
    assert cv.ensure_list(cv.int_)({}) == []


def test_ensure_list_single_value() -> None:
    assert cv.ensure_list(cv.int_)(5) == [5]


def test_ensure_list_actual_list() -> None:
    assert cv.ensure_list(cv.int_)([1, 2, 3]) == [1, 2, 3]


# ---------------------------------------------------------------------------
# hex_int / int_to_hex_string / int_
# ---------------------------------------------------------------------------


def test_hex_int() -> None:
    result = cv.hex_int(255)
    assert result == 255
    assert isinstance(result, HexInt)


def test_int_to_hex_string_int() -> None:
    assert cv.int_to_hex_string(64) == "0x40"


def test_int_to_hex_string_passthrough() -> None:
    assert cv.int_to_hex_string("already") == "already"


def test_int_float_whole() -> None:
    assert cv.int_(5.0) == 5


def test_int_float_fractional() -> None:
    with pytest.raises(Invalid, match="only accepts integers with no fractional part"):
        cv.int_(5.5)


def test_int_hex_string() -> None:
    assert cv.int_("0xFF") == 255


# ---------------------------------------------------------------------------
# int_range / float_range no-min branches
# ---------------------------------------------------------------------------


def test_int_range_no_min() -> None:
    validator = cv.int_range(max=10)
    assert validator(5) == 5


def test_float_range_no_min() -> None:
    validator = cv.float_range(max=10.0)
    assert validator(5.0) == 5.0


# ---------------------------------------------------------------------------
# use_id / declare_id / templatable
# ---------------------------------------------------------------------------


def test_use_id_schema_extract() -> None:
    assert cv.use_id(int)(SCHEMA_EXTRACT) is int


def test_use_id_none() -> None:
    result = cv.use_id(int)(None)
    assert isinstance(result, ID)
    assert result.is_declaration is False


def test_use_id_existing_id_passthrough() -> None:
    existing = ID("foo", is_declaration=False, type=int)
    assert cv.use_id(int)(existing) is existing


def test_use_id_from_string() -> None:
    result = cv.use_id(int)("foo")
    assert isinstance(result, ID)
    assert result.id == "foo"
    assert result.is_declaration is False


def test_declare_id_schema_extract() -> None:
    assert cv.declare_id(int)(SCHEMA_EXTRACT) is int


def test_declare_id_none() -> None:
    result = cv.declare_id(int)(None)
    assert isinstance(result, ID)
    assert result.is_declaration is True


def test_declare_id_from_string() -> None:
    result = cv.declare_id(int)("foo")
    assert result.id == "foo"
    assert result.is_declaration is True


def test_templatable_schema_extract() -> None:
    assert cv.templatable(cv.int_)(SCHEMA_EXTRACT) is cv.int_


def test_templatable_lambda() -> None:
    result = cv.templatable(cv.int_)(Lambda("return 5;"))
    assert isinstance(result, Lambda)


def test_templatable_plain_value() -> None:
    assert cv.templatable(cv.int_)(5) == 5


def test_templatable_dict_validators() -> None:
    validator = cv.templatable({cv.Required("x"): cv.int_})
    assert validator({"x": 5}) == {"x": 5}


# ---------------------------------------------------------------------------
# only_on / only_with_framework
# ---------------------------------------------------------------------------


def test_only_on_list_platform_match() -> None:
    _set_core_target(PLATFORM_ESP32, "arduino")
    validator = cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266])
    assert validator("x") == "x"


def test_only_on_wrong_platform() -> None:
    _set_core_target(PLATFORM_ESP8266, "arduino")
    validator = cv.only_on(PLATFORM_ESP32)
    with pytest.raises(Invalid, match="only available on"):
        validator("x")


def test_only_with_framework_match() -> None:
    _set_core_target(PLATFORM_ESP32, "arduino")
    validator = cv.only_with_framework([Framework.ARDUINO])
    assert validator("x") == "x"


def test_only_with_framework_mismatch_with_suggestion() -> None:
    _set_core_target(PLATFORM_ESP32, "esp-idf")
    validator = cv.only_with_framework(
        Framework.ARDUINO,
        suggestions={Framework.ESP_IDF: ("some_component", "some/path")},
    )
    with pytest.raises(Invalid, match="some/path"):
        validator("x")


def test_only_with_framework_mismatch_no_suggestion() -> None:
    _set_core_target(PLATFORM_ESP32, "esp-idf")
    validator = cv.only_with_framework(Framework.ARDUINO)
    with pytest.raises(Invalid, match="only available with framework"):
        validator("x")


def test_only_with_framework_suggestion_without_docs_path() -> None:
    _set_core_target(PLATFORM_ESP32, "esp-idf")
    validator = cv.only_with_framework(
        Framework.ARDUINO,
        suggestions={Framework.ESP_IDF: ("some_component", None)},
    )
    with pytest.raises(Invalid, match="Please use 'some_component'"):
        validator("x")


# ---------------------------------------------------------------------------
# has_*_key helpers
# ---------------------------------------------------------------------------


def test_has_at_least_one_key_not_dict() -> None:
    with pytest.raises(Invalid, match="expected dictionary"):
        cv.has_at_least_one_key("a", "b")([])


def test_has_at_least_one_key_none() -> None:
    with pytest.raises(Invalid, match="at least one of"):
        cv.has_at_least_one_key("a", "b")({"c": 1})


def test_has_at_least_one_key_ok() -> None:
    obj = {"a": 1}
    assert cv.has_at_least_one_key("a", "b")(obj) is obj


def test_has_exactly_one_key_not_dict() -> None:
    with pytest.raises(Invalid, match="expected dictionary"):
        cv.has_exactly_one_key("a", "b")("notdict")


def test_has_exactly_one_key_too_many() -> None:
    with pytest.raises(Invalid, match="Cannot specify more than one"):
        cv.has_exactly_one_key("a", "b")({"a": 1, "b": 2})


def test_has_exactly_one_key_too_few() -> None:
    with pytest.raises(Invalid, match="Must contain exactly one"):
        cv.has_exactly_one_key("a", "b")({"c": 1})


def test_has_exactly_one_key_ok() -> None:
    obj = {"a": 1}
    assert cv.has_exactly_one_key("a", "b")(obj) is obj


def test_has_at_most_one_key_not_dict() -> None:
    with pytest.raises(Invalid, match="expected dictionary"):
        cv.has_at_most_one_key("a", "b")(5)


def test_has_at_most_one_key_too_many() -> None:
    with pytest.raises(vol.MultipleInvalid, match="Cannot specify more than one"):
        cv.has_at_most_one_key("a", "b")({"a": 1, "b": 2})


def test_has_at_most_one_key_ok() -> None:
    obj = {"a": 1}
    assert cv.has_at_most_one_key("a", "b")(obj) is obj


def test_has_none_or_all_keys_not_dict() -> None:
    with pytest.raises(Invalid, match="expected dictionary"):
        cv.has_none_or_all_keys("a", "b")(5)


def test_has_none_or_all_keys_partial() -> None:
    with pytest.raises(Invalid, match="none or all"):
        cv.has_none_or_all_keys("a", "b")({"a": 1})


def test_has_none_or_all_keys_all() -> None:
    obj = {"a": 1, "b": 2}
    assert cv.has_none_or_all_keys("a", "b")(obj) is obj


def test_has_none_or_all_keys_none() -> None:
    obj = {"c": 3}
    assert cv.has_none_or_all_keys("a", "b")(obj) is obj


# ---------------------------------------------------------------------------
# time_period_str_colon / time_period_str_unit
# ---------------------------------------------------------------------------


def test_time_period_str_colon_int() -> None:
    with pytest.raises(Invalid, match="wrap time values in quotes"):
        cv.time_period_str_colon(5)


def test_time_period_str_colon_not_str() -> None:
    with pytest.raises(Invalid):
        cv.time_period_str_colon([1, 2])


def test_time_period_str_colon_bad_value() -> None:
    with pytest.raises(Invalid):
        cv.time_period_str_colon("aa:bb")


def test_time_period_str_colon_hh_mm() -> None:
    assert cv.time_period_str_colon("01:30") == TimePeriod(hours=1, minutes=30)


def test_time_period_str_colon_hh_mm_ss() -> None:
    assert cv.time_period_str_colon("01:30:15") == TimePeriod(
        hours=1, minutes=30, seconds=15
    )


def test_time_period_str_colon_too_many_parts() -> None:
    with pytest.raises(Invalid):
        cv.time_period_str_colon("1:2:3:4")


def test_time_period_str_unit_int() -> None:
    with pytest.raises(Invalid, match=r"no time \*unit\*"):
        cv.time_period_str_unit(5)


def test_time_period_str_unit_timeperiod_input() -> None:
    assert cv.time_period_str_unit(TimePeriod(seconds=5)) == TimePeriod(seconds=5)


def test_time_period_str_unit_not_str() -> None:
    with pytest.raises(Invalid, match="Expected string for time period"):
        cv.time_period_str_unit([1])


def test_time_period_str_unit_no_match() -> None:
    with pytest.raises(Invalid, match="Expected time period with unit"):
        cv.time_period_str_unit("5/3")


def test_time_period_str_unit_empty_mantissa() -> None:
    with pytest.raises(Invalid):
        cv.time_period_str_unit("s")


# ---------------------------------------------------------------------------
# time_period_in_* converters
# ---------------------------------------------------------------------------


def test_time_period_in_milliseconds_too_precise() -> None:
    with pytest.raises(Invalid, match="Maximum precision is milliseconds"):
        cv.time_period_in_milliseconds_(TimePeriod(microseconds=5))


def test_time_period_in_microseconds_too_precise() -> None:
    with pytest.raises(Invalid, match="Maximum precision is microseconds"):
        cv.time_period_in_microseconds_(TimePeriod(nanoseconds=5))


def test_time_period_in_microseconds_ok() -> None:
    assert cv.time_period_in_microseconds_(
        TimePeriod(microseconds=5)
    ) == TimePeriodMicroseconds(microseconds=5)


def test_time_period_in_nanoseconds_ok() -> None:
    assert cv.time_period_in_nanoseconds_(
        TimePeriod(nanoseconds=5)
    ) == TimePeriodNanoseconds(nanoseconds=5)


@pytest.mark.parametrize(
    "value",
    [
        TimePeriod(nanoseconds=1),
        TimePeriod(microseconds=1),
        TimePeriod(milliseconds=1),
    ],
)
def test_time_period_in_seconds_too_precise(value: TimePeriod) -> None:
    with pytest.raises(Invalid, match="Maximum precision is seconds"):
        cv.time_period_in_seconds_(value)


def test_time_period_in_seconds_ok() -> None:
    assert cv.time_period_in_seconds_(TimePeriod(seconds=5)) == TimePeriodSeconds(
        seconds=5
    )


@pytest.mark.parametrize(
    "value",
    [
        TimePeriod(nanoseconds=1),
        TimePeriod(microseconds=1),
        TimePeriod(milliseconds=1),
        TimePeriod(seconds=1),
    ],
)
def test_time_period_in_minutes_too_precise(value: TimePeriod) -> None:
    with pytest.raises(Invalid, match="Maximum precision is minutes"):
        cv.time_period_in_minutes_(value)


def test_time_period_in_minutes_ok() -> None:
    assert cv.time_period_in_minutes_(TimePeriod(minutes=5)) == TimePeriodMinutes(
        minutes=5
    )


# ---------------------------------------------------------------------------
# time_of_day / date_time
# ---------------------------------------------------------------------------


def test_time_of_day_valid() -> None:
    assert cv.time_of_day("12:34:56") == {
        CONF_HOUR: 12,
        CONF_MINUTE: 34,
        CONF_SECOND: 56,
    }


def test_date_time_dict_input() -> None:
    validator = cv.date_time(date=True, time=False)
    result = validator({CONF_YEAR: 2024, CONF_MONTH: 5, CONF_DAY: 1})
    assert result[CONF_YEAR] == 2024


def test_date_time_date_only_string() -> None:
    validator = cv.date_time(date=True, time=False)
    assert validator("2024-5-1") == {CONF_YEAR: 2024, CONF_MONTH: 5, CONF_DAY: 1}


def test_date_time_date_and_time_string() -> None:
    validator = cv.date_time(date=True, time=True)
    result = validator("2024-05-01 13:30:00")
    assert result[CONF_HOUR] == 13
    assert result[CONF_YEAR] == 2024


def test_date_time_invalid_format() -> None:
    validator = cv.date_time(date=False, time=True)
    with pytest.raises(Invalid, match="Invalid time"):
        validator("notatime")


def test_date_time_ampm() -> None:
    validator = cv.date_time(date=False, time=True)
    assert validator("1:30 PM")[CONF_HOUR] == 13


def test_date_time_no_seconds() -> None:
    validator = cv.date_time(date=False, time=True)
    assert validator("13:30")[CONF_SECOND] == 0


def test_date_time_strptime_error() -> None:
    validator = cv.date_time(date=False, time=True)
    with pytest.raises(Invalid, match="Invalid time"):
        validator("25:99")


# ---------------------------------------------------------------------------
# mac_address / uuid
# ---------------------------------------------------------------------------


def test_mac_address_valid() -> None:
    result = cv.mac_address("AA:BB:CC:DD:EE:FF")
    assert isinstance(result, MACAddress)


def test_mac_address_wrong_parts() -> None:
    with pytest.raises(Invalid, match="6 : .colon. separated parts"):
        cv.mac_address("AA:BB:CC")


def test_mac_address_wrong_length() -> None:
    with pytest.raises(Invalid, match="format XX:XX"):
        cv.mac_address("A:BB:CC:DD:EE:FF")


def test_mac_address_non_hex() -> None:
    with pytest.raises(Invalid, match="hexadecimal values"):
        cv.mac_address("GG:BB:CC:DD:EE:FF")


def test_uuid_valid() -> None:
    result = cv.uuid("12345678-1234-5678-1234-567812345678")
    assert str(result) == "12345678-1234-5678-1234-567812345678"


# ---------------------------------------------------------------------------
# float_with_unit family
# ---------------------------------------------------------------------------


def test_float_with_unit_optional_unit_plain_float() -> None:
    assert cv.angle("1.5") == 1.5


def test_float_with_unit_optional_unit_with_suffix() -> None:
    assert cv.angle("45deg") == 45.0


def test_float_with_unit_with_suffix() -> None:
    assert cv.frequency("10kHz") == 10000.0


def test_float_with_unit_no_match() -> None:
    with pytest.raises(Invalid, match="Expected frequency with unit"):
        cv.frequency("!!")


def test_float_with_unit_invalid_suffix() -> None:
    with pytest.raises(Invalid, match="Invalid frequency suffix"):
        cv.frequency("10xHz")


def test_temperature_celsius() -> None:
    assert cv.temperature("25°C") == 25.0


def test_temperature_kelvin() -> None:
    assert cv.temperature("300K") == pytest.approx(300 - 273.15)


def test_temperature_fahrenheit() -> None:
    assert cv.temperature("32°F") == pytest.approx(0.0)


def test_temperature_invalid() -> None:
    with pytest.raises(Invalid, match="Invalid temperature suffix"):
        cv.temperature("5x")


def test_temperature_delta_celsius() -> None:
    assert cv.temperature_delta("5°C") == 5.0


def test_temperature_delta_kelvin() -> None:
    assert cv.temperature_delta("5K") == 5.0


def test_temperature_delta_fahrenheit() -> None:
    assert cv.temperature_delta("9°F") == pytest.approx(5.0)


def test_temperature_delta_invalid() -> None:
    with pytest.raises(Invalid, match="Invalid temperature suffix"):
        cv.temperature_delta("5x")


def test_color_temperature_mireds() -> None:
    assert cv.color_temperature("153 mireds") == pytest.approx(153.0)


def test_color_temperature_kelvin() -> None:
    assert cv.color_temperature("6536 K") == pytest.approx(1000000.0 / 6536)


def test_color_temperature_negative() -> None:
    with pytest.raises(Invalid, match="cannot be negative"):
        cv.color_temperature("-1 mireds")


# ---------------------------------------------------------------------------
# validate_bytes
# ---------------------------------------------------------------------------


def test_validate_bytes_plain() -> None:
    assert cv.validate_bytes("100") == 100


def test_validate_bytes_with_unit() -> None:
    assert cv.validate_bytes("2kB") == 2000


def test_validate_bytes_no_match() -> None:
    with pytest.raises(Invalid, match="Expected number of bytes"):
        cv.validate_bytes("abc")


def test_validate_bytes_invalid_suffix() -> None:
    with pytest.raises(Invalid, match="Invalid metric suffix"):
        cv.validate_bytes("5xx")


def test_validate_bytes_negative_exponent() -> None:
    with pytest.raises(Invalid, match="positive exponents"):
        cv.validate_bytes("5m")


# ---------------------------------------------------------------------------
# hostname / domain / domain_name / ssid
# ---------------------------------------------------------------------------


def test_hostname_valid() -> None:
    assert cv.hostname("my-host01") == "my-host01"


def test_hostname_invalid() -> None:
    with pytest.raises(Invalid, match="Invalid hostname"):
        cv.hostname("invalid_host!")


def test_domain_valid_name() -> None:
    assert cv.domain("example.com") == "example.com"


def test_domain_ip_fallback() -> None:
    assert cv.domain("::1") == "::1"


def test_domain_invalid() -> None:
    with pytest.raises(Invalid, match="Invalid domain"):
        cv.domain("::not::valid::")


def test_domain_name_empty() -> None:
    assert cv.domain_name("") == ""


def test_domain_name_valid() -> None:
    assert cv.domain_name(".local") == ".local"


def test_domain_name_no_leading_dot() -> None:
    with pytest.raises(Invalid, match="must start with"):
        cv.domain_name("local")


def test_domain_name_double_dot() -> None:
    with pytest.raises(Invalid, match="single"):
        cv.domain_name("..local")


def test_domain_name_invalid_char() -> None:
    with pytest.raises(Invalid, match="alphanumeric"):
        cv.domain_name(".local!")


def test_ssid_valid() -> None:
    assert cv.ssid("MyNetwork") == "MyNetwork"


def test_ssid_empty() -> None:
    with pytest.raises(Invalid, match="can't be empty"):
        cv.ssid("")


def test_ssid_too_long() -> None:
    with pytest.raises(Invalid, match="longer than 32"):
        cv.ssid("x" * 33)


# ---------------------------------------------------------------------------
# IP address / network validators
# ---------------------------------------------------------------------------


def test_ipv6address_valid() -> None:
    assert str(cv.ipv6address("::1")) == "::1"


def test_ipv6address_invalid() -> None:
    with pytest.raises(Invalid, match="not a valid IPv6 address"):
        cv.ipv6address("not-ipv6")


def test_ipv4address_multi_broadcast_multicast() -> None:
    assert str(cv.ipv4address_multi_broadcast("224.0.0.1")) == "224.0.0.1"


def test_ipv4address_multi_broadcast_broadcast() -> None:
    assert str(cv.ipv4address_multi_broadcast("255.255.255.255")) == "255.255.255.255"


def test_ipv4address_multi_broadcast_invalid() -> None:
    with pytest.raises(Invalid, match="not a multicasst"):
        cv.ipv4address_multi_broadcast("192.168.0.1")


def test_ipv4network_valid() -> None:
    assert str(cv.ipv4network("192.168.0.0/24")) == "192.168.0.0/24"


def test_ipv4network_invalid() -> None:
    with pytest.raises(Invalid, match="not a valid IPv4 network"):
        cv.ipv4network("notanetwork")


def test_ipv6network_valid() -> None:
    assert str(cv.ipv6network("2001:db8::/32")) == "2001:db8::/32"


def test_ipv6network_invalid() -> None:
    with pytest.raises(Invalid, match="not a valid IPv6 network"):
        cv.ipv6network("notanetwork")


def test_ipnetwork_valid() -> None:
    assert str(cv.ipnetwork("10.0.0.0/8")) == "10.0.0.0/8"


def test_ipnetwork_invalid() -> None:
    with pytest.raises(Invalid, match="not a valid IP network"):
        cv.ipnetwork("notanetwork")


# ---------------------------------------------------------------------------
# MQTT topic validators
# ---------------------------------------------------------------------------


def test_valid_topic_none() -> None:
    assert cv._valid_topic(None) == ""


def test_valid_topic_dict() -> None:
    with pytest.raises(Invalid, match="dictionary with topic"):
        cv._valid_topic({"a": 1})


def test_valid_topic_unicode_error() -> None:
    with pytest.raises(Invalid, match="valid UTF-8"):
        cv._valid_topic("\ud800")


def test_valid_topic_empty() -> None:
    with pytest.raises(Invalid, match="must not be empty"):
        cv._valid_topic("")


def test_valid_topic_too_long() -> None:
    with pytest.raises(Invalid, match="not be longer than 65535"):
        cv._valid_topic("x" * 65536)


def test_valid_topic_null_char() -> None:
    with pytest.raises(Invalid, match="null character"):
        cv._valid_topic("a\0b")


def test_subscribe_topic_valid() -> None:
    assert cv.subscribe_topic("home/+/temp") == "home/+/temp"


def test_subscribe_topic_multilevel() -> None:
    assert cv.subscribe_topic("home/#") == "home/#"


def test_subscribe_topic_bad_plus() -> None:
    with pytest.raises(Invalid, match="Single-level wildcard"):
        cv.subscribe_topic("home/a+/temp")


def test_subscribe_topic_hash_not_last() -> None:
    with pytest.raises(Invalid, match="Multi-level wildcard must be the last"):
        cv.subscribe_topic("home/#/temp")


def test_subscribe_topic_hash_not_after_separator() -> None:
    with pytest.raises(Invalid, match="must be after a topic level separator"):
        cv.subscribe_topic("home#")


def test_publish_topic_valid() -> None:
    assert cv.publish_topic("home/temp") == "home/temp"


def test_publish_topic_wildcard() -> None:
    with pytest.raises(Invalid, match="Wildcards can not be used"):
        cv.publish_topic("home/+")


def test_mqtt_payload_none() -> None:
    assert cv.mqtt_payload(None) == ""


def test_mqtt_payload_value() -> None:
    assert cv.mqtt_payload("hello") == "hello"


def test_mqtt_qos_valid() -> None:
    assert cv.mqtt_qos("1") == 1


def test_mqtt_qos_not_int() -> None:
    with pytest.raises(Invalid, match="must be integer"):
        cv.mqtt_qos("abc")


def test_mqtt_qos_out_of_range() -> None:
    with pytest.raises(Invalid):
        cv.mqtt_qos(5)


# ---------------------------------------------------------------------------
# requires_component / conflicts_with_component
# ---------------------------------------------------------------------------


def test_requires_component_loaded() -> None:
    CORE.loaded_integrations = {"mqtt"}
    assert cv.requires_component("mqtt")("x") == "x"


def test_requires_component_not_loaded() -> None:
    CORE.loaded_integrations = set()
    with pytest.raises(Invalid, match="requires component mqtt"):
        cv.requires_component("mqtt")("x")


def test_conflicts_with_component_loaded() -> None:
    CORE.loaded_integrations = {"mqtt"}
    with pytest.raises(Invalid, match="not compatible with component mqtt"):
        cv.conflicts_with_component("mqtt")("x")


def test_conflicts_with_component_not_loaded() -> None:
    CORE.loaded_integrations = set()
    assert cv.conflicts_with_component("mqtt")("x") == "x"


# ---------------------------------------------------------------------------
# percentage_int / invalid / valid
# ---------------------------------------------------------------------------


def test_percentage_int_with_percent() -> None:
    assert cv.percentage_int("50%") == 50


def test_percentage_int_plain() -> None:
    assert cv.percentage_int(50) == 50


def test_invalid_always_raises() -> None:
    with pytest.raises(Invalid, match="my message"):
        cv.invalid("my message")("anything")


def test_valid_returns_value() -> None:
    obj = object()
    assert cv.valid(obj) is obj


# ---------------------------------------------------------------------------
# prepend_path / remove_prepend_path
# ---------------------------------------------------------------------------


def test_prepend_path_single() -> None:
    with pytest.raises(Invalid) as exc_info, cv.prepend_path("foo"):
        raise Invalid("bad")
    assert list(exc_info.value.path) == ["foo"]


def test_prepend_path_list() -> None:
    with pytest.raises(Invalid) as exc_info, cv.prepend_path(["a", "b"]):
        raise Invalid("bad")
    assert list(exc_info.value.path) == ["a", "b"]


def test_remove_prepend_path_matching() -> None:
    with pytest.raises(Invalid) as exc_info, cv.remove_prepend_path(["a"]):
        raise Invalid("bad", path=["a", "b"])
    assert list(exc_info.value.path) == ["b"]


def test_remove_prepend_path_non_matching() -> None:
    with pytest.raises(Invalid) as exc_info, cv.remove_prepend_path("x"):
        raise Invalid("bad", path=["a", "b"])
    assert list(exc_info.value.path) == ["a", "b"]


# ---------------------------------------------------------------------------
# one_of / enum
# ---------------------------------------------------------------------------


def test_one_of_extra_kwargs() -> None:
    with pytest.raises(ValueError):
        cv.one_of(1, 2, bogus=True)


def test_one_of_schema_extract() -> None:
    assert cv.one_of("a", "b")(SCHEMA_EXTRACT) == ("a", "b")


def test_one_of_string_and_space() -> None:
    assert cv.one_of("a_b", string=True, space="_")("a b") == "a_b"


def test_one_of_int() -> None:
    assert cv.one_of(1, 2, int=True)("2") == 2


def test_one_of_float() -> None:
    assert cv.one_of(1.0, 2.0, float=True)("2.0") == 2.0


def test_one_of_lower() -> None:
    assert cv.one_of("abc", lower=True)("ABC") == "abc"


def test_one_of_upper() -> None:
    assert cv.one_of("ABC", upper=True)("abc") == "ABC"


def test_one_of_unknown_with_suggestion() -> None:
    with pytest.raises(Invalid, match="did you mean"):
        cv.one_of("apple", "banana")("aple")


def test_one_of_unknown_no_suggestion() -> None:
    with pytest.raises(Invalid, match="valid options are"):
        cv.one_of("apple", "banana")("zzzzzz")


def test_enum_schema_extract() -> None:
    mapping = {"a": 1, "b": 2}
    assert cv.enum(mapping)(SCHEMA_EXTRACT) == mapping


def test_enum_valid() -> None:
    mapping = {"a": 10, "b": 20}
    result = cv.enum(mapping)("a")
    assert result == "a"
    assert result.enum_value == 10


# ---------------------------------------------------------------------------
# lambda_ / returning_lambda
# ---------------------------------------------------------------------------


def test_lambda_from_string() -> None:
    result = cv.lambda_(_wrap_str("return 5;"))
    assert isinstance(result, Lambda)
    assert result.value == "return 5;"


def test_lambda_existing_lambda() -> None:
    lam = Lambda("x")
    assert cv.lambda_(lam) is lam


def test_lambda_entity_id_reference() -> None:
    with pytest.raises(Invalid, match="entity-id-style ID"):
        cv.lambda_(Lambda("return id(light.living_room);"))


def test_returning_lambda_valid() -> None:
    assert isinstance(cv.returning_lambda(_wrap_str("return 5;")), Lambda)


def test_returning_lambda_no_return() -> None:
    with pytest.raises(Invalid, match="return statement"):
        cv.returning_lambda(Lambda("int x = 5;"))


# ---------------------------------------------------------------------------
# dimensions
# ---------------------------------------------------------------------------


def test_dimensions_list_valid() -> None:
    assert cv.dimensions([320, 240]) == [320, 240]


def test_dimensions_list_wrong_length() -> None:
    with pytest.raises(Invalid, match="length of two"):
        cv.dimensions([1, 2, 3])


def test_dimensions_list_non_int() -> None:
    with pytest.raises(Invalid, match="must be integers"):
        cv.dimensions(["a", "b"])


def test_dimensions_list_non_positive() -> None:
    with pytest.raises(Invalid, match="at least be 1"):
        cv.dimensions([0, 240])


def test_dimensions_string_valid() -> None:
    assert cv.dimensions("320x240") == [320, 240]


def test_dimensions_number_invalid() -> None:
    with pytest.raises(Invalid, match="must be a string"):
        cv.dimensions(320)


def test_dimensions_string_invalid() -> None:
    with pytest.raises(Invalid, match="Only WIDTHxHEIGHT"):
        cv.dimensions("notdimensions")


# ---------------------------------------------------------------------------
# entity_id
# ---------------------------------------------------------------------------


def test_entity_id_valid() -> None:
    assert cv.entity_id("Light.Living_Room") == "light.living_room"


def test_entity_id_no_dot() -> None:
    with pytest.raises(Invalid, match="exactly one dot"):
        cv.entity_id("nodot")


def test_entity_id_invalid_char() -> None:
    with pytest.raises(Invalid, match="Invalid character"):
        cv.entity_id("light.living!room")


# ---------------------------------------------------------------------------
# extract_keys / typed_schema
# ---------------------------------------------------------------------------


def test_extract_keys_from_schema() -> None:
    schema = cv.Schema({cv.Optional("b"): cv.int_, cv.Required("a"): cv.int_})
    assert cv.extract_keys(schema) == ["a", "b"]


def test_extract_keys_from_dict() -> None:
    assert cv.extract_keys({"x": cv.int_, cv.Optional("y"): cv.int_}) == ["x", "y"]


def test_extract_keys_invalid_key() -> None:
    with pytest.raises(ValueError):
        cv.extract_keys({1: cv.int_})


def test_typed_schema_basic() -> None:
    schema = cv.typed_schema({"foo": cv.Schema({cv.Optional("x"): cv.int_})})
    assert schema({"type": "foo", "x": 5}) == {"type": "foo", "x": 5}


def test_typed_schema_not_dict() -> None:
    schema = cv.typed_schema({"foo": cv.Schema({})})
    with pytest.raises(Invalid, match="must be dict"):
        schema("notdict")


def test_typed_schema_missing_key() -> None:
    schema = cv.typed_schema({"foo": cv.Schema({})})
    with pytest.raises(Invalid, match="type not specified"):
        schema({"x": 5})


def test_typed_schema_default_type() -> None:
    schema = cv.typed_schema({"foo": cv.Schema({})}, default_type="foo")
    assert schema({}) == {"type": "foo"}


def test_typed_schema_with_enum() -> None:
    schema = cv.typed_schema({"foo": cv.Schema({})}, enum={"foo": 42})
    result = schema({"type": "foo"})
    assert result["type"] == "foo"
    assert result["type"].enum_value == 42


# ---------------------------------------------------------------------------
# SplitDefault / OnlyWithout
# ---------------------------------------------------------------------------


def test_split_default_no_match() -> None:
    _set_core_target(PLATFORM_ESP8266, "arduino")
    schema = cv.Schema({cv.SplitDefault("key", esp32="value"): cv.string})
    assert "key" not in schema({})


def test_only_without_component_absent() -> None:
    CORE.loaded_integrations = set()
    schema = cv.Schema({cv.OnlyWithout("key", "mqtt", default="dval"): cv.string})
    assert schema({})["key"] == "dval"


def test_only_without_component_present() -> None:
    CORE.loaded_integrations = {"mqtt"}
    schema = cv.Schema({cv.OnlyWithout("key", "mqtt", default="dval"): cv.string})
    assert "key" not in schema({})


# ---------------------------------------------------------------------------
# _entity_base_validator / ensure_schema
# ---------------------------------------------------------------------------


def test_entity_base_validator_name_present() -> None:
    result = cv._entity_base_validator({CONF_NAME: "My Name"})
    assert result[CONF_NAME] == "My Name"


def test_entity_base_validator_neither() -> None:
    with pytest.raises(Invalid, match="'id:' or 'name:' is required"):
        cv._entity_base_validator({})


def test_entity_base_validator_id_not_manual() -> None:
    config = {CONF_ID: ID("auto", is_declaration=True, type=int, is_manual=False)}
    with pytest.raises(Invalid, match="'id:' or 'name:' is required"):
        cv._entity_base_validator(config)


def test_entity_base_validator_id_manual() -> None:
    config = {CONF_ID: ID("myid", is_declaration=True, type=int, is_manual=True)}
    result = cv._entity_base_validator(config)
    assert result[CONF_NAME] == "myid"
    assert result[CONF_INTERNAL] is True


def test_entity_base_validator_name_none() -> None:
    result = cv._entity_base_validator({CONF_NAME: None})
    assert result[CONF_NAME] == ""


def test_ensure_schema_passthrough() -> None:
    schema = cv.Schema({})
    assert cv.ensure_schema(schema) is schema


def test_ensure_schema_wraps() -> None:
    result = cv.ensure_schema({cv.Optional("x"): cv.int_})
    assert isinstance(result, cv.Schema)


# ---------------------------------------------------------------------------
# validate_registry_entry
# ---------------------------------------------------------------------------


def _make_registry(*names: str, type_id: object = int) -> Registry:
    registry = Registry()
    for name in names:
        registry.register(name, type_id, cv.Schema({cv.Optional("param"): cv.int_}))(
            lambda: None
        )
    return registry


def test_validate_registry_entry_string_shorthand() -> None:
    registry = _make_registry("foo")
    result = cv.validate_registry_entry("action", registry)("foo")
    assert "foo" in result


def test_validate_registry_entry_not_mapping() -> None:
    registry = _make_registry()
    with pytest.raises(Invalid, match="must consist of key-value mapping"):
        cv.validate_registry_entry("action", registry)(5)


def test_validate_registry_entry_missing_key() -> None:
    registry = _make_registry()
    with pytest.raises(Invalid, match="Key missing"):
        cv.validate_registry_entry("action", registry)({})


def test_validate_registry_entry_unknown_key() -> None:
    registry = _make_registry()
    with pytest.raises(Invalid, match="Unable to find action"):
        cv.validate_registry_entry("action", registry)({"unknown": {}})


def test_validate_registry_entry_two_keys() -> None:
    registry = _make_registry("foo", "bar")
    with pytest.raises(Invalid, match="Cannot have two action"):
        cv.validate_registry_entry("action", registry)({"foo": {}, "bar": {}})


def test_validate_registry_entry_none_value() -> None:
    registry = _make_registry("foo")
    result = cv.validate_registry_entry("action", registry)({"foo": None})
    assert "foo" in result


def test_validate_registry_entry_no_type_id() -> None:
    registry = _make_registry("foo", type_id=None)
    result = cv.validate_registry_entry("action", registry)({"foo": {}})
    assert "foo" in result


# ---------------------------------------------------------------------------
# maybe_simple_value / entity_category
# ---------------------------------------------------------------------------


def test_maybe_simple_value_schema_extract() -> None:
    schema = cv.Schema({cv.Required(CONF_VALUE): cv.string})
    validator, key = cv.maybe_simple_value(schema)(SCHEMA_EXTRACT)
    assert key == CONF_VALUE


def test_maybe_simple_value_dict_with_key() -> None:
    schema = cv.Schema({cv.Required(CONF_VALUE): cv.string})
    assert cv.maybe_simple_value(schema)({"value": "x"}) == {"value": "x"}


def test_maybe_simple_value_plain() -> None:
    schema = cv.Schema({cv.Required(CONF_VALUE): cv.string})
    assert cv.maybe_simple_value(schema)("x") == {"value": "x"}


def test_maybe_simple_value_custom_key() -> None:
    schema = cv.Schema({cv.Required("name"): cv.string})
    assert cv.maybe_simple_value(schema, key="name")({"name": "x"}) == {"name": "x"}


def test_entity_category_valid() -> None:
    assert cv.entity_category("config") == "config"


def test_entity_category_invalid() -> None:
    with pytest.raises(Invalid):
        cv.entity_category("bogus")


# ---------------------------------------------------------------------------
# url / git_ref / source_refresh / version helpers
# ---------------------------------------------------------------------------


def test_url_valid() -> None:
    assert cv.url("https://example.com/path") == "https://example.com/path"


def test_url_file_scheme() -> None:
    assert cv.url("file:///tmp/x") == "file:///tmp/x"


def test_url_invalid_value_error() -> None:
    with pytest.raises(Invalid, match="Not a valid URL"):
        cv.url("http://[::1")


def test_url_no_host() -> None:
    with pytest.raises(Invalid, match="Expected a file scheme"):
        cv.url("notaurl")


def test_git_ref_valid() -> None:
    assert cv.git_ref("v1.2.3") == "v1.2.3"


def test_git_ref_invalid() -> None:
    with pytest.raises(Invalid, match="Not a valid git ref"):
        cv.git_ref("!!!")


def test_source_refresh_always() -> None:
    assert cv.source_refresh("always").total_seconds == 0


def test_source_refresh_never() -> None:
    assert cv.source_refresh("never").total_seconds == 365250 * 24 * 3600


def test_source_refresh_value() -> None:
    assert cv.source_refresh("60s").total_seconds == 60


def test_version_number_valid() -> None:
    assert cv.version_number("2024.5.1") == "2024.5.1"


def test_version_number_invalid() -> None:
    with pytest.raises(Invalid, match="Not a valid version number"):
        cv.version_number("notaversion")


def test_validate_esphome_version_ok() -> None:
    assert cv.validate_esphome_version("1.0.0") == "1.0.0"


def test_validate_esphome_version_too_old() -> None:
    with pytest.raises(Invalid, match="ESPHome version is too old"):
        cv.validate_esphome_version("9999.0.0")


def test_platformio_version_constraint_no_op() -> None:
    assert cv.platformio_version_constraint("1.2.3") == [(None, "1.2.3")]


def test_platformio_version_constraint_with_ops() -> None:
    assert cv.platformio_version_constraint(">=1.2.3,<2.0.0") == [
        (">=", "1.2.3"),
        ("<", "2.0.0"),
    ]


# ---------------------------------------------------------------------------
# require_framework_version (no extra_message) / require_esphome_version
# ---------------------------------------------------------------------------


def test_require_framework_version_incompatible_no_extra() -> None:
    _set_framework_version(PLATFORM_ESP32, "arduino", cv.Version(1, 0, 0))
    with pytest.raises(Invalid, match="incompatible with ESP32"):
        cv.require_framework_version()("test")


def test_require_framework_version_too_low_no_extra() -> None:
    _set_framework_version(PLATFORM_ESP32, "arduino", cv.Version(1, 0, 0))
    with pytest.raises(Invalid, match="at least framework version 2.0.0"):
        cv.require_framework_version(esp32_arduino=cv.Version(2, 0, 0))("test")


def test_require_framework_version_too_high_no_extra() -> None:
    _set_framework_version(PLATFORM_ESP32, "arduino", cv.Version(2, 0, 0))
    with pytest.raises(Invalid, match="version 1.0.0 or lower"):
        cv.require_framework_version(
            esp32_arduino=cv.Version(1, 0, 0), max_version=True
        )("test")


def test_require_esphome_version_ok() -> None:
    assert cv.require_esphome_version(1, 0, 0)("test") == "test"


def test_require_esphome_version_too_old() -> None:
    with pytest.raises(Invalid, match="at least ESPHome version 9999.0.0"):
        cv.require_esphome_version(9999, 0, 0)("test")


# ---------------------------------------------------------------------------
# suppress_invalid / validate_source_shorthand / rename_key
# ---------------------------------------------------------------------------


def test_suppress_invalid() -> None:
    with cv.suppress_invalid():
        raise Invalid("suppressed")


def test_validate_source_shorthand_not_string() -> None:
    with pytest.raises(Invalid, match="Shorthand only for strings"):
        cv.validate_source_shorthand(123)


def test_validate_source_shorthand_local_path(setup_core: Path) -> None:
    (setup_core / "mydir").mkdir()
    result = cv.validate_source_shorthand("mydir")
    assert result[CONF_TYPE] == TYPE_LOCAL


def test_validate_source_shorthand_github(setup_core: Path) -> None:
    result = cv.validate_source_shorthand("github://user/repo@main")
    assert result[CONF_TYPE] == TYPE_GIT
    assert result[CONF_REF] == "main"


def test_validate_source_shorthand_github_no_ref(setup_core: Path) -> None:
    result = cv.validate_source_shorthand("github://user/repo")
    assert result[CONF_TYPE] == TYPE_GIT
    assert CONF_REF not in result


def test_validate_source_shorthand_github_pr(setup_core: Path) -> None:
    result = cv.validate_source_shorthand("github://pr#1234")
    assert result[CONF_REF] == "pull/1234/head"


def test_validate_source_shorthand_invalid(setup_core: Path) -> None:
    with pytest.raises(Invalid, match="not a file system path"):
        cv.validate_source_shorthand("notvalid")


def test_rename_key_present() -> None:
    assert cv.rename_key("old", "new")({"old": 5}) == {"new": 5}


def test_rename_key_absent() -> None:
    assert cv.rename_key("old", "new")({"other": 5}) == {"other": 5}
