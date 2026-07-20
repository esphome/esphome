from pathlib import Path
import string

from hypothesis import example, given, settings
from hypothesis.strategies import builds, integers, ip_addresses, one_of, text
import pytest
import voluptuous as vol

from esphome import config_validation as cv
from esphome.components.esp32 import (
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
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
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_HOST,
    PLATFORM_LN882X,
    PLATFORM_RP2,
    PLATFORM_RTL87XX,
    SCHEDULER_DONT_RUN,
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
from esphome.yaml_util import ESPHomeDataBase, SensitiveStr, make_data_base


def test_check_not_templatable__invalid():
    with pytest.raises(Invalid, match="This option is not templatable!"):
        cv.check_not_templatable(Lambda(""))


@pytest.mark.parametrize("value", ("foo", 1, "D12", False))
def test_alphanumeric__valid(value):
    actual = cv.alphanumeric(value)

    assert actual == str(value)


@pytest.mark.parametrize("value", ("£23", "Foo!"))
def test_alphanumeric__invalid(value):
    with pytest.raises(Invalid):
        cv.alphanumeric(value)


@given(value=text(alphabet=string.ascii_lowercase + string.digits + "-_"))
def test_valid_name__valid(value):
    actual = cv.valid_name(value)

    assert actual == value


@pytest.mark.parametrize("value", ("foo bar", "FooBar", "foo::bar"))
def test_valid_name__invalid(value):
    with pytest.raises(Invalid):
        cv.valid_name(value)


@pytest.mark.parametrize("value", ("${name}", "${NAME}", "$NAME", "${name}_name"))
def test_valid_name__substitution_valid(value):
    CORE.vscode = True
    actual = cv.valid_name(value)
    assert actual == value

    CORE.vscode = False
    with pytest.raises(Invalid):
        actual = cv.valid_name(value)


@pytest.mark.parametrize("value", ("{NAME}", "${A NAME}"))
def test_valid_name__substitution_like_invalid(value):
    with pytest.raises(Invalid):
        cv.valid_name(value)


@pytest.mark.parametrize("value", ("myid", "anID", "SOME_ID_test", "MYID_99"))
def test_validate_id_name__valid(value):
    actual = cv.validate_id_name(value)

    assert actual == value


@pytest.mark.parametrize("value", ("id of mine", "id-4", "{name_id}", "id::name"))
def test_validate_id_name__invalid(value):
    with pytest.raises(Invalid):
        cv.validate_id_name(value)


@pytest.mark.parametrize("value", ("${id}", "${ID}", "${ID}_test_1", "$MYID"))
def test_validate_id_name__substitution_valid(value):
    CORE.vscode = True
    actual = cv.validate_id_name(value)
    assert actual == value

    CORE.vscode = False
    with pytest.raises(Invalid):
        cv.validate_id_name(value)


@given(one_of(integers(), text()))
def test_string__valid(value):
    actual = cv.string(value)

    assert actual == str(value)


@pytest.mark.parametrize("value", ({}, [], True, False, None))
def test_string__invalid(value):
    with pytest.raises(Invalid):
        cv.string(value)


@given(text())
def test_strict_string__valid(value):
    actual = cv.string_strict(value)

    assert actual == value


@pytest.mark.parametrize("value", (None, 123))
def test_string_string__invalid(value):
    with pytest.raises(Invalid, match="Must be string, got"):
        cv.string_strict(value)


def test_sensitive__default_delegates_to_string() -> None:
    validator = cv.sensitive()

    assert isinstance(validator, cv.SensitiveValidator)
    assert validator.inner is cv.string
    assert validator("hunter2") == "hunter2"
    assert validator(42) == "42"


def test_sensitive__custom_inner_delegates_validation() -> None:
    validator = cv.sensitive(cv.string_strict)

    assert validator.inner is cv.string_strict
    assert validator("abc") == "abc"
    with pytest.raises(Invalid, match="Must be string, got"):
        validator(123)


def test_sensitive__wraps_string_result_in_sensitive_str() -> None:
    validator = cv.sensitive()
    result = validator("hunter2")

    assert isinstance(result, SensitiveStr)
    assert isinstance(result, str)
    assert result == "hunter2"


def test_sensitive__does_not_double_tag_already_sensitive() -> None:
    # If the inner validator already returns a SensitiveStr (e.g., nested
    # cv.sensitive wrappers), re-tagging is a no-op rather than a new
    # SensitiveStr around the same value.
    pre_tagged = SensitiveStr("hunter2")

    def inner(_value):
        return pre_tagged

    validator = cv.sensitive(inner)
    result = validator("anything")

    assert result is pre_tagged


def test_sensitive__non_string_result_passes_through() -> None:
    # If an inner validator returns something other than a string (e.g., a
    # Lambda template), the sensitive wrapper must not coerce it.
    sentinel = object()

    def inner(_value):
        return sentinel

    validator = cv.sensitive(inner)
    assert validator("anything") is sentinel


def test_sensitive__is_detectable_via_isinstance() -> None:
    validator = cv.sensitive()

    assert isinstance(validator, cv.SensitiveValidator)


def test_bind_key__bare_usage_validates_and_is_sensitive() -> None:
    # Used bare (cv.bind_key) it is itself a sensitive validator: detectable for
    # frontend masking and validating a value directly tags the result.
    assert isinstance(cv.bind_key, cv.SensitiveValidator)

    result = cv.bind_key("0123456789ABCDEF0123456789ABCDEF")

    assert isinstance(result, SensitiveStr)
    assert result == "0123456789ABCDEF0123456789ABCDEF"


def test_bind_key__bare_usage_in_schema() -> None:
    # Voluptuous calls the bare validator with the config value; the result must
    # come through tagged sensitive.
    schema = cv.Schema({cv.Required("key"): cv.bind_key})
    out = schema({"key": "0123456789ABCDEF0123456789ABCDEF"})

    assert isinstance(out["key"], SensitiveStr)


def test_bind_key__factory_returns_sensitive_validator() -> None:
    # Called with a name (cv.bind_key(name=...)) it returns a new sensitive
    # validator rather than validating.
    validator = cv.bind_key(name="Decryption key")

    assert isinstance(validator, cv.SensitiveValidator)
    assert validator is not cv.bind_key
    assert isinstance(validator("0123456789ABCDEF0123456789ABCDEF"), SensitiveStr)


@pytest.mark.parametrize(
    ("value", "error"),
    (
        ("00", "Decryption key must consist of 16 hexadecimal numbers"),
        ("0123456789ABCDEF0123456789ABCDEG", "Decryption key must be hex values"),
    ),
)
def test_bind_key__custom_name_in_error(value: str, error: str) -> None:
    # The ``name`` argument (used by dsmr/dlms_meter) customizes error messages.
    validator = cv.bind_key(name="Decryption key")
    with pytest.raises(Invalid, match=error):
        validator(value)


def test_bind_key__rejects_non_hex_pair_length() -> None:
    # Odd-length input yields a trailing single-char part, hitting the
    # "format XX" branch rather than the hex-value branch.
    with pytest.raises(Invalid, match="Bind key must be format XX"):
        cv.bind_key("0123456789ABCDEF0123456789ABCDE")


def test_bind_key__direct_call_with_name_validates_with_that_name() -> None:
    # Passing both a value and a name validates immediately using the custom
    # name for error wording, and still tags the result sensitive.
    result = cv.bind_key("0123456789ABCDEF0123456789ABCDEF", name="Decryption key")
    assert isinstance(result, SensitiveStr)

    with pytest.raises(Invalid, match="Decryption key must consist of"):
        cv.bind_key("00", name="Decryption key")


def test_bind_key__factory_without_name_keeps_existing_name() -> None:
    # Re-invoking a named validator without a name preserves its name rather
    # than resetting to the default.
    named = cv.bind_key(name="Decryption key")
    rederived = named()

    with pytest.raises(Invalid, match="Decryption key must consist of"):
        rederived("00")


def test_bind_key__repr_is_name_keyed_and_non_recursive() -> None:
    # ``self.inner`` is a bound method of the instance, so the inherited
    # ``repr(self.inner)`` would recurse infinitely; the override keeps repr
    # finite and keyed on the name for schema-dump dedup.
    assert repr(cv.bind_key) == "bind_key('Bind key')"
    assert repr(cv.bind_key(name="Decryption key")) == "bind_key('Decryption key')"


def test_sensitive__repr_mirrors_inner() -> None:
    # The schema dump dedups on ``repr(schema)``; mirroring the inner
    # validator's repr keeps two ``cv.sensitive(cv.string)`` wrappers
    # interchangeable for that purpose and avoids leaking the wrapper as
    # noise in voluptuous error messages.
    assert repr(cv.sensitive(cv.string)) == repr(cv.string)
    assert repr(cv.sensitive(cv.string)) == repr(cv.sensitive(cv.string))


def test_sensitive_key_fragments__covers_common_terms() -> None:
    assert isinstance(cv.SENSITIVE_KEY_FRAGMENTS, frozenset)
    for term in ("password", "passcode", "secret", "token", "api_key", "apikey", "psk"):
        assert term in cv.SENSITIVE_KEY_FRAGMENTS


@given(
    builds(
        lambda v: "mdi:" + v,
        text(
            alphabet=string.ascii_letters + string.digits + "-_",
            min_size=1,
            max_size=20,
        ),
    )
)
@example("")
def test_icon__valid(value):
    actual = cv.icon(value)

    assert actual == value


def test_icon__invalid():
    with pytest.raises(Invalid, match="Icons must match the format "):
        cv.icon("foo")


def test_icon__max_length():
    """Test that icons exceeding 63 bytes are rejected."""
    # Exactly 63 bytes should pass
    max_icon = "mdi:" + "a" * 59  # 63 bytes total
    assert cv.icon(max_icon) == max_icon

    # 64 bytes should fail
    too_long = "mdi:" + "a" * 60  # 64 bytes total
    with pytest.raises(Invalid, match="Icon string is too long"):
        cv.icon(too_long)


def test_byte_length() -> None:
    """Test ByteLength validator checks UTF-8 byte length, not char count."""
    validator = cv.ByteLength(max=10)  # pylint: disable=no-member

    # ASCII: 10 chars = 10 bytes, should pass
    assert validator("a" * 10) == "a" * 10

    # ASCII: 11 chars = 11 bytes, should fail
    with pytest.raises(Invalid, match="too long.*11 bytes.*max 10"):
        validator("a" * 11)

    # Multibyte: 3 chars × 3 bytes = 9 bytes, should pass
    assert validator("温度传") == "温度传"

    # Multibyte: 4 chars × 3 bytes = 12 bytes, should fail
    with pytest.raises(Invalid, match="too long.*12 bytes.*max 10"):
        validator("温度传感")


@pytest.mark.parametrize("value", ("True", "YES", "on", "enAblE", True))
def test_boolean__valid_true(value):
    assert cv.boolean(value) is True


@pytest.mark.parametrize("value", ("False", "NO", "off", "disAblE", False))
def test_boolean__valid_false(value):
    assert cv.boolean(value) is False


@pytest.mark.parametrize("value", (None, 1, 0, "foo"))
def test_boolean__invalid(value):
    with pytest.raises(Invalid, match="Expected boolean value"):
        cv.boolean(value)


# deadline disabled: the validator is trivially fast, but Hypothesis's per-example
# deadline can spuriously trip on slow/loaded CI runners (e.g. one example hitting
# a GC pause), making this a flaky failure. Matches test_helpers.py.
@settings(deadline=None)
@given(value=ip_addresses(v=4).map(str))
def test_ipv4__valid(value):
    cv.ipv4address(value)


@pytest.mark.parametrize("value", ("127.0.0", "localhost", ""))
def test_ipv4__invalid(value):
    with pytest.raises(Invalid, match="is not a valid IPv4 address"):
        cv.ipv4address(value)


@settings(deadline=None)
@given(value=ip_addresses(v=6).map(str))
def test_ipv6__valid(value):
    cv.ipaddress(value)


@pytest.mark.parametrize("value", ("127.0.0", "localhost", "", "2001:db8::2::3"))
def test_ipv6__invalid(value):
    with pytest.raises(Invalid, match="is not a valid IP address"):
        cv.ipaddress(value)


# TODO: ensure_list
@given(integers())
def hex_int__valid(value):
    actual = cv.hex_int(value)

    assert isinstance(actual, HexInt)
    assert actual == value


@pytest.mark.parametrize(
    "framework, platform, variant, full, idf, arduino, simple",
    [
        ("arduino", PLATFORM_ESP8266, None, "1", "1", "1", "1"),
        ("arduino", PLATFORM_ESP32, VARIANT_ESP32, "3", "2", "3", "2"),
        ("esp-idf", PLATFORM_ESP32, VARIANT_ESP32, "4", "4", "2", "2"),
        ("arduino", PLATFORM_ESP32, VARIANT_ESP32C2, "3", "2", "3", "2"),
        ("esp-idf", PLATFORM_ESP32, VARIANT_ESP32C2, "4", "4", "2", "2"),
        ("arduino", PLATFORM_ESP32, VARIANT_ESP32S2, "6", "5", "6", "5"),
        ("esp-idf", PLATFORM_ESP32, VARIANT_ESP32S2, "7", "7", "5", "5"),
        ("arduino", PLATFORM_ESP32, VARIANT_ESP32S3, "9", "8", "9", "8"),
        ("esp-idf", PLATFORM_ESP32, VARIANT_ESP32S3, "10", "10", "8", "8"),
        ("arduino", PLATFORM_ESP32, VARIANT_ESP32C3, "12", "11", "12", "11"),
        ("esp-idf", PLATFORM_ESP32, VARIANT_ESP32C3, "13", "13", "11", "11"),
        ("arduino", PLATFORM_ESP32, VARIANT_ESP32C6, "15", "14", "15", "14"),
        ("esp-idf", PLATFORM_ESP32, VARIANT_ESP32C6, "16", "16", "14", "14"),
        ("arduino", PLATFORM_ESP32, VARIANT_ESP32H2, "18", "17", "18", "17"),
        ("esp-idf", PLATFORM_ESP32, VARIANT_ESP32H2, "19", "19", "17", "17"),
        ("arduino", PLATFORM_RP2, None, "20", "20", "20", "20"),
        ("arduino", PLATFORM_BK72XX, None, "21", "21", "21", "21"),
        ("arduino", PLATFORM_RTL87XX, None, "22", "22", "22", "22"),
        ("arduino", PLATFORM_LN882X, None, "23", "23", "23", "23"),
        ("host", PLATFORM_HOST, None, "24", "24", "24", "24"),
    ],
)
def test_split_default(framework, platform, variant, full, idf, arduino, simple):
    from esphome.components.esp32 import KEY_ESP32
    from esphome.const import (
        KEY_CORE,
        KEY_TARGET_FRAMEWORK,
        KEY_TARGET_PLATFORM,
        KEY_VARIANT,
    )

    CORE.data[KEY_CORE] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = platform
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = framework
    if platform == PLATFORM_ESP32:
        CORE.data[KEY_ESP32] = {}
        CORE.data[KEY_ESP32][KEY_VARIANT] = variant

    common_mappings = {
        "esp8266": "1",
        "esp32": "2",
        "esp32_s2": "5",
        "esp32_s3": "8",
        "esp32_c3": "11",
        "esp32_c6": "14",
        "esp32_h2": "17",
        "rp2": "20",
        "bk72xx": "21",
        "rtl87xx": "22",
        "ln882x": "23",
        "host": "24",
    }

    arduino_mappings = {
        "esp32_arduino": "3",
        "esp32_s2_arduino": "6",
        "esp32_s3_arduino": "9",
        "esp32_c3_arduino": "12",
        "esp32_c6_arduino": "15",
        "esp32_h2_arduino": "18",
    }

    idf_mappings = {
        "esp32_idf": "4",
        "esp32_s2_idf": "7",
        "esp32_s3_idf": "10",
        "esp32_c3_idf": "13",
        "esp32_c6_idf": "16",
        "esp32_h2_idf": "19",
    }

    schema = cv.Schema(
        {
            cv.SplitDefault(
                "full", **common_mappings, **idf_mappings, **arduino_mappings
            ): str,
            cv.SplitDefault("idf", **common_mappings, **idf_mappings): str,
            cv.SplitDefault("arduino", **common_mappings, **arduino_mappings): str,
            cv.SplitDefault("simple", **common_mappings): str,
        }
    )

    assert schema({}).get("full") == full
    assert schema({}).get("idf") == idf
    assert schema({}).get("arduino") == arduino
    assert schema({}).get("simple") == simple


@pytest.mark.parametrize(
    "framework, platform, message",
    [
        ("arduino", PLATFORM_ESP32, "ESP32 using arduino framework"),
        ("esp-idf", PLATFORM_ESP32, "ESP32 using esp-idf framework"),
        ("arduino", PLATFORM_ESP8266, "ESP8266 using arduino framework"),
        ("arduino", PLATFORM_RP2, "RP2 using arduino framework"),
        ("arduino", PLATFORM_BK72XX, "BK72XX using arduino framework"),
        ("host", PLATFORM_HOST, "HOST using host framework"),
    ],
)
def test_require_framework_version(framework, platform, message):
    from esphome.const import (
        KEY_CORE,
        KEY_FRAMEWORK_VERSION,
        KEY_TARGET_FRAMEWORK,
        KEY_TARGET_PLATFORM,
    )

    CORE.data[KEY_CORE] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = platform
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = framework
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version(1, 0, 0)

    assert (
        cv.require_framework_version(
            esp_idf=cv.Version(0, 5, 0),
            esp32_arduino=cv.Version(0, 5, 0),
            esp8266_arduino=cv.Version(0, 5, 0),
            rp2_arduino=cv.Version(0, 5, 0),
            bk72xx_arduino=cv.Version(0, 5, 0),
            host=cv.Version(0, 5, 0),
            extra_message="test 1",
        )("test")
        == "test"
    )

    with pytest.raises(
        vol.error.Invalid,
        match="This feature requires at least framework version 2.0.0. test 2",
    ):
        cv.require_framework_version(
            esp_idf=cv.Version(2, 0, 0),
            esp32_arduino=cv.Version(2, 0, 0),
            esp8266_arduino=cv.Version(2, 0, 0),
            rp2_arduino=cv.Version(2, 0, 0),
            bk72xx_arduino=cv.Version(2, 0, 0),
            host=cv.Version(2, 0, 0),
            extra_message="test 2",
        )("test")

    assert (
        cv.require_framework_version(
            esp_idf=cv.Version(1, 5, 0),
            esp32_arduino=cv.Version(1, 5, 0),
            esp8266_arduino=cv.Version(1, 5, 0),
            rp2_arduino=cv.Version(1, 5, 0),
            bk72xx_arduino=cv.Version(1, 5, 0),
            host=cv.Version(1, 5, 0),
            max_version=True,
            extra_message="test 3",
        )("test")
        == "test"
    )

    with pytest.raises(
        vol.error.Invalid,
        match="This feature requires framework version 0.5.0 or lower. test 4",
    ):
        cv.require_framework_version(
            esp_idf=cv.Version(0, 5, 0),
            esp32_arduino=cv.Version(0, 5, 0),
            esp8266_arduino=cv.Version(0, 5, 0),
            rp2_arduino=cv.Version(0, 5, 0),
            bk72xx_arduino=cv.Version(0, 5, 0),
            host=cv.Version(0, 5, 0),
            max_version=True,
            extra_message="test 4",
        )("test")

    with pytest.raises(
        vol.error.Invalid, match=f"This feature is incompatible with {message}. test 5"
    ):
        cv.require_framework_version(
            extra_message="test 5",
        )("test")


def _setup_core_for_framework(platform: str, framework: str) -> None:
    """Wire CORE.data with the minimum keys for require_framework_version /
    SplitDefault to evaluate without raising KeyError."""
    from esphome.const import (
        KEY_CORE,
        KEY_FRAMEWORK_VERSION,
        KEY_TARGET_FRAMEWORK,
        KEY_TARGET_PLATFORM,
    )

    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: platform,
        KEY_TARGET_FRAMEWORK: framework,
        KEY_FRAMEWORK_VERSION: cv.Version(1, 0, 0),
    }


