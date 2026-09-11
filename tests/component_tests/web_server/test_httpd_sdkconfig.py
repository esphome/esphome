"""Tests for ESP-IDF web server sdkconfig defaults."""

from collections.abc import Callable

from esphome.components.esp32 import RawSdkconfigValue
from esphome.components.esp32.const import KEY_ESP32, KEY_SDKCONFIG_OPTIONS
from esphome.core import CORE


def test_web_server_idf_uses_default_header_limit(
    generate_main: Callable[[str], str],
) -> None:
    """The web server retains its existing 1024-byte default."""
    generate_main(
        "tests/component_tests/web_server/web_server_header_limit_default.yaml"
    )

    assert (
        CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]["CONFIG_HTTPD_MAX_REQ_HDR_LEN"]
        == 1024
    )


def test_web_server_idf_respects_explicit_header_limit(
    generate_main: Callable[[str], str],
) -> None:
    """An explicit sdkconfig option takes precedence over the component default."""
    generate_main(
        "tests/component_tests/web_server/web_server_header_limit_override.yaml"
    )

    value = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]["CONFIG_HTTPD_MAX_REQ_HDR_LEN"]
    assert isinstance(value, RawSdkconfigValue)
    assert value.value == "4096"
