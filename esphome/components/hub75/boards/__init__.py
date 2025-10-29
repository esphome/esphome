"""Board presets for HUB75 displays.

Each board preset defines standard pin mappings for HUB75 controller boards.
"""

import importlib
import pkgutil


class BoardConfig:
    """Board configuration storing HUB75 pin mappings.

    Boards are automatically registered in the class-level boards dict when instantiated.
    """

    boards: dict[str, "BoardConfig"] = {}

    def __init__(
        self,
        name: str,
        r1_pin: int,
        g1_pin: int,
        b1_pin: int,
        r2_pin: int,
        g2_pin: int,
        b2_pin: int,
        a_pin: int,
        b_pin: int,
        c_pin: int,
        d_pin: int,
        e_pin: int | None,
        lat_pin: int,
        oe_pin: int,
        clk_pin: int,
        # Optional pin modifiers
        a_pin_ignore_strapping: bool = False,
    ):
        """Create a board configuration.

        Args:
            name: Board identifier (e.g., "apollo-automation-m1-rev4")
            r1_pin through clk_pin: HUB75 pin numbers
            a_pin_ignore_strapping: If True, ignore strapping warnings on A pin
        """
        self.name = name.lower()
        self.pins = {
            "r1": r1_pin,
            "g1": g1_pin,
            "b1": b1_pin,
            "r2": r2_pin,
            "g2": g2_pin,
            "b2": b2_pin,
            "a": a_pin,
            "b": b_pin,
            "c": c_pin,
            "d": d_pin,
            "e": e_pin,
            "lat": lat_pin,
            "oe": oe_pin,
            "clk": clk_pin,
        }
        self.a_pin_ignore_strapping = a_pin_ignore_strapping

        # Auto-register this board
        BoardConfig.boards[self.name] = self

    @classmethod
    def get_boards(cls) -> dict[str, "BoardConfig"]:
        """Return all registered boards."""
        return cls.boards

    def get_pin(self, pin_name: str) -> int | None:
        """Get pin number for a given pin name."""
        return self.pins.get(pin_name)


# Dynamically import all board definition modules
for module_info in pkgutil.iter_modules(__path__):
    importlib.import_module(f".{module_info.name}", package=__package__)
