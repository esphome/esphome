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


def test_process_stacktrace_esp8266_crash_handler(
    setup_core: Path, mock_esp8266_decode_pc: Mock
) -> None:
    """Test process_stacktrace handles ESP8266 crash handler backtrace lines."""
    from esphome.components.esp8266 import process_stacktrace

    config = {"name": "test"}

    # Simulate crash handler log lines as they appear from the API/serial
    line_pc = "[E][esp8266:191]:   PC: 0x40220060"
    state = process_stacktrace(config, line_pc, False)
    mock_esp8266_decode_pc.assert_called_once_with(config, "40220060")
    assert state is False

    mock_esp8266_decode_pc.reset_mock()

    # Near-null data address (wild pointer) is not a code address, must be ignored
    line_excvaddr = "[E][esp8266:193]:   EXCVADDR: 0x0000008A"
    state = process_stacktrace(config, line_excvaddr, False)
    mock_esp8266_decode_pc.assert_not_called()
    assert state is False

    mock_esp8266_decode_pc.reset_mock()

    line_bt0 = "[E][esp8266:196]:   BT0: 0x40212345"
    state = process_stacktrace(config, line_bt0, False)
    mock_esp8266_decode_pc.assert_called_once_with(config, "40212345")
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

    mock_esp32_decode_pc.reset_mock()

    # Reason line carries no address, must not trigger a decode
    line_reason = "[E][esp32.crash:079]:   Reason: Fault - LoadProhibited (cause 28)"
    state = process_stacktrace(config, line_reason, False)
    mock_esp32_decode_pc.assert_not_called()
    assert state is False

    mock_esp32_decode_pc.reset_mock()

    # EXCVADDR pointing at code (e.g. jumping through a corrupted pointer) decodes
    line_excvaddr = "[E][esp32.crash:081]:   EXCVADDR: 0x400D9ABC  (faulting address)"
    state = process_stacktrace(config, line_excvaddr, False)
    mock_esp32_decode_pc.assert_called_once_with(config, "400D9ABC")
    assert state is False

    mock_esp32_decode_pc.reset_mock()

    # EXCVADDR pointing at data (heap/null) is not a code address, must be ignored
    line_excvaddr_data = (
        "[E][esp32.crash:081]:   EXCVADDR: 0x0000001C  (faulting address)"
    )
    state = process_stacktrace(config, line_excvaddr_data, False)
    mock_esp32_decode_pc.assert_not_called()
    assert state is False

    mock_esp32_decode_pc.reset_mock()

    # RISC-V MTVAL pointing at code decodes
    line_mtval = "[E][esp32.crash:081]:   MTVAL: 0x42001234  (faulting address)"
    state = process_stacktrace(config, line_mtval, False)
    mock_esp32_decode_pc.assert_called_once_with(config, "42001234")
    assert state is False

    mock_esp32_decode_pc.reset_mock()

    # RISC-V MTVAL pointing at data must be ignored
    line_mtval_data = "[E][esp32.crash:081]:   MTVAL: 0x3FC80123  (faulting address)"
    state = process_stacktrace(config, line_mtval_data, False)
    mock_esp32_decode_pc.assert_not_called()
    assert state is False


def test_process_stacktrace_esp32_foreign_crash(
    setup_core: Path, mock_esp32_decode_pc: Mock
) -> None:
    """Crash records from a different firmware build must not be decoded."""
    from esphome.components.esp32 import process_stacktrace

    config = {"name": "test"}

    line_note = (
        "[E][esp32.crash:390]:   Captured by a different firmware build; "
        "addresses belong to that build's ELF"
    )
    state = process_stacktrace(config, line_note, False)
    mock_esp32_decode_pc.assert_not_called()
    assert state is False

    # Lowercase labels are deliberately not matched by any decoder regex,
    # since symbols would come from the wrong ELF
    lines_addrs = [
        "[E][esp32.crash:391]:   pc: 0x400D1234",
        "[E][esp32.crash:392]:   excvaddr: 0x400D5678",
        "[E][esp32.crash:392]:   mtval: 0x42001234",
        "[E][esp32.crash:393]:   bt0: 0x400F19A6",
        "[E][esp32.crash:394]:   other core (0):",
        "[E][esp32.crash:395]:   bt15: 0x42005ABC",
    ]
    for line in lines_addrs:
        state = process_stacktrace(config, line, False)
        mock_esp32_decode_pc.assert_not_called()
        assert state is False
