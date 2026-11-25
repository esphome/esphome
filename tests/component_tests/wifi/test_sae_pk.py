"""enable_sae_pk turns WPA3 SAE-PK support on; ESP-IDF's default of on is
otherwise overridden, since few access points use it."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import RawSdkconfigValue
from esphome.components.wifi import CONFIG_SCHEMA
from esphome.const import PlatformFramework
from tests.component_tests.network import sdkconfig_option
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.parametrize(
    ("config_file", "expected"),
    [
        pytest.param("esp32_idf.yaml", False, id="default_off"),
        pytest.param("sae_pk.yaml", True, id="enabled"),
        pytest.param(
            "sae_pk_sdkconfig.yaml", RawSdkconfigValue("y"), id="user_sdkconfig_wins"
        ),
    ],
)
def test_sae_pk_sdkconfig(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    expected: bool | RawSdkconfigValue,
) -> None:
    generate_main(component_config_path(config_file))

    assert sdkconfig_option("CONFIG_ESP_WIFI_ENABLE_SAE_PK") == expected


def test_sae_pk_esp32_only(set_core_config: SetCoreConfigCallable) -> None:
    """SAE-PK is an ESP-IDF supplicant feature, so other platforms reject it."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)

    with pytest.raises(cv.Invalid, match="ESP32"):
        CONFIG_SCHEMA({"ssid": "test", "password": "testtest", "enable_sae_pk": True})