def test_only_on_rp2_passes_on_rp2_platform() -> None:
    """``cv.only_on_rp2`` is the canonical family gate. It accepts any value
    untouched when the configured platform is rp2."""
    _setup_core_for_framework(PLATFORM_RP2, "arduino")
    assert cv.only_on_rp2("anything") == "anything"


def test_only_on_rp2_rejects_other_platforms() -> None:
    """The same gate raises ``Invalid`` outside the rp2 platform."""
    _setup_core_for_framework(PLATFORM_ESP32, "arduino")
    with pytest.raises(Invalid, match="rp2"):
        cv.only_on_rp2("anything")


def test_only_on_rp2040_delegates_and_warns_once(caplog) -> None:
    """``cv.only_on_rp2040`` is a deprecation shim — it logs a one-shot
    warning, dedupes via CORE.data, and delegates to ``only_on_rp2``.
    Repeated calls in the same run must not log again."""
    import logging

    _setup_core_for_framework(PLATFORM_RP2, "arduino")
    # Reset the dedupe flag so this test is independent of order.
    CORE.data.pop(cv._ONLY_ON_RP2040_DEPRECATED_KEY, None)

    with caplog.at_level(logging.WARNING, logger="esphome.config_validation"):
        assert cv.only_on_rp2040("ok") == "ok"
        first_warnings = [r for r in caplog.records if "only_on_rp2040" in r.message]
        assert len(first_warnings) == 1
        assert "2027.7.0" in first_warnings[0].message

        # Second call dedupes — no additional warning is emitted.
        assert cv.only_on_rp2040("ok") == "ok"
        warnings_after_second = [
            r for r in caplog.records if "only_on_rp2040" in r.message
        ]
        assert len(warnings_after_second) == 1


