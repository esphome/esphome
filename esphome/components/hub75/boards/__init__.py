"""Board presets for HUB75 displays.

Each board preset defines standard pin mappings for HUB75 controller boards.
"""

from dataclasses import dataclass, field
import importlib
import pkgutil
from typing import ClassVar


class BoardRegistry:
    """Global registry for board configurations."""

    _boards: ClassVar[dict[str, "BoardConfig"]] = {}

    @classmethod
    def register(cls, board: "BoardConfig") -> None:
        """Register a board configuration."""
        cls._boards[board.name] = board

    @classmethod
    def get_boards(cls) -> dict[str, "BoardConfig"]:
        """Return all registered boards."""
        return cls._boards


@dataclass
class BoardConfig:
    """Board configuration storing HUB75 pin mappings."""

    name: str
    r1_pin: int
    g1_pin: int
    b1_pin: int
    r2_pin: int
    g2_pin: int
    b2_pin: int
    a_pin: int
    b_pin: int
    c_pin: int
    d_pin: int
    e_pin: int | None
    lat_pin: int
    oe_pin: int
    clk_pin: int
    ignore_strapping_pins: tuple[str, ...] = ()  # e.g., ("a_pin", "clk_pin")

    # Derived field for pin lookup
    pins: dict[str, int | None] = field(default_factory=dict, init=False, repr=False)

    def __post_init__(self):
        """Initialize derived fields and register board."""
        self.name = self.name.lower()
        self.pins = {
            "r1": self.r1_pin,
            "g1": self.g1_pin,
            "b1": self.b1_pin,
            "r2": self.r2_pin,
            "g2": self.g2_pin,
            "b2": self.b2_pin,
            "a": self.a_pin,
            "b": self.b_pin,
            "c": self.c_pin,
            "d": self.d_pin,
            "e": self.e_pin,
            "lat": self.lat_pin,
            "oe": self.oe_pin,
            "clk": self.clk_pin,
        }
        BoardRegistry.register(self)

    def get_pin(self, pin_name: str) -> int | None:
        """Get pin number for a given pin name."""
        return self.pins.get(pin_name)


# Dynamically import all board definition modules
for module_info in pkgutil.iter_modules(__path__):
    importlib.import_module(f".{module_info.name}", package=__package__)
