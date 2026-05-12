import string

from hypothesis import example, given
from hypothesis.strategies import builds, integers, ip_addresses, one_of, text
import pytest
import voluptuous as vol

from esphome import config_validation
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
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_HOST,
    PLATFORM_LN882X,
    PLATFORM_RP2040,
    PLATFORM_RTL87XX,
    SCHEDULER_DONT_RUN,
)
from esphome.core import CORE, HexInt, Lambda


def test_check_not_templatable__invalid():
    with pytest.raises(Invalid, match="This option is not templatable!"):
        config_validation.check_not_templatable(Lambda(""))


@pytest.mark.parametrize("value", ("foo", 1, "D12", False))
def test_alphanumeric__valid(value):
    actual = config_validation.alphanumeric(value)

    assert actual == str(value)


@pytest.mark.parametrize("value", ("£23", "Foo!"))
def test_alphanumeric__invalid(value):
    with pytest.raises(Invalid):
        config_validation.alphanumeric(value)


@given(value=text(alphabet=string.ascii_lowercase + string.digits + "-_"))
def test_valid_name__valid(value):
    actual = config_validation.valid_name(value)

    assert actual == value


@pytest.mark.parametrize("value", ("foo bar", "FooBar", "foo::bar"))
def test_valid_name__invalid(value):
    with pytest.raises(Invalid):
        config_validation.valid_name(value)


@pytest.mark.parametrize("value", ("${name}", "${NAME}", "$NAME", "${name}_name"))
def test_valid_name__substitution_valid(value):
    CORE.vscode = True
    actual = config_validation.valid_name(value)
    assert actual == value

    CORE.vscode = False
    with pytest.raises(Invalid):
        actual = config_validation.valid_name(value)


@pytest.mark.parametrize("value", ("{NAME}", "${A NAME}"))
def test_valid_name__substitution_like_invalid(value):
    with pytest.raises(Invalid):
        config_validation.valid_name(value)


@pytest.mark.parametrize("value", ("myid", "anID", "SOME_ID_test", "MYID_99"))
def test_validate_id_name__valid(value):
    actual = config_validation.validate_id_name(value)

    assert actual == value


@pytest.mark.parametrize("value", ("id of mine", "id-4", "{name_id}", "id::name"))
def test_validate_id_name__invalid(value):
    with pytest.raises(Invalid):
        config_validation.validate_id_name(value)


@pytest.mark.parametrize("value", ("${id}", "${ID}", "${ID}_test_1", "$MYID"))
def test_validate_id_name__substitution_valid(value):
    CORE.vscode = True
    actual = config_validation.validate_id_name(value)
    assert actual == value

    CORE.vscode = False
    with pytest.raises(Invalid):
        config_validation.validate_id_name(value)


@given(one_of(integers(), text()))
def test_string__valid(value):
    actual = config_validation.string(value)

    assert actual == str(value)


@pytest.mark.parametrize("value", ({}, [], True, False, None))
def test_string__invalid(value):
    with pytest.raises(Invalid):
        config_validation.string(value)


@given(text())
def test_strict_string__valid(value):
    actual = config_validation.string_strict(value)

    assert actual == value


@pytest.mark.parametrize("value", (None, 123))
def test_string_string__invalid(value):
    with pytest.raises(Invalid, match="Must be string, got"):
        config_validation.string_strict(value)


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
    actual = config_validation.icon(value)

    assert actual == value


def test_icon__invalid():
    with pytest.raises(Invalid, match="Icons must match the format "):
        config_validation.icon("foo")


def test_icon__max_length():
    """Test that icons exceeding 63 bytes are rejected."""
    # Exactly 63 bytes should pass
    max_icon = "mdi:" + "a" * 59  # 63 bytes total
    assert config_validation.icon(max_icon) == max_icon

    # 64 bytes should fail
    too_long = "mdi:" + "a" * 60  # 64 bytes total
    with pytest.raises(Invalid, match="Icon string is too long"):
        config_validation.icon(too_long)


def test_byte_length() -> None:
    """Test ByteLength validator checks UTF-8 byte length, not char count."""
    validator = config_validation.ByteLength(max=10)  # pylint: disable=no-member

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
    assert config_validation.boolean(value) is True


@pytest.mark.parametrize("value", ("False", "NO", "off", "disAblE", False))
def test_boolean__valid_false(value):
    assert config_validation.boolean(value) is False


