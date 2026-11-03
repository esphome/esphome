"""Tests for HTTPS on the ESP32 web server."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.esp32.const import (
    KEY_ESP32,
    KEY_EXCLUDE_COMPONENTS,
    KEY_SDKCONFIG_OPTIONS,
)
from esphome.config import read_config
from esphome.core import CORE


def test_ssl_sets_up_https(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The ssl block compiles HTTPS in and passes the port and credentials on."""
    main_cpp = generate_main(component_config_path("ssl.yaml"))

    assert "set_port(443)" in main_cpp
    assert "set_ssl_credentials(" in main_cpp
    assert "USE_WEBSERVER_SSL" in {define.name for define in CORE.defines}
    assert CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]["CONFIG_ESP_HTTPS_SERVER_ENABLE"]
    assert "esp_https_server" not in CORE.data[KEY_ESP32][KEY_EXCLUDE_COMPONENTS]


def test_ssl_rejects_certificate_as_key(
    component_config_path: Callable[[str], Path], capsys: pytest.CaptureFixture[str]
) -> None:
    """A certificate pasted into key: fails at build time, not on the device."""
    CORE.config_path = component_config_path("ssl_certificate_as_key.yaml")

    assert read_config({}) is None
    assert "PRIVATE KEY-----" in capsys.readouterr().out


def test_ssl_rejects_encrypted_key(
    component_config_path: Callable[[str], Path], capsys: pytest.CaptureFixture[str]
) -> None:
    """The server has no password for the key, so an encrypted key fails at build time."""
    CORE.config_path = component_config_path("ssl_encrypted_key.yaml")

    assert read_config({}) is None
    assert "Encrypted private keys are not supported" in capsys.readouterr().out


@pytest.mark.parametrize(
    ("config_file", "component"),
    [
        ("ssl_with_web_server.yaml", "web_server"),
        ("ssl_with_captive_portal.yaml", "captive_portal"),
        ("ssl_with_prometheus.yaml", "prometheus"),
    ],
)
def test_ssl_rejects_incompatible_components(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
    config_file: str,
    component: str,
) -> None:
    """The ssl option can't be combined with components that need plain HTTP."""
    CORE.config_path = component_config_path(config_file)

    assert read_config({}) is None
    assert f"can't be used together with '{component}' yet" in capsys.readouterr().out
