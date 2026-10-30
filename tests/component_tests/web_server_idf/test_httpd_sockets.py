"""Tests for the ESP-IDF httpd server's socket accounting."""

from collections.abc import Callable
import logging
from pathlib import Path

import pytest

from esphome.components import web_server_base
from esphome.components.socket import KEY_SOCKET_CONSUMERS_UDP
from esphome.core import CORE
from tests.component_tests.helpers import get_define_value

MAX_OPEN_DEFINE = "WEB_SERVER_MAX_OPEN_SOCKETS"


def test_web_server_keeps_idf_default(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """web_server alone registers 5 clients; the limit stays at the ESP-IDF default."""
    generate_main(component_config_path("web_server.yaml"))

    assert CORE.data[KEY_SOCKET_CONSUMERS_UDP]["web_server_idf"] == 2
    assert get_define_value(MAX_OPEN_DEFINE) == "7"


def test_captive_portal_probes_are_not_httpd_clients(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The captive portal probes are plain TCP sockets, not httpd slots."""
    generate_main(component_config_path("web_server_captive_portal.yaml"))

    assert get_define_value(MAX_OPEN_DEFINE) == "7"


def test_extra_consumer_raises_limit(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A consumer holding long-lived clients raises the limit."""
    web_server_base.add_httpd_clients(4, "stream")
    generate_main(component_config_path("web_server.yaml"))

    assert get_define_value(MAX_OPEN_DEFINE) == "9"


def test_user_lwip_limit_caps_clients(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """httpd_start() rejects more than CONFIG_LWIP_MAX_SOCKETS - 3 clients."""
    caplog.set_level(logging.INFO)
    generate_main(component_config_path("lwip_override.yaml"))

    assert get_define_value(MAX_OPEN_DEFINE) == "6"
    # One warning for the undersized pool, from esp32 only.
    warnings = [r for r in caplog.records if r.levelno == logging.WARNING]
    assert len(warnings) == 1
    assert "CONFIG_LWIP_MAX_SOCKETS is set to 9" in warnings[0].message
    assert "limits the web server to 6 clients" in caplog.text


def test_esp8266_has_no_httpd_sockets(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """ESPAsyncWebServer has no httpd control sockets or client limit define."""
    generate_main(component_config_path("web_server_esp8266.yaml"))

    assert "web_server_idf" not in CORE.data.get(KEY_SOCKET_CONSUMERS_UDP, {})
    assert get_define_value(MAX_OPEN_DEFINE) is None
