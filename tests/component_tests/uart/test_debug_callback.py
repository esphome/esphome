"""The uart debug sequence runs from a generated function the debugger calls."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome import config_validation as cv
from esphome.components.uart import DEBUG_SCHEMA
from esphome.const import PlatformFramework
from tests.component_tests.helpers import callback_function, constructed, function_body
from tests.component_tests.types import SetCoreConfigCallable


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    return generate_main(component_config_path("debug_callback.yaml"))


@pytest.mark.parametrize(
    ("var", "uart", "settings"),
    [
        (
            "uart_uartdebugger_id",
            "uart_a",
            [
                "set_direction(uart::UART_DIRECTION_RX)",
                "set_after_bytes(20)",
                "set_after_timeout(50)",
                'set_debug_prefix("[A] ")',
            ],
        ),
        (
            "named_debugger",
            "uart_b",
            [
                "set_direction(uart::UART_DIRECTION_BOTH)",
                "set_after_bytes(150)",
                "set_after_timeout(100)",
            ],
        ),
    ],
)
def test_debugger_constructed(
    main_cpp: str, var: str, uart: str, settings: list[str]
) -> None:
    """Each debugger is built on its sequence's function and gets its settings."""
    function = callback_function(main_cpp, var)
    assert constructed(main_cpp, var) == (
        f"FireCallback<{function}, uart::UARTDebugger>({uart})"
    )
    for setting in settings:
        assert f"{var}->{setting};" in main_cpp


def test_inline_sequence_runs_in_function(main_cpp: str) -> None:
    """A sequence of inline actions runs in the function, with the trigger's argument names."""
    body = function_body(main_cpp, callback_function(main_cpp, "uart_uartdebugger_id"))
    assert "UARTDebug::log_string(direction, bytes, debug_prefix);" in body
    assert "}(direction, bytes, debug_prefix);" in body
    assert "->trigger(" not in body


def test_delayed_sequence_triggers_automation(main_cpp: str) -> None:
    """A delay keeps the Automation, which the function triggers with the arguments."""
    body = function_body(main_cpp, callback_function(main_cpp, "named_debugger"))
    assert re.fullmatch(
        r"\n  ::automation_id\w*->trigger\(direction, bytes, debug_prefix\);", body
    )


def test_sequence_rejects_several_automations(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """One debugger runs one generated function, so sequence: takes a single automation."""
    set_core_config(PlatformFramework.ESP32_IDF)
    block = {"then": [{"delay": "10ms"}]}
    DEBUG_SCHEMA({"sequence": [block]})
    with pytest.raises(cv.Invalid, match="only a single automation"):
        DEBUG_SCHEMA({"sequence": [block, block]})
