"""Tests for HTTPS on the ESP32 web server."""

from collections.abc import Callable, Generator
from pathlib import Path
import sys

import pytest

from esphome import loader
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32, idf_version
from esphome.components.esp32.const import (
    KEY_ESP32,
    KEY_EXCLUDE_COMPONENTS,
    KEY_SDKCONFIG_OPTIONS,
)
from esphome.components.web_server_base import get_httpd_client_count
from esphome.components.web_server_idf import HTTPS_MAX_CONNECTIONS, consume_ssl_sockets
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from esphome.core import CORE
from tests.component_tests.helpers import get_define_value
from tests.component_tests.types import SetCoreConfigCallable

ESP32_PLATFORM_DATA = {KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32}


@pytest.fixture(autouse=True)
def unload_https_test() -> Generator[None]:
    """Drop the test-only external component so later tests start clean."""
    yield
    loader._COMPONENT_CACHE.pop("https_test", None)
    sys.modules.pop("esphome.components.https_test", None)
    loader.clear_component_meta_finders()


def test_consume_ssl_sockets_sets_up_https(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A component with consume_ssl_sockets() in its schema compiles HTTPS in, with the server side of TLS."""
    generate_main(component_config_path("https.yaml"))

    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert "USE_WEBSERVER_SSL" in {define.name for define in CORE.defines}
    assert get_define_value("WEB_SERVER_MAX_OPEN_SOCKETS") == str(HTTPS_MAX_CONNECTIONS)
    assert get_httpd_client_count() == HTTPS_MAX_CONNECTIONS
    assert sdkconfig["CONFIG_ESP_HTTPS_SERVER_ENABLE"]
    assert "esp_https_server" not in CORE.data[KEY_ESP32][KEY_EXCLUDE_COMPONENTS]
    assert not sdkconfig.get("CONFIG_MBEDTLS_TLS_CLIENT_ONLY")


def test_no_https_without_consume_ssl_sockets(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without consume_ssl_sockets() nothing HTTPS is compiled in."""
    generate_main(component_config_path("web_server.yaml"))

    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert "USE_WEBSERVER_SSL" not in {define.name for define in CORE.defines}
    assert "CONFIG_ESP_HTTPS_SERVER_ENABLE" not in sdkconfig
    assert "esp_https_server" in CORE.data[KEY_ESP32][KEY_EXCLUDE_COMPONENTS]


def test_https_sockets_reserved_once(set_core_config: SetCoreConfigCallable) -> None:
    """The listener is shared, so a second component using HTTPS reserves no more connections."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    consume_ssl_sockets({})
    consume_ssl_sockets({})

    assert get_httpd_client_count() == HTTPS_MAX_CONNECTIONS


def test_https_sockets_not_reserved_off_esp32(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """HTTPS exists only on ESP32, so other platforms reserve no connections for it."""
    set_core_config(PlatformFramework.RP2040_ARDUINO)
    consume_ssl_sockets({})

    assert get_httpd_client_count() == 0


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
    assert f"can't be used together with {component} yet" in capsys.readouterr().out


def test_ssl_needs_idf_5_5(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    """HTTPS is rejected before ESP-IDF 5.5, which added the TLS handshake timeout."""
    CORE.config_path = component_config_path("https_idf_5_4.yaml")

    assert read_config({}) is None
    assert "HTTPS needs ESP-IDF 5.5 or later" in capsys.readouterr().out


def test_ssl_on_arduino_checks_its_idf_version(
    component_config_path: Callable[[str], Path],
) -> None:
    """On Arduino the check uses the ESP-IDF the core is built on, not the Arduino version."""
    CORE.config_path = component_config_path("https_arduino.yaml")

    assert read_config({}) is not None
    assert idf_version() >= cv.Version(5, 5, 0)
