"""Optional MCP251XFD interrupt configuration."""

import pytest

from esphome.components.mcp251xfd import canbus as mcp251xfd
import esphome.config_validation as cv


@pytest.mark.parametrize("pin", [4, {"number": 4, "mode": "INPUT_PULLUP"}])
def test_interrupt_pin(pin: int | dict[str, object]) -> None:
    """Accept an optional internal input pin."""
    config = mcp251xfd.CONFIG_SCHEMA({"can_id": 1, "cs_pin": 21, "interrupt_pin": pin})
    assert config["interrupt_pin"]["number"] == 4
    assert config["interrupt_pin"]["mode"]["input"]
    assert "interrupt_pin" not in mcp251xfd.CONFIG_SCHEMA({"can_id": 1, "cs_pin": 21})


@pytest.mark.parametrize(
    ("options", "message"),
    [
        ({"inverted": True}, "MCP251xFD INT is active-low"),
        ({"allow_other_uses": True}, "MCP251xFD INT does not support sharing"),
    ],
)
def test_invalid_interrupt_pin(options: dict[str, object], message: str) -> None:
    """The active-low interrupt disallows inversion and pin sharing."""
    with pytest.raises(cv.Invalid, match=message):
        mcp251xfd.CONFIG_SCHEMA(
            {"can_id": 1, "cs_pin": 21, "interrupt_pin": {"number": 4, **options}}
        )
