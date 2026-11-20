"""WPA2 Enterprise build options."""

from collections.abc import Callable
import logging
from pathlib import Path

import pytest

from esphome.components.esp32.const import KEY_ESP32, KEY_SDKCONFIG_OPTIONS
from esphome.core import CORE

HOSTED_EAP_OPTION = "CONFIG_ESP_HOSTED_HOST_FEAT_WIFI_EXT_ENT"


@pytest.mark.parametrize(
    ("config", "hosted"),
    [("eap_esp32.yaml", False), ("eap_hosted.yaml", True)],
)
def test_eap_host_feature_follows_esp32_hosted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
    config: str,
    hosted: bool,
) -> None:
    """Only a hosted build turns on the host feature that sends the EAP calls."""
    with caplog.at_level(logging.WARNING):
        generate_main(component_config_path(config))

    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig["CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT"] is True
    assert (sdkconfig.get(HOSTED_EAP_OPTION) is True) is hosted
    assert ("coprocessor firmware built with WPA2 Enterprise" in caplog.text) is hosted
