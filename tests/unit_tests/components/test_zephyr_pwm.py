"""Unit tests for esphome.components.zephyr_pwm overlays."""

from __future__ import annotations

from esphome.components.zephyr.const import KEY_ZEPHYR
from esphome.components.zephyr_pwm.output import PWMBlock, _overlay_pwm_renesas
from esphome.const import KEY_CORE, KEY_TARGET_PLATFORM, PLATFORM_ZEPHYR
from esphome.core import CORE


def test_renesas_overlay_uses_gtioc_function_for_any_gpt_channel() -> None:
    """RA_PSEL_GPT1 selects the GTIOC pin function on every GPT channel."""
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    CORE.data[KEY_ZEPHYR] = {"variant": "RA4M1", "family": "renesas"}
    overlay = _overlay_pwm_renesas(
        [PWMBlock(id=0, period_ns=1_000_000, pins=[69, 70])], ["pwm3"]
    )
    assert "RA_PSEL(RA_PSEL_GPT1, 4, 5)" in overlay
    assert "RA_PSEL_GPT3" not in overlay
