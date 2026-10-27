"""Tests for when web OTA's handler waits in WebServerBase for the server to be created."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

_PENDING = "USE_WEBSERVER_BASE_PENDING_HANDLER"


@pytest.mark.parametrize(
    ("config_file", "pending"),
    [
        # captive_portal loads web OTA, and with it the web_server package, but only
        # creates the server when the AP starts
        ("pending_handler_captive_portal.yaml", True),
        ("pending_handler_web_server.yaml", False),
        ("pending_handler_prometheus.yaml", False),
    ],
)
def test_pending_handler_define(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    pending: bool,
) -> None:
    """Only configs where nothing creates the server before web OTA's setup() keep the pointer."""
    generate_main(component_config_path(config_file))
    defines = {define.name for define in CORE.defines}

    assert "USE_WEBSERVER_OTA" in defines
    assert (_PENDING in defines) is pending
