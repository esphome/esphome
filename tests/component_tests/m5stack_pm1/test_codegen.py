"""Code generation tests for the M5Stack PM1 hub."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path


def test_pin_options_reach_the_hub(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """fast_mode defaults to off, and the pins' interrupt and wake bits are folded into the hub's registers."""
    main_cpp = generate_main(component_config_path("pins.yaml"))

    assert "new(pm1) m5stack_pm1::M5StackPM1Component(false)" in main_cpp
    # GPIO0 interrupt enabled, the IRQ output GPIO4 and the other pins masked.
    assert "pm1->set_interrupt_gpio(4, 30);" in main_cpp
    # GPIO2 and GPIO3 wake, GPIO2 on a rising edge.
    assert "pm1->set_wake_config(12, 4);" in main_cpp
