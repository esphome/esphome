"""Tests for ESP32 component."""

from pathlib import Path
from unittest.mock import Mock


def test_process_stacktrace_esp8266_exception(setup_core: Path, caplog) -> None:
    """Test process_stacktrace handles ESP8266 exceptions."""
    from esphome.components.esp8266 import process_stacktrace

    config = {"name": "test"}

    # Test exception type parsing
    line = "Exception (28):"
    backtrace_state = False

    result = process_stacktrace(config, line, backtrace_state)

    assert "Access to invalid address: LOAD (wild pointer?)" in caplog.text
    assert result is False


def test_process_stacktrace_esp8266_backtrace(
    setup_core: Path, mock_esp8266_decode_pc: Mock
) -> None:
    """Test process_stacktrace handles ESP8266 multi-line backtrace."""
    from esphome.components.esp8266 import process_stacktrace

    config = {"name": "test"}

    # Start of backtrace
    line1 = ">>>stack>>>"
    state = process_stacktrace(config, line1, False)
    assert state is True

    # Backtrace content with addresses
    line2 = "40201234 40205678"
    state = process_stacktrace(config, line2, state)
    assert state is True
    assert mock_esp8266_decode_pc.call_count == 2

    # End of backtrace
    line3 = "<<<stack<<<"
    state = process_stacktrace(config, line3, state)
    assert state is False


def test_process_stacktrace_esp32_backtrace(
    setup_core: Path, mock_esp32_decode_pc: Mock
) -> None:
    """Test process_stacktrace handles ESP32 single-line backtrace."""
    from esphome.components.esp32 import process_stacktrace

    config = {"name": "test"}

    line = "Backtrace: 0x40081234:0x3ffb1234 0x40085678:0x3ffb5678"
    state = process_stacktrace(config, line, False)

    # Should decode both addresses
    assert mock_esp32_decode_pc.call_count == 2
    mock_esp32_decode_pc.assert_any_call(config, "40081234")
    mock_esp32_decode_pc.assert_any_call(config, "40085678")
    assert state is False


def test_process_stacktrace_bad_alloc(
    setup_core: Path, mock_esp32_decode_pc: Mock, caplog
) -> None:
    """Test process_stacktrace handles bad alloc messages."""
    from esphome.components.esp32 import process_stacktrace

    config = {"name": "test"}

    line = "last failed alloc call: 40201234(512)"
    state = process_stacktrace(config, line, False)

    assert "Memory allocation of 512 bytes failed at 40201234" in caplog.text
    mock_esp32_decode_pc.assert_called_once_with(config, "40201234")
    assert state is False


def test_process_stacktrace_esp32_crash_handler(
    setup_core: Path, mock_esp32_decode_pc: Mock
) -> None:
    """Test process_stacktrace handles ESP32 crash handler backtrace lines."""
    from esphome.components.esp32 import process_stacktrace

    config = {"name": "test"}

    # Simulate crash handler log lines as they appear from the API/serial
    line_pc = "[E][esp32.crash:078]:   PC:  0x400D1234  (fault location)"
    state = process_stacktrace(config, line_pc, False)
    # PC line is matched by existing STACKTRACE_ESP32_PC_RE
    mock_esp32_decode_pc.assert_called_with(config, "400D1234")
    assert state is False

    mock_esp32_decode_pc.reset_mock()

    line_bt0 = "[E][esp32.crash:080]:   BT0: 0x400D5678  (backtrace)"
    state = process_stacktrace(config, line_bt0, False)
    mock_esp32_decode_pc.assert_called_once_with(config, "400D5678")
    assert state is False

    mock_esp32_decode_pc.reset_mock()

    line_bt1 = "[E][esp32.crash:080]:   BT1: 0x42005ABC  (backtrace)"
    state = process_stacktrace(config, line_bt1, False)
    mock_esp32_decode_pc.assert_called_once_with(config, "42005ABC")
    assert state is False
