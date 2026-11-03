"""Tests for HTTPS on the ESP32 web server."""

from collections.abc import Callable, Generator
from pathlib import Path
import sys

import pytest

from esphome import loader
from esphome.components.esp32.const import (
    KEY_ESP32,
    KEY_EXCLUDE_COMPONENTS,
    KEY_SDKCONFIG_OPTIONS,
)
from esphome.config import read_config
from esphome.core import CORE


@pytest.fixture(autouse=True)
def unload_https_test() -> Generator[None]:
    """Drop the test-only external component so later tests start clean."""
    yield
    loader._COMPONENT_CACHE.pop("https_test", None)
    sys.modules.pop("esphome.components.https_test", None)
    loader.clear_component_meta_finders()


def test_enable_ssl_sets_up_https(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A component that calls enable_ssl() compiles HTTPS in."""
    generate_main(component_config_path("https.yaml"))

    assert "USE_WEBSERVER_SSL" in {define.name for define in CORE.defines}
    assert CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]["CONFIG_ESP_HTTPS_SERVER_ENABLE"]
    assert "esp_https_server" not in CORE.data[KEY_ESP32][KEY_EXCLUDE_COMPONENTS]


@pytest.mark.parametrize(
    ("config_file", "component"),
    [
        ("https_with_web_server.yaml", "web_server"),
        ("https_with_captive_portal.yaml", "captive_portal"),
        ("https_with_prometheus.yaml", "prometheus"),
    ],
)
def test_ssl_rejects_incompatible_components(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
    config_file: str,
    component: str,
) -> None:
    """HTTPS can't be combined with components that need plain HTTP."""
    CORE.config_path = component_config_path(config_file)

    assert read_config({}) is None
    assert f"can't be used together with '{component}' yet" in capsys.readouterr().out