def test_only_on_rp2040_still_gates_on_non_rp2(caplog) -> None:
    """The deprecation shim must still raise on non-rp2 platforms — it
    delegates to ``only_on_rp2``, so the gating behavior is preserved."""
    import logging

    _setup_core_for_framework(PLATFORM_ESP32, "arduino")
    CORE.data.pop(cv._ONLY_ON_RP2040_DEPRECATED_KEY, None)

    with (
        caplog.at_level(logging.WARNING, logger="esphome.config_validation"),
        pytest.raises(Invalid, match="rp2"),
    ):
        cv.only_on_rp2040("anything")


def test_require_framework_version_esp32_variant_specific_key() -> None:
    """ESP32 variant-specific kwargs (``esp32_c3_arduino``) must win over
    the base ``esp32_arduino`` key when the configured variant matches."""
    from esphome.components.esp32 import KEY_ESP32
    from esphome.const import (
        KEY_CORE,
        KEY_FRAMEWORK_VERSION,
        KEY_TARGET_FRAMEWORK,
        KEY_TARGET_PLATFORM,
        KEY_VARIANT,
    )

    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: PLATFORM_ESP32,
        KEY_TARGET_FRAMEWORK: "arduino",
        KEY_FRAMEWORK_VERSION: cv.Version(1, 2, 0),
    }
    CORE.data[KEY_ESP32] = {KEY_VARIANT: VARIANT_ESP32C3}

    # Variant-specific entry permits this version; base key would reject it.
    assert (
        cv.require_framework_version(
            esp32_arduino=cv.Version(5, 0, 0),  # would reject
            esp32_c3_arduino=cv.Version(1, 0, 0),  # wins, ok
        )("test")
        == "test"
    )


