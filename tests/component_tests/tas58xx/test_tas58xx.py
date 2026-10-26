"""Tests that the TAS58xx fault_output and modulation defaults keep the existing behaviour."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path


def test_tas58xx_defaults_keep_the_fault_pin_and_bd_modulation(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Without fault_output or modulation each model keeps its usual fault pin and BD modulation."""
    main_cpp = generate_main(
        "tests/component_tests/tas58xx/test_tas58xx_fault_output_modulation.yaml"
    )

    assert "amp_5805_default->set_fault_output(0);" in main_cpp
    assert "amp_5825_default->set_fault_output(1);" in main_cpp
    for amp in ("amp_5805_default", "amp_5825_default", "amp_5805_none"):
        assert f"{amp}->set_modulation(tas58xx::MODULATION_BD_MODE);" in main_cpp


def test_tas58xx_fault_output_none_leaves_the_pin_alone(
    generate_main: Callable[[str | Path], str],
) -> None:
    """fault_output: none does not configure a fault pin."""
    main_cpp = generate_main(
        "tests/component_tests/tas58xx/test_tas58xx_fault_output_modulation.yaml"
    )

    assert "amp_5805_none->set_fault_output(" not in main_cpp
