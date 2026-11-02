"""Tests for the GPIO switch component."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import re


def test_gpio_switch_interlock_group_shares_one_table(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Every member of an interlock group points at the same flash table."""
    main_cpp = generate_main(
        "tests/component_tests/gpio/test_gpio_switch_interlock.yaml"
    )

    tables = re.findall(r"(gpio_interlock\w*)\[\] PROGMEM = \{([^}]*)\};", main_cpp)
    assert tables == [("gpio_interlock", "sw_a, sw_b, sw_c")]
    for sw in ("sw_a", "sw_b", "sw_c"):
        assert f"{sw}->set_interlock(gpio_interlock, 3);" in main_cpp


def test_gpio_switch_interlock_with_only_itself_has_no_table(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A switch interlocked only with itself gets no table and no set_interlock."""
    main_cpp = generate_main(
        "tests/component_tests/gpio/test_gpio_switch_interlock.yaml"
    )

    assert "sw_self->set_interlock(" not in main_cpp
    assert "sw_self->set_interlock_wait_time(0);" in main_cpp