def test_require_framework_version_rp2_variant_specific_key() -> None:
    """RP2 variant kwargs (``rp2_2040_arduino``) must win over the base
    ``rp2_arduino`` key when ``CORE.data['rp2']['variant']`` is wired."""
    from esphome.const import (
        KEY_CORE,
        KEY_FRAMEWORK_VERSION,
        KEY_TARGET_FRAMEWORK,
        KEY_TARGET_PLATFORM,
    )

    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: PLATFORM_RP2,
        KEY_TARGET_FRAMEWORK: "arduino",
        KEY_FRAMEWORK_VERSION: cv.Version(1, 2, 0),
    }
    CORE.data["rp2"] = {"variant": "RP2040"}

    # Variant key wins — base ``rp2_arduino`` (which would reject) is ignored.
    assert (
        cv.require_framework_version(
            rp2_arduino=cv.Version(5, 0, 0),  # would reject
            rp2_2040_arduino=cv.Version(1, 0, 0),  # wins, ok
        )("test")
        == "test"
    )

    # Without a variant kwarg the base ``rp2_arduino`` is used (fallback).
    CORE.data["rp2"] = {"variant": "RP2350"}
    assert (
        cv.require_framework_version(
            rp2_arduino=cv.Version(1, 0, 0),
        )("test")
        == "test"
    )


