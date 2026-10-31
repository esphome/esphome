"""Tests for the ESP-IDF httpd server's socket accounting."""

from collections.abc import Callable
import logging
from pathlib import Path

import pytest

from esphome.components import web_server_base, web_server_idf
from esphome.components.socket import KEY_SOCKET_CONSUMERS_UDP
from esphome.core import CORE
from tests.component_tests.helpers import get_define_value

MAX_OPEN_DEFINE = "WEB_SERVER_MAX_OPEN_SOCKETS"


@pytest.mark.parametrize(
    "config_file",
    [
        # web_server alone registers 5 clients, below the ESP-IDF default.
        "web_server.yaml",
        # The captive portal probes are plain TCP sockets, not httpd slots.
        "web_server_captive_portal.yaml",
    ],
)
def test_limit_stays_at_idf_default(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    """Configs with fewer clients than the ESP-IDF default keep the default."""
    generate_main(component_config_path(config_file))

    assert CORE.data[KEY_SOCKET_CONSUMERS_UDP]["web_server_idf"] == 2
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


def test_tiny_user_lwip_limit_keeps_define_valid(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A pool too small for any client still yields a limit of at least 1.

    httpd_start() still fails at runtime with such a pool; esp32 warns about it.
    """
    monkeypatch.setattr(web_server_idf, "get_user_lwip_max_sockets", lambda: 3)
    web_server_base.add_httpd_clients(5, "web_server")

    assert web_server_idf._httpd_max_open_sockets() == 1


def test_esp8266_has_no_httpd_sockets(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """ESPAsyncWebServer has no httpd control sockets or client limit define."""
    generate_main(component_config_path("web_server_esp8266.yaml"))

    assert "web_server_idf" not in CORE.data.get(KEY_SOCKET_CONSUMERS_UDP, {})
    assert get_define_value(MAX_OPEN_DEFINE) is None
