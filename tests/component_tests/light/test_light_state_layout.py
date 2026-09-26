"""Limits and flash layout that let LightState stay small."""

import pytest

from esphome import config_validation as cv
from esphome.components.light.effects import (
    MAX_EFFECTS,
    MONOCHROMATIC_EFFECTS,
    validate_effects,
)


def _effects(count: int) -> list[dict[str, dict[str, str]]]:
    return [{"pulse": {"name": f"Pulse {i}"}} for i in range(count)]


def test_rejects_more_effects_than_the_index_holds() -> None:
    with pytest.raises(cv.Invalid, match=f"at most {MAX_EFFECTS} effects"):
        validate_effects(MONOCHROMATIC_EFFECTS)(_effects(MAX_EFFECTS + 1))


def test_accepts_a_normal_effect_list() -> None:
    assert len(validate_effects(MONOCHROMATIC_EFFECTS)(_effects(3))) == 3


def test_gamma_table_initializer_holds_the_lut_then_gamma_times_100() -> None:
    from esphome.components.light import gamma_table_initializer, generate_gamma_table

    init = gamma_table_initializer(2.8)
    lut = ", ".join(f"0x{int(v):04X}" for v in generate_gamma_table(2.8))
    assert init == f"{{{{{lut}}}, 280}}"


@pytest.mark.parametrize("gamma", [0.0, 2.8, 655.0])
def test_gamma_correct_accepts_values_that_fit(gamma: float) -> None:
    from esphome.components.light import validate_gamma_correct

    assert validate_gamma_correct(gamma) == gamma


def test_gamma_correct_rejects_values_the_table_cannot_hold() -> None:
    from esphome.components.light import validate_gamma_correct

    with pytest.raises(cv.Invalid):
        validate_gamma_correct(655.5)