def test_split_default_rp2_variant_keys() -> None:
    """``SplitDefault`` resolves ``rp2_<chip>_<framework>`` first, falling
    back to ``rp2_<chip>`` and ``rp2_<framework>`` before the base key."""
    from esphome.const import KEY_CORE, KEY_TARGET_FRAMEWORK, KEY_TARGET_PLATFORM

    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: PLATFORM_RP2,
        KEY_TARGET_FRAMEWORK: "arduino",
    }
    CORE.data["rp2"] = {"variant": "RP2040"}

    schema = cv.Schema(
        {
            cv.SplitDefault(
                "full",
                rp2="base",
                rp2_arduino="base-framework",
                rp2_2040="variant-only",
                rp2_2040_arduino="variant-framework",
            ): str,
        }
    )
    # Most specific (variant + framework) wins.
    assert schema({}).get("full") == "variant-framework"

    # Drop the most-specific kwarg → variant-only wins.
    schema = cv.Schema(
        {
            cv.SplitDefault(
                "full",
                rp2="base",
                rp2_arduino="base-framework",
                rp2_2040="variant-only",
            ): str,
        }
    )
    assert schema({}).get("full") == "variant-only"

    # RP2350 variant — no rp2_2350_* kwargs → fall through to base framework.
    CORE.data["rp2"] = {"variant": "RP2350"}
    schema = cv.Schema(
        {
            cv.SplitDefault(
                "full",
                rp2="base",
                rp2_arduino="base-framework",
                rp2_2040="not-this",
            ): str,
        }
    )
    assert schema({}).get("full") == "base-framework"


def test_only_with_single_component_loaded() -> None:
    """Test OnlyWith with single component when component is loaded."""
    CORE.loaded_integrations = {"mqtt"}

    schema = cv.Schema(
        {
            cv.OnlyWith("mqtt_id", "mqtt", default="test_mqtt"): str,
        }
    )

    result = schema({})
    assert result.get("mqtt_id") == "test_mqtt"


def test_only_with_single_component_not_loaded() -> None:
    """Test OnlyWith with single component when component is not loaded."""
    CORE.loaded_integrations = set()

    schema = cv.Schema(
        {
            cv.OnlyWith("mqtt_id", "mqtt", default="test_mqtt"): str,
        }
    )

    result = schema({})
    assert "mqtt_id" not in result


def test_only_with_list_all_components_loaded() -> None:
    """Test OnlyWith with list when all components are loaded."""
    CORE.loaded_integrations = {"zigbee", "nrf52"}

    schema = cv.Schema(
        {
            cv.OnlyWith("zigbee_id", ["zigbee", "nrf52"], default="test_zigbee"): str,
        }
    )

    result = schema({})
    assert result.get("zigbee_id") == "test_zigbee"


def test_only_with_list_partial_components_loaded() -> None:
    """Test OnlyWith with list when only some components are loaded."""
    CORE.loaded_integrations = {"zigbee"}  # Only zigbee, not nrf52

    schema = cv.Schema(
        {
            cv.OnlyWith("zigbee_id", ["zigbee", "nrf52"], default="test_zigbee"): str,
        }
    )

    result = schema({})
    assert "zigbee_id" not in result


def test_only_with_list_no_components_loaded() -> None:
    """Test OnlyWith with list when no components are loaded."""
    CORE.loaded_integrations = set()

    schema = cv.Schema(
        {
            cv.OnlyWith("zigbee_id", ["zigbee", "nrf52"], default="test_zigbee"): str,
        }
    )

    result = schema({})
    assert "zigbee_id" not in result


def test_only_with_list_multiple_components() -> None:
    """Test OnlyWith with list requiring three components."""
    CORE.loaded_integrations = {"comp1", "comp2", "comp3"}

    schema = cv.Schema(
        {
            cv.OnlyWith(
                "test_id", ["comp1", "comp2", "comp3"], default="test_value"
            ): str,
        }
    )

    result = schema({})
    assert result.get("test_id") == "test_value"

    # Test with one missing
    CORE.loaded_integrations = {"comp1", "comp2"}
    result = schema({})
    assert "test_id" not in result


def test_only_with_empty_list() -> None:
    """Test OnlyWith with empty list (edge case)."""
    CORE.loaded_integrations = set()

    schema = cv.Schema(
        {
            cv.OnlyWith("test_id", [], default="test_value"): str,
        }
    )

    # all([]) returns True, so default should be applied
    result = schema({})
    assert result.get("test_id") == "test_value"


