"""Tests for the homeassistant switch platform."""

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE

DIR = Path(__file__).parent


def test_switch_alone_enables_actions(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A switch with no other homeassistant platform still enables the actions it sends."""
    main_cpp = generate_main(DIR / "switch_valid.yaml")

    assert 'ha_switch->set_entity_id("switch.porch");' in main_cpp
    assert any(d.name == "USE_API_HOMEASSISTANT_SERVICES" for d in CORE.defines)
