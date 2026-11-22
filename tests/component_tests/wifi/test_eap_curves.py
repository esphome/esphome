"""A real wifi eap: config keeps the rare TLS curves that private RADIUS PKIs
use, which the TLS curve trim otherwise turns off on ESP-IDF 6."""

from collections.abc import Callable
from pathlib import Path

from esphome.components.esp32 import MBEDTLS_RARE_TLS_CURVE_OPTIONS
from tests.component_tests.network import sdkconfig_option


def test_eap_keeps_rare_curves(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("eap_esp32.yaml"))

    assert sdkconfig_option("CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT") is True
    for option in MBEDTLS_RARE_TLS_CURVE_OPTIONS:
        assert sdkconfig_option(option) is not False, option
