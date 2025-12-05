import string

from hypothesis import example, given
from hypothesis.strategies import builds, integers, ip_addresses, one_of, text
import pytest
import voluptuous as vol

from esphome import config_validation
from esphome.components.esp32.const import (
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
    from esphome.components.esp32.const import KEY_ESP32
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
