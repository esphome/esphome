"""Tests for esphome.components.api.client."""

from __future__ import annotations

from unittest.mock import patch

from esphome.components.api import client as api_client
from esphome.core import EsphomeError


def test_process_stacktrace_line_swallows_esphome_error() -> None:
    """A failing stack-trace decode must not propagate.

    on_log runs inside an asyncio protocol callback; if EsphomeError
    escapes, the loop reports "Fatal error: protocol.data_received()
    call failed.", tears the connection down, and ReconnectLogic loops
    forever as the device replays the same crash trace on every
    reconnect.
    """
    config = {"esphome": {"name": "test"}}

    with patch.object(
        api_client, "process_stacktrace", side_effect=EsphomeError("no idedata")
    ) as mock_process:
        result = api_client._process_stacktrace_line(
            config, "PC: 0x4010496e", True, None
        )

    assert mock_process.called
    assert result is False


def test_process_stacktrace_line_swallows_platform_handler_error() -> None:
    """The same protection must apply to the platform-specific handler."""
    config = {"esphome": {"name": "test"}}

    def platform_handler(_config, _line, _state):
        raise EsphomeError("no idedata")

    result = api_client._process_stacktrace_line(
        config, "PC: 0x4010496e", True, platform_handler
    )

    assert result is False


def test_process_stacktrace_line_returns_handler_result() -> None:
    """When decoding succeeds, the handler's result is returned unchanged."""
    config = {"esphome": {"name": "test"}}

    with patch.object(
        api_client, "process_stacktrace", return_value=True
    ) as mock_process:
        result = api_client._process_stacktrace_line(
            config, "PC: 0x4010496e", False, None
        )

    mock_process.assert_called_once_with(
        config, "PC: 0x4010496e", backtrace_state=False
    )
    assert result is True


def test_process_stacktrace_line_uses_platform_handler_when_provided() -> None:
    """The platform handler is preferred over the generic one."""
    config = {"esphome": {"name": "test"}}
    calls: list[tuple[object, str, bool]] = []

    def platform_handler(cfg, line, state):
        calls.append((cfg, line, state))
        return True

    with patch.object(api_client, "process_stacktrace") as mock_generic:
        result = api_client._process_stacktrace_line(
            config, "BT0: 0x4010496e", False, platform_handler
        )

    assert calls == [(config, "BT0: 0x4010496e", False)]
    assert mock_generic.called is False
    assert result is True