def test_only_with_user_value_overrides_default() -> None:
    """Test OnlyWith respects user-provided values over defaults."""
    CORE.loaded_integrations = {"mqtt"}

    schema = cv.Schema(
        {
            cv.OnlyWith("mqtt_id", "mqtt", default="default_id"): str,
        }
    )

    result = schema({"mqtt_id": "custom_id"})
    assert result.get("mqtt_id") == "custom_id"


@pytest.mark.parametrize("value", ("hello", "Hello World", "test_name", "温度"))
def test_string_no_slash__valid(value: str) -> None:
    actual = cv.string_no_slash(value)
    assert actual == value


@pytest.mark.parametrize(
    ("value", "expected"),
    (
        ("has/slash", "has⁄slash"),
        ("a/b/c", "a⁄b⁄c"),
        ("/leading", "⁄leading"),
        ("trailing/", "trailing⁄"),
    ),
)
def test_string_no_slash__slash_replaced_with_warning(
    value: str, expected: str, caplog: pytest.LogCaptureFixture
) -> None:
    """Test that '/' is auto-replaced with fraction slash and warning is logged."""
    actual = cv.string_no_slash(value)
    assert actual == expected
    assert "reserved as a URL path separator" in caplog.text
    assert "will become an error in ESPHome 2026.7.0" in caplog.text


def test_string_no_slash__long_string_allowed() -> None:
    # string_no_slash doesn't enforce length - use cv.Length() separately
    long_value = "x" * 200
    assert cv.string_no_slash(long_value) == long_value


def test_string_no_slash__empty() -> None:
    assert cv.string_no_slash("") == ""


@pytest.mark.parametrize("value", ("Temperature", "Living Room Light", "温度传感器"))
def test_validate_entity_name__valid(value: str) -> None:
    actual = cv._validate_entity_name(value)
    assert actual == value


