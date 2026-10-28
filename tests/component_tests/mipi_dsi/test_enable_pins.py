"""Tests for applying the default enable pins of mipi_dsi models."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome.components.mipi import DriverChip


def test_default_enable_pins_by_address(
    generate_main: Callable[[str | Path], str],
    component_fixture_path: Callable[[str], Path],
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Default enable pins naming a hub by address resolve to that hub, and only
    the pin used elsewhere is left out, not the same pin number on the other hub."""
    from esphome.components.mipi_dsi.display import MODELS

    models = set(DriverChip.models)
    model = MODELS["M5STACK-TAB5"].extend(
        "TEST-ENABLE-BY-ADDRESS",
        enable_pin=[
            {"pi4ioe5v6408": {"address": 0x43}, "number": 1},
            {"pi4ioe5v6408": {"address": 0x44}, "number": 1},
        ],
    )
    for name in set(DriverChip.models) - models:
        monkeypatch.delitem(DriverChip.models, name)
    monkeypatch.setitem(MODELS, model.name, model)

    main_cpp = generate_main(component_fixture_path("enable_pins_address.yaml"))

    match = re.search(r"default_pins->set_enable_pins\(\{(.*)\}\);", main_cpp)
    assert match is not None
    pin_ids = match.group(1).split(", ")
    assert len(pin_ids) == 1
    assert f"{pin_ids[0]}->set_parent(pi4ioe1);" in main_cpp
    assert f"{pin_ids[0]}->set_pin(1);" in main_cpp
