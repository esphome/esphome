"""Tests for the shared HTTP server's socket accounting."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components import web_server_base
from esphome.components.socket import (
    KEY_SOCKET_CONSUMERS_TCP,
    KEY_SOCKET_CONSUMERS_TCP_LISTEN,
    KEY_SOCKET_CONSUMERS_UDP,
)
from esphome.core import CORE
from tests.component_tests.helpers import get_define_value

CONFIG_DIR = Path(__file__).parent / "config"
MAX_OPEN_DEFINE = "WEB_SERVER_MAX_OPEN_SOCKETS"


def _consumers(key: str) -> dict[str, int]:
    return CORE.data.get(key, {})


def test_web_server_keeps_idf_default(
    generate_main: Callable[[str | Path], str],
) -> None:
    """web_server alone registers 5 clients; the limit stays at the ESP-IDF default."""
    generate_main(CONFIG_DIR / "web_server.yaml")

    assert _consumers(KEY_SOCKET_CONSUMERS_TCP)["web_server"] == 5
    assert _consumers(KEY_SOCKET_CONSUMERS_TCP_LISTEN)["web_server_base"] == 1
    assert _consumers(KEY_SOCKET_CONSUMERS_UDP)["web_server_base"] == 2
    assert web_server_base.get_httpd_client_count() == 5
    assert get_define_value(MAX_OPEN_DEFINE) == "7"


def test_web_server_with_captive_portal(
    generate_main: Callable[[str | Path], str],
) -> None:
    """The captive portal probes count as 3 more clients of the same server."""
    generate_main(CONFIG_DIR / "web_server_captive_portal.yaml")

    tcp = _consumers(KEY_SOCKET_CONSUMERS_TCP)
    assert tcp["web_server"] == 5
    assert tcp["captive_portal"] == 3
    udp = _consumers(KEY_SOCKET_CONSUMERS_UDP)
    assert udp["web_server_base"] == 2
    assert udp["captive_portal"] == 1
    assert web_server_base.get_httpd_client_count() == 8
    assert get_define_value(MAX_OPEN_DEFINE) == "8"


def test_extra_consumer_raises_limit(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A consumer holding long-lived clients adds to the socket registry and the limit."""
    web_server_base.add_httpd_clients(4, "stream")
    generate_main(CONFIG_DIR / "web_server.yaml")

    tcp = _consumers(KEY_SOCKET_CONSUMERS_TCP)
    assert tcp["stream"] == 4
    assert tcp["web_server"] == 5
    assert web_server_base.get_httpd_client_count() == 9
    assert get_define_value(MAX_OPEN_DEFINE) == "9"


def test_prometheus_only_uses_default(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A server with no registered clients keeps the ESP-IDF default."""
    generate_main(CONFIG_DIR / "prometheus.yaml")

    assert web_server_base.get_httpd_client_count() == 0
    assert _consumers(KEY_SOCKET_CONSUMERS_UDP)["web_server_base"] == 2
    assert get_define_value(MAX_OPEN_DEFINE) == "7"


def test_user_lwip_limit_caps_clients(
    generate_main: Callable[[str | Path], str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """httpd_start() rejects more than CONFIG_LWIP_MAX_SOCKETS - 3 clients."""
    generate_main(CONFIG_DIR / "lwip_override.yaml")

    assert web_server_base.get_httpd_client_count() == 8
    assert get_define_value(MAX_OPEN_DEFINE) == "6"
    assert "leaves room for only 6 HTTP clients" in caplog.text


def test_esp8266_counts_tcp_only(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESPAsyncWebServer has no httpd control sockets or client limit define."""
    generate_main(CONFIG_DIR / "web_server_esp8266.yaml")

    assert _consumers(KEY_SOCKET_CONSUMERS_TCP)["web_server"] == 5
    assert _consumers(KEY_SOCKET_CONSUMERS_TCP_LISTEN)["web_server_base"] == 1
    assert "web_server_base" not in _consumers(KEY_SOCKET_CONSUMERS_UDP)
    assert get_define_value(MAX_OPEN_DEFINE) is None