def test_validate_entity_name__slash_replaced_with_warning(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test that '/' in entity names is auto-replaced with fraction slash."""
    actual = cv._validate_entity_name("has/slash")
    assert actual == "has⁄slash"
    assert "reserved as a URL path separator" in caplog.text


def test_validate_entity_name__max_length() -> None:
    # 120 bytes should pass
    assert cv._validate_entity_name("x" * 120) == "x" * 120

    # 121 bytes should fail
    with pytest.raises(Invalid, match="too long.*121 bytes.*Maximum.*120"):
        cv._validate_entity_name("x" * 121)


def test_validate_entity_name__multibyte_byte_length() -> None:
    # 40 chars of 3-byte UTF-8 = 120 bytes, should pass
    assert cv._validate_entity_name("温" * 40) == "温" * 40

    # 41 chars of 3-byte UTF-8 = 123 bytes, should fail (over 120 byte limit)
    with pytest.raises(Invalid, match="too long.*123 bytes.*Maximum.*120"):
        cv._validate_entity_name("温" * 41)


def test_validate_entity_name__none_without_friendly_name() -> None:
    # When name is "None" and friendly_name is not set, it should fail
    CORE.friendly_name = None
    with pytest.raises(Invalid, match="friendly_name is not set"):
        cv._validate_entity_name("None")


def test_validate_entity_name__none_with_friendly_name() -> None:
    # When name is "None" but friendly_name is set, it should return None
    CORE.friendly_name = "My Device"
    result = cv._validate_entity_name("None")
    assert result is None
    CORE.friendly_name = None  # Reset


# --- percentage validators ---


@pytest.mark.parametrize(
    ("value", "expected"),
    (
        ("0%", 0.0),
        ("50%", 0.5),
        ("100%", 1.0),
        (0.0, 0.0),
        (0.5, 0.5),
        (1.0, 1.0),
        ("0.0", 0.0),
        ("0.5", 0.5),
        ("1.0", 1.0),
    ),
)
def test_percentage__valid(value: object, expected: float) -> None:
    assert cv.percentage(value) == expected


@pytest.mark.parametrize(
    "value",
    (
        "150%",
        "-10%",
        "-0.1",
        "1.1",
        2,
        -1,
        "foo",
        None,
    ),
)
def test_percentage__invalid(value: object) -> None:
    with pytest.raises(Invalid):
        cv.percentage(value)


@pytest.mark.parametrize(
    ("value", "expected"),
    (
        ("0%", 0.0),
        ("50%", 0.5),
        ("100%", 1.0),
        ("-50%", -0.5),
        ("-100%", -1.0),
        (0.0, 0.0),
        (0.5, 0.5),
        (-0.5, -0.5),
        (1.0, 1.0),
        (-1.0, -1.0),
    ),
)
def test_possibly_negative_percentage__valid(value: object, expected: float) -> None:
    assert cv.possibly_negative_percentage(value) == expected


@pytest.mark.parametrize(
    "value",
    (
        "150%",
        "-150%",
        2,
        -2,
        "foo",
        None,
    ),
)
def test_possibly_negative_percentage__invalid(value: object) -> None:
    with pytest.raises(Invalid):
        cv.possibly_negative_percentage(value)


@pytest.mark.parametrize(
    ("value", "expected"),
    (
        ("0%", 0.0),
        ("50%", 0.5),
        ("100%", 1.0),
        ("150%", 1.5),
        ("200%", 2.0),
        (0.0, 0.0),
        (0.5, 0.5),
        (1.0, 1.0),
    ),
)
def test_unbounded_percentage__valid(value: object, expected: float) -> None:
    assert cv.unbounded_percentage(value) == expected


@pytest.mark.parametrize(
    "value",
    (
        "-10%",
        "-0.5",
        -1,
        "foo",
        None,
    ),
)
def test_unbounded_percentage__invalid(value: object) -> None:
    with pytest.raises(Invalid):
        cv.unbounded_percentage(value)


@pytest.mark.parametrize(
    ("value", "expected"),
    (
        ("0%", 0.0),
        ("50%", 0.5),
        ("150%", 1.5),
        ("-50%", -0.5),
        ("-150%", -1.5),
        ("200%", 2.0),
        ("-200%", -2.0),
        (0.0, 0.0),
        (0.5, 0.5),
        (-0.5, -0.5),
        (1.0, 1.0),
        (-1.0, -1.0),
    ),
)
def test_unbounded_possibly_negative_percentage__valid(
    value: object, expected: float
) -> None:
    assert cv.unbounded_possibly_negative_percentage(value) == expected


@pytest.mark.parametrize("value", ("foo", None))
def test_unbounded_possibly_negative_percentage__invalid(value: object) -> None:
    with pytest.raises(Invalid):
        cv.unbounded_possibly_negative_percentage(value)


@pytest.mark.parametrize(
    "value",
    (50, -50, 2, -2),
)
def test_percentage_validators__raw_number_above_one_without_percent_sign(
    value: object,
) -> None:
    """Raw numeric values outside [-1, 1] must use a percent sign."""
    with pytest.raises(Invalid, match="percent sign"):
        cv.unbounded_percentage(value)
    with pytest.raises(Invalid, match="percent sign"):
        cv.unbounded_possibly_negative_percentage(value)


def test_update_interval__coerces_zero_to_one_ms(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """update_interval: 0ms must be coerced to 1ms (not rejected) because a
    literal 0ms schedule causes Scheduler::call() to spin. Coercion keeps
    existing configs compiling on upgrade while emitting a user-facing
    warning that directs them to set a non-zero value."""
    with caplog.at_level("WARNING"):
        result = cv.update_interval("0ms")
    assert result.total_milliseconds == 1
    assert "update_interval of 0ms is not supported" in caplog.text
    assert "1ms" in caplog.text


def test_update_interval__preserves_nonzero_values() -> None:
    """Non-zero update_interval values must pass through unchanged."""
    assert cv.update_interval("1ms").total_milliseconds == 1
    assert cv.update_interval("50ms").total_milliseconds == 50
    assert cv.update_interval("60s").total_milliseconds == 60000


def test_update_interval__never_passes_through() -> None:
    """update_interval: never must still map to SCHEDULER_DONT_RUN."""
    result = cv.update_interval("never")
    assert result.total_milliseconds == SCHEDULER_DONT_RUN


# ---------------------------------------------------------------------------
# Visibility UI-hint kwarg
# ---------------------------------------------------------------------------


def test_optional_default_visibility_is_none() -> None:
    """An ``Optional`` with no ``visibility`` kwarg reports ``None``.

    The marker stays faithful to what the author wrote: ESPHome does
    not encode the default on it. Resolving ``None`` to an effective
    visibility is the consumer's job — a schema-aware editor treats an
    unset ``Optional`` as ``ADVANCED`` (see :class:`Visibility`).
    """
    o = cv.Optional("foo")
    assert o.visibility is None


def test_optional_visibility_advanced() -> None:
    """``visibility=Visibility.ADVANCED`` is recorded on the marker."""
    o = cv.Optional("foo", visibility=cv.Visibility.ADVANCED)
    assert o.visibility is cv.Visibility.ADVANCED


def test_optional_visibility_yaml_only() -> None:
    """``visibility=Visibility.YAML_ONLY`` is recorded on the marker."""
    o = cv.Optional("foo", visibility=cv.Visibility.YAML_ONLY)
    assert o.visibility is cv.Visibility.YAML_ONLY


def test_optional_visibility_ui() -> None:
    """``visibility=Visibility.UI`` is recorded on the marker.

    ``UI`` promotes an ``Optional`` onto the editor's main form,
    overriding the consumer's default of ``ADVANCED`` for unset
    optionals.
    """
    o = cv.Optional("foo", visibility=cv.Visibility.UI)
    assert o.visibility is cv.Visibility.UI


def test_visibility_str_values_match_dump_emission() -> None:
    """``Visibility`` is a ``StrEnum`` whose values are the literal
    strings the schema dumper emits.

    The schema bundle consumers (catalog generators, third-party
    schema-aware tooling) shouldn't need an enum import to read the
    field — pinning the on-the-wire spelling here keeps the dump
    contract stable.
    """
    assert str(cv.Visibility.UI) == "ui"
    assert str(cv.Visibility.ADVANCED) == "advanced"
    assert str(cv.Visibility.YAML_ONLY) == "yaml_only"


def test_optional_visibility_does_not_affect_validation() -> None:
    """The kwarg is an advisory UI hint — it must not change how the
    validator behaves. A schema with ``visibility`` applied must
    accept and reject the same values it would without it.
    """
    plain = cv.Schema({cv.Optional("foo", default=42): cv.int_})
    flagged = cv.Schema(
        {
            cv.Optional(
                "foo",
                default=42,
                visibility=cv.Visibility.YAML_ONLY,
            ): cv.int_
        }
    )
    # Same accept / default-fill behavior.
    assert plain({"foo": 7}) == flagged({"foo": 7}) == {"foo": 7}
    assert plain({}) == flagged({}) == {"foo": 42}
    # Same rejection on bad input.
    with pytest.raises(Invalid):
        plain({"foo": "not-an-int"})
    with pytest.raises(Invalid):
        flagged({"foo": "not-an-int"})


def test_required_default_visibility_is_none() -> None:
    """``Required`` mirrors ``Optional`` for the ``visibility`` kwarg."""
    r = cv.Required("foo")
    assert r.visibility is None


def test_required_visibility_kwarg() -> None:
    """``Required`` accepts ``visibility`` for symmetry with ``Optional``.

    Required fields rarely need the kwarg, but exposing it lets
    consumers apply uniform logic across key markers.
    """
    r = cv.Required("foo", visibility=cv.Visibility.ADVANCED)
    assert r.visibility is cv.Visibility.ADVANCED


def test_polling_component_schema_visibility_opt_in() -> None:
    """``visibility=`` propagates to the inherited ``update_interval``.

    Time platforms pass ``Visibility.ADVANCED``; sensors and other
    polling components leave it ``None`` and keep the un-flagged shape.
    """
    default = cv.polling_component_schema("15min")
    advanced = cv.polling_component_schema("15min", visibility=cv.Visibility.ADVANCED)
    default_keys = {str(k): k for k in default.schema}
    advanced_keys = {str(k): k for k in advanced.schema}
    assert default_keys["update_interval"].visibility is None
    assert advanced_keys["update_interval"].visibility is cv.Visibility.ADVANCED
    # The opt-in only touches update_interval — setup_priority
    # still inherits its YAML_ONLY visibility from COMPONENT_SCHEMA
    # in both shapes.
    assert default_keys["setup_priority"].visibility is cv.Visibility.YAML_ONLY
    assert advanced_keys["setup_priority"].visibility is cv.Visibility.YAML_ONLY


def test_polling_component_schema_no_default_ignores_visibility() -> None:
    """``visibility`` is silently ignored when the field is Required.

    When ``default_update_interval=None`` the field becomes
    ``Required``. Hiding a Required field behind an advanced
    disclosure is a UX hazard — a collapsed-by-default editor could
    let the user submit without noticing the form has an unfilled
    required field. The helper accepts the kwarg unconditionally
    for caller ergonomics but doesn't honour it on this branch.
    """
    schema = cv.polling_component_schema(None, visibility=cv.Visibility.ADVANCED)
    keys = {str(k): k for k in schema.schema}
    assert isinstance(keys["update_interval"], cv.Required)
    assert keys["update_interval"].visibility is None


def test_visibility_marker_is_per_field_no_mutation() -> None:
    """Each field's ``visibility`` is recorded as the author wrote it.

    Cascading semantics — "a stricter parent forces its descendants
    at-least as strict" — live on the consumer side, not in the
    marker itself. The schema marker stays as-written so consumers
    can walk the parent chain and compute the effective visibility
    themselves; mutating the marker would lose the per-field author
    intent.

    Pin both directions of the no-mutation contract: an inner
    ``YAML_ONLY`` under an ``ADVANCED`` parent stays ``YAML_ONLY``
    on the marker (the consumer's effective-visibility cascade
    would also report ``YAML_ONLY`` since it's stricter), and an
    un-marked inner field stays ``None`` on the marker (the
    cascade's job is to compute ``ADVANCED`` from the parent — a
    detail this test deliberately doesn't pin, since it's a
    consumer concern).
    """
    inner_unset = cv.Optional("baz")
    inner_yaml_only = cv.Optional("qux", visibility=cv.Visibility.YAML_ONLY)
    parent = cv.Optional("foo", visibility=cv.Visibility.ADVANCED)

    # Wire them into a nested schema — none of the markers' own
    # ``visibility`` should change as a result.
    schema = cv.Schema(
        {
            parent: cv.Schema(
                {
                    inner_unset: cv.int_,
                    inner_yaml_only: cv.string,
                }
            )
        }
    )
    assert schema  # touch the schema so any deferred mutation runs

    assert parent.visibility is cv.Visibility.ADVANCED
    assert inner_unset.visibility is None
    assert inner_yaml_only.visibility is cv.Visibility.YAML_ONLY


def test_entity_metadata_visibility_hints() -> None:
    """Entity and value-describing metadata is classified for visual editors.

    The headline ``name`` stays on the main form (``UI``); descriptive
    metadata (device_class, unit, …), presentation options, and per-entity
    integration plumbing (MQTT, web_server ordering) fall to the advanced
    disclosure (``ADVANCED``).
    """
    advanced = cv.Visibility.ADVANCED

    entity_base = {str(k): k for k in cv.ENTITY_BASE_SCHEMA.schema}
    assert entity_base["name"].visibility is cv.Visibility.UI
    for field in (
        "icon",
        "internal",
        "disabled_by_default",
        "entity_category",
        "device_id",
    ):
        assert entity_base[field].visibility is advanced, field

    mqtt = {str(k): k for k in cv.MQTT_COMPONENT_SCHEMA.schema}
    for field in ("qos", "retain", "discovery", "state_topic", "availability"):
        assert mqtt[field].visibility is advanced, field

    from esphome.components import binary_sensor, number, sensor
    from esphome.components.web_server import WEBSERVER_SORTING_SCHEMA

    sensor_markers = {str(k): k for k in sensor.sensor_schema().schema}
    for field in (
        "unit_of_measurement",
        "accuracy_decimals",
        "device_class",
        "state_class",
        "force_update",
    ):
        assert sensor_markers[field].visibility is advanced, field

    binary = {str(k): k for k in binary_sensor.binary_sensor_schema().schema}
    assert binary["device_class"].visibility is advanced

    number_markers = {str(k): k for k in number.number_schema(number.Number).schema}
    assert number_markers["mode"].visibility is advanced
    assert number_markers["device_class"].visibility is advanced

    # The whole per-entity web_server block is advanced; children inherit
    # via the consumer cascade, so only the parent key carries the hint.
    web = {str(k): k for k in WEBSERVER_SORTING_SCHEMA.schema}
    assert web["web_server"].visibility is advanced


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


def test_version_parse_without_patch() -> None:
    """A two-part version parses with patch defaulting to 0, so framework
    shorthands like '6.0' and '6.0-rc1' are accepted."""
    version = cv.Version.parse("6.0")
    assert (version.major, version.minor, version.patch, version.extra) == (
        6,
        0,
        0,
        "",
    )
    version = cv.Version.parse("6.0-rc1")
    assert (version.major, version.minor, version.patch, version.extra) == (
        6,
        0,
        0,
        "rc1",
    )


def test_version_parse_numeric_extra() -> None:
    """Four-part versions keep the trailing component as extra (pioarduino
    packaging revisions, e.g. 5.5.3.1)."""
    version = cv.Version.parse("5.5.3.1")
    assert (version.major, version.minor, version.patch, version.extra) == (
        5,
        5,
        3,
        "1",
    )


@pytest.mark.parametrize("value", ["not.a.version", "6", "a.b", ""])
def test_version_parse_invalid(value: str) -> None:
    with pytest.raises(ValueError, match="Not a valid version number"):
        cv.Version.parse(value)


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
    with pytest.raises(Invalid, match="not a multicast"):
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
