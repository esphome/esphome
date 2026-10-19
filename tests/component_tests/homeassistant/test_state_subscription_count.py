"""Tests for Home Assistant state subscription count calculation and pre-reservation."""

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE

DIR = Path(__file__).parent


def test_mixed_entities_state_subscription_count(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A configuration with mixed entities computes the exact weighted subscription count."""
    generate_main(DIR / "mixed_entities.yaml")

    # sensor (2*1) + binary_sensor (1) + text_sensor (1) + switch (1)
    # + select (2) + number (4) + text (4) + button (0) + light (8) = 23
    define_match = [
        d for d in CORE.defines if d.name == "HOMEASSISTANT_STATE_SUBSCRIPTION_COUNT"
    ]
    assert len(define_match) == 1
    assert str(define_match[0].value) == "23"
    assert any(d.name == "USE_API_HOMEASSISTANT_STATES" for d in CORE.defines)


def test_button_only_no_state_subscription_count(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A configuration with only button entities has no state subscriptions."""
    generate_main(DIR / "button_valid.yaml")

    assert not any(
        d.name == "HOMEASSISTANT_STATE_SUBSCRIPTION_COUNT" for d in CORE.defines
    )
    assert any(d.name == "USE_API_HOMEASSISTANT_STATES" for d in CORE.defines)
    assert any(d.name == "USE_API_HOMEASSISTANT_SERVICES" for d in CORE.defines)


def test_select_only_state_subscription_count(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A configuration with 2 select entities calculates 2 * 2 = 4 subscriptions."""
    generate_main(DIR / "select_valid.yaml")

    define_match = [
        d for d in CORE.defines if d.name == "HOMEASSISTANT_STATE_SUBSCRIPTION_COUNT"
    ]
    assert len(define_match) == 1
    assert str(define_match[0].value) == "4"
    assert any(d.name == "USE_API_HOMEASSISTANT_STATES" for d in CORE.defines)