@pytest.mark.parametrize("value", (None, 1, 0, "foo"))
def test_boolean__invalid(value):
    with pytest.raises(Invalid, match="Expected boolean value"):
        config_validation.boolean(value)


@given(value=ip_addresses(v=4).map(str))
def test_ipv4__valid(value):
    config_validation.ipv4address(value)


@pytest.mark.parametrize("value", ("127.0.0", "localhost", ""))
def test_ipv4__invalid(value):
    with pytest.raises(Invalid, match="is not a valid IPv4 address"):
        config_validation.ipv4address(value)


@given(value=ip_addresses(v=6).map(str))
def test_ipv6__valid(value):
    config_validation.ipaddress(value)


@pytest.mark.parametrize("value", ("127.0.0", "localhost", "", "2001:db8::2::3"))
def test_ipv6__invalid(value):
    with pytest.raises(Invalid, match="is not a valid IP address"):
        config_validation.ipaddress(value)


# TODO: ensure_list
@given(integers())
def hex_int__valid(value):
    actual = config_validation.hex_int(value)

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
        ("arduino", PLATFORM_RP2040, None, "20", "20", "20", "20"),
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
        "rp2040": "20",
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

    schema = config_validation.Schema(
        {
            config_validation.SplitDefault(
                "full", **common_mappings, **idf_mappings, **arduino_mappings
            ): str,
            config_validation.SplitDefault(
                "idf", **common_mappings, **idf_mappings
            ): str,
            config_validation.SplitDefault(
                "arduino", **common_mappings, **arduino_mappings
            ): str,
            config_validation.SplitDefault("simple", **common_mappings): str,
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
        ("arduino", PLATFORM_RP2040, "RP2040 using arduino framework"),
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
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = config_validation.Version(1, 0, 0)

    assert (
        config_validation.require_framework_version(
            esp_idf=config_validation.Version(0, 5, 0),
            esp32_arduino=config_validation.Version(0, 5, 0),
            esp8266_arduino=config_validation.Version(0, 5, 0),
            rp2040_arduino=config_validation.Version(0, 5, 0),
            bk72xx_arduino=config_validation.Version(0, 5, 0),
            host=config_validation.Version(0, 5, 0),
            extra_message="test 1",
        )("test")
        == "test"
    )

    with pytest.raises(
        vol.error.Invalid,
        match="This feature requires at least framework version 2.0.0. test 2",
    ):
        config_validation.require_framework_version(
            esp_idf=config_validation.Version(2, 0, 0),
            esp32_arduino=config_validation.Version(2, 0, 0),
            esp8266_arduino=config_validation.Version(2, 0, 0),
            rp2040_arduino=config_validation.Version(2, 0, 0),
            bk72xx_arduino=config_validation.Version(2, 0, 0),
            host=config_validation.Version(2, 0, 0),
            extra_message="test 2",
        )("test")

    assert (
        config_validation.require_framework_version(
            esp_idf=config_validation.Version(1, 5, 0),
            esp32_arduino=config_validation.Version(1, 5, 0),
            esp8266_arduino=config_validation.Version(1, 5, 0),
            rp2040_arduino=config_validation.Version(1, 5, 0),
            bk72xx_arduino=config_validation.Version(1, 5, 0),
            host=config_validation.Version(1, 5, 0),
            max_version=True,
            extra_message="test 3",
        )("test")
        == "test"
    )

    with pytest.raises(
        vol.error.Invalid,
        match="This feature requires framework version 0.5.0 or lower. test 4",
    ):
        config_validation.require_framework_version(
            esp_idf=config_validation.Version(0, 5, 0),
            esp32_arduino=config_validation.Version(0, 5, 0),
            esp8266_arduino=config_validation.Version(0, 5, 0),
            rp2040_arduino=config_validation.Version(0, 5, 0),
            bk72xx_arduino=config_validation.Version(0, 5, 0),
            host=config_validation.Version(0, 5, 0),
            max_version=True,
            extra_message="test 4",
        )("test")

    with pytest.raises(
        vol.error.Invalid, match=f"This feature is incompatible with {message}. test 5"
    ):
        config_validation.require_framework_version(
            extra_message="test 5",
        )("test")


def test_only_with_single_component_loaded() -> None:
    """Test OnlyWith with single component when component is loaded."""
    CORE.loaded_integrations = {"mqtt"}

    schema = config_validation.Schema(
        {
            config_validation.OnlyWith("mqtt_id", "mqtt", default="test_mqtt"): str,
        }
    )

    result = schema({})
    assert result.get("mqtt_id") == "test_mqtt"


def test_only_with_single_component_not_loaded() -> None:
    """Test OnlyWith with single component when component is not loaded."""
    CORE.loaded_integrations = set()

    schema = config_validation.Schema(
        {
            config_validation.OnlyWith("mqtt_id", "mqtt", default="test_mqtt"): str,
        }
    )

    result = schema({})
    assert "mqtt_id" not in result


def test_only_with_list_all_components_loaded() -> None:
    """Test OnlyWith with list when all components are loaded."""
    CORE.loaded_integrations = {"zigbee", "nrf52"}

    schema = config_validation.Schema(
        {
            config_validation.OnlyWith(
                "zigbee_id", ["zigbee", "nrf52"], default="test_zigbee"
            ): str,
        }
    )

    result = schema({})
    assert result.get("zigbee_id") == "test_zigbee"


def test_only_with_list_partial_components_loaded() -> None:
    """Test OnlyWith with list when only some components are loaded."""
    CORE.loaded_integrations = {"zigbee"}  # Only zigbee, not nrf52

    schema = config_validation.Schema(
        {
            config_validation.OnlyWith(
                "zigbee_id", ["zigbee", "nrf52"], default="test_zigbee"
            ): str,
        }
    )

    result = schema({})
    assert "zigbee_id" not in result


def test_only_with_list_no_components_loaded() -> None:
    """Test OnlyWith with list when no components are loaded."""
    CORE.loaded_integrations = set()

    schema = config_validation.Schema(
        {
            config_validation.OnlyWith(
                "zigbee_id", ["zigbee", "nrf52"], default="test_zigbee"
            ): str,
        }
    )

    result = schema({})
    assert "zigbee_id" not in result


def test_only_with_list_multiple_components() -> None:
    """Test OnlyWith with list requiring three components."""
    CORE.loaded_integrations = {"comp1", "comp2", "comp3"}

    schema = config_validation.Schema(
        {
            config_validation.OnlyWith(
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

    schema = config_validation.Schema(
        {
            config_validation.OnlyWith("test_id", [], default="test_value"): str,
        }
    )

    # all([]) returns True, so default should be applied
    result = schema({})
    assert result.get("test_id") == "test_value"


def test_only_with_user_value_overrides_default() -> None:
    """Test OnlyWith respects user-provided values over defaults."""
    CORE.loaded_integrations = {"mqtt"}

    schema = config_validation.Schema(
        {
            config_validation.OnlyWith("mqtt_id", "mqtt", default="default_id"): str,
        }
    )

    result = schema({"mqtt_id": "custom_id"})
    assert result.get("mqtt_id") == "custom_id"


@pytest.mark.parametrize("value", ("hello", "Hello World", "test_name", "温度"))
def test_string_no_slash__valid(value: str) -> None:
    actual = config_validation.string_no_slash(value)
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
    actual = config_validation.string_no_slash(value)
    assert actual == expected
    assert "reserved as a URL path separator" in caplog.text
    assert "will become an error in ESPHome 2026.7.0" in caplog.text


def test_string_no_slash__long_string_allowed() -> None:
    # string_no_slash doesn't enforce length - use cv.Length() separately
    long_value = "x" * 200
    assert config_validation.string_no_slash(long_value) == long_value


def test_string_no_slash__empty() -> None:
    assert config_validation.string_no_slash("") == ""


@pytest.mark.parametrize("value", ("Temperature", "Living Room Light", "温度传感器"))
def test_validate_entity_name__valid(value: str) -> None:
    actual = config_validation._validate_entity_name(value)
    assert actual == value


def test_validate_entity_name__slash_replaced_with_warning(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test that '/' in entity names is auto-replaced with fraction slash."""
    actual = config_validation._validate_entity_name("has/slash")
    assert actual == "has⁄slash"
    assert "reserved as a URL path separator" in caplog.text


def test_validate_entity_name__max_length() -> None:
    # 120 bytes should pass
    assert config_validation._validate_entity_name("x" * 120) == "x" * 120

    # 121 bytes should fail
    with pytest.raises(Invalid, match="too long.*121 bytes.*Maximum.*120"):
        config_validation._validate_entity_name("x" * 121)


def test_validate_entity_name__multibyte_byte_length() -> None:
    # 40 chars of 3-byte UTF-8 = 120 bytes, should pass
    assert config_validation._validate_entity_name("温" * 40) == "温" * 40

    # 41 chars of 3-byte UTF-8 = 123 bytes, should fail (over 120 byte limit)
    with pytest.raises(Invalid, match="too long.*123 bytes.*Maximum.*120"):
        config_validation._validate_entity_name("温" * 41)


def test_validate_entity_name__none_without_friendly_name() -> None:
    # When name is "None" and friendly_name is not set, it should fail
    CORE.friendly_name = None
    with pytest.raises(Invalid, match="friendly_name is not set"):
        config_validation._validate_entity_name("None")


def test_validate_entity_name__none_with_friendly_name() -> None:
    # When name is "None" but friendly_name is set, it should return None
    CORE.friendly_name = "My Device"
    result = config_validation._validate_entity_name("None")
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
    assert config_validation.percentage(value) == expected


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
        config_validation.percentage(value)


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
    assert config_validation.possibly_negative_percentage(value) == expected


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
        config_validation.possibly_negative_percentage(value)


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
    assert config_validation.unbounded_percentage(value) == expected


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
        config_validation.unbounded_percentage(value)


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
    assert config_validation.unbounded_possibly_negative_percentage(value) == expected


@pytest.mark.parametrize("value", ("foo", None))
def test_unbounded_possibly_negative_percentage__invalid(value: object) -> None:
    with pytest.raises(Invalid):
        config_validation.unbounded_possibly_negative_percentage(value)


@pytest.mark.parametrize(
    "value",
    (50, -50, 2, -2),
)
def test_percentage_validators__raw_number_above_one_without_percent_sign(
    value: object,
) -> None:
    """Raw numeric values outside [-1, 1] must use a percent sign."""
    with pytest.raises(Invalid, match="percent sign"):
        config_validation.unbounded_percentage(value)
    with pytest.raises(Invalid, match="percent sign"):
        config_validation.unbounded_possibly_negative_percentage(value)


def test_update_interval__coerces_zero_to_one_ms(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """update_interval: 0ms must be coerced to 1ms (not rejected) because a
    literal 0ms schedule causes Scheduler::call() to spin. Coercion keeps
    existing configs compiling on upgrade while emitting a user-facing
    warning that directs them to set a non-zero value."""
    with caplog.at_level("WARNING"):
        result = config_validation.update_interval("0ms")
    assert result.total_milliseconds == 1
    assert "update_interval of 0ms is not supported" in caplog.text
    assert "1ms" in caplog.text


def test_update_interval__preserves_nonzero_values() -> None:
    """Non-zero update_interval values must pass through unchanged."""
    assert config_validation.update_interval("1ms").total_milliseconds == 1
    assert config_validation.update_interval("50ms").total_milliseconds == 50
    assert config_validation.update_interval("60s").total_milliseconds == 60000


def test_update_interval__never_passes_through() -> None:
    """update_interval: never must still map to SCHEDULER_DONT_RUN."""
    result = config_validation.update_interval("never")
    assert result.total_milliseconds == SCHEDULER_DONT_RUN


# ---------------------------------------------------------------------------
# Visibility UI-hint kwarg
# ---------------------------------------------------------------------------


def test_optional_default_visibility_is_none() -> None:
    """An ``Optional`` with no ``visibility`` kwarg reports ``None``.

    Consumers can read the attribute directly with plain attribute
    access; absence (``None``) means "render on the editor's main
    form."
    """
    o = config_validation.Optional("foo")
    assert o.visibility is None


def test_optional_visibility_advanced() -> None:
    """``visibility=Visibility.ADVANCED`` is recorded on the marker."""
    o = config_validation.Optional(
        "foo", visibility=config_validation.Visibility.ADVANCED
    )
    assert o.visibility is config_validation.Visibility.ADVANCED


def test_optional_visibility_yaml_only() -> None:
    """``visibility=Visibility.YAML_ONLY`` is recorded on the marker."""
    o = config_validation.Optional(
        "foo", visibility=config_validation.Visibility.YAML_ONLY
    )
    assert o.visibility is config_validation.Visibility.YAML_ONLY


def test_visibility_str_values_match_dump_emission() -> None:
    """``Visibility`` is a ``StrEnum`` whose values are the literal
    strings the schema dumper emits.

    The schema bundle consumers (catalog generators, third-party
    schema-aware tooling) shouldn't need an enum import to read the
    field — pinning the on-the-wire spelling here keeps the dump
    contract stable.
    """
    assert str(config_validation.Visibility.ADVANCED) == "advanced"
    assert str(config_validation.Visibility.YAML_ONLY) == "yaml_only"


def test_optional_visibility_does_not_affect_validation() -> None:
    """The kwarg is an advisory UI hint — it must not change how the
    validator behaves. A schema with ``visibility`` applied must
    accept and reject the same values it would without it.
    """
    plain = config_validation.Schema(
        {config_validation.Optional("foo", default=42): config_validation.int_}
    )
    flagged = config_validation.Schema(
        {
            config_validation.Optional(
                "foo",
                default=42,
                visibility=config_validation.Visibility.YAML_ONLY,
            ): config_validation.int_
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
    r = config_validation.Required("foo")
    assert r.visibility is None


def test_required_visibility_kwarg() -> None:
    """``Required`` accepts ``visibility`` for symmetry with ``Optional``.

    Required fields rarely need the kwarg, but exposing it lets
    consumers apply uniform logic across key markers.
    """
    r = config_validation.Required(
        "foo", visibility=config_validation.Visibility.ADVANCED
    )
    assert r.visibility is config_validation.Visibility.ADVANCED


def test_polling_component_schema_visibility_opt_in() -> None:
    """``visibility=`` propagates to the inherited ``update_interval``.

    Time platforms pass ``Visibility.ADVANCED``; sensors and other
    polling components leave it ``None`` and keep the un-flagged shape.
    """
    default = config_validation.polling_component_schema("15min")
    advanced = config_validation.polling_component_schema(
        "15min", visibility=config_validation.Visibility.ADVANCED
    )
    default_keys = {str(k): k for k in default.schema}
    advanced_keys = {str(k): k for k in advanced.schema}
    assert default_keys["update_interval"].visibility is None
    assert (
        advanced_keys["update_interval"].visibility
        is config_validation.Visibility.ADVANCED
    )
    # The opt-in only touches update_interval — setup_priority
    # still inherits its YAML_ONLY visibility from COMPONENT_SCHEMA
    # in both shapes.
    assert (
        default_keys["setup_priority"].visibility
        is config_validation.Visibility.YAML_ONLY
    )
    assert (
        advanced_keys["setup_priority"].visibility
        is config_validation.Visibility.YAML_ONLY
    )


def test_polling_component_schema_no_default_ignores_visibility() -> None:
    """``visibility`` is silently ignored when the field is Required.

    When ``default_update_interval=None`` the field becomes
    ``Required``. Hiding a Required field behind an advanced
    disclosure is a UX hazard — a collapsed-by-default editor could
    let the user submit without noticing the form has an unfilled
    required field. The helper accepts the kwarg unconditionally
    for caller ergonomics but doesn't honour it on this branch.
    """
    schema = config_validation.polling_component_schema(
        None, visibility=config_validation.Visibility.ADVANCED
    )
    keys = {str(k): k for k in schema.schema}
    assert isinstance(keys["update_interval"], config_validation.Required)
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
    inner_unset = config_validation.Optional("baz")
    inner_yaml_only = config_validation.Optional(
        "qux", visibility=config_validation.Visibility.YAML_ONLY
    )
    parent = config_validation.Optional(
        "foo", visibility=config_validation.Visibility.ADVANCED
    )

    # Wire them into a nested schema — none of the markers' own
    # ``visibility`` should change as a result.
    schema = config_validation.Schema(
        {
            parent: config_validation.Schema(
                {
                    inner_unset: config_validation.int_,
                    inner_yaml_only: config_validation.string,
                }
            )
        }
    )
    assert schema  # touch the schema so any deferred mutation runs

    assert parent.visibility is config_validation.Visibility.ADVANCED
    assert inner_unset.visibility is None
    assert inner_yaml_only.visibility is config_validation.Visibility.YAML_ONLY
