"""Tests for UART parity code generation."""

from collections.abc import Callable
from pathlib import Path


def test_default_parity_is_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Parity NONE is the C++ initializer, so the setter is skipped."""
    main_cpp = generate_main(component_config_path("parity_default.yaml"))

    assert "default_uart->set_parity(" not in main_cpp


def test_custom_parity_is_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A non default parity still reaches the setter."""
    main_cpp = generate_main(component_config_path("parity_even.yaml"))

    assert "even_uart->set_parity(uart::UART_CONFIG_PARITY_EVEN);" in main_cpp
