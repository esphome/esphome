"""LightState stores the active effect index in a uint16_t, so the effect count is capped."""

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
