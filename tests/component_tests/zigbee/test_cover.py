"""Exercise the ESP32 cover bridge through real validation and code generation."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest
import yaml

Config = dict


@pytest.fixture
def fixture_config(component_fixture_path: Callable[[str], Path]) -> Config:
    return yaml.safe_load(component_fixture_path("esp32_cover.yaml").read_text())


def covering_types(cpp: str) -> list[int]:
    return [
        int(x)
        for x in re.findall(
            r"new\(\w+\) zigbee::ZigbeeCover\(\w+, \d+, \w+, (\d+)\)", cpp
        )
    ]


def _build(config: Config, tmp_path: Path, generate_main: Callable[[Path], str]) -> str:
    path = tmp_path / "test.yaml"
    path.write_text(yaml.safe_dump(config, sort_keys=False))
    return generate_main(path)


@pytest.mark.parametrize(
    ("device_class", "covering_type"),
    [
        pytest.param(None, 0x00, id="none-rollershade"),
        pytest.param("awning", 0x05, id="awning"),
        pytest.param("curtain", 0x04, id="curtain-drapery"),
        pytest.param("shutter", 0x00, id="shutter-lift-not-tilt-only"),
    ],
)
def test_window_covering_type_from_device_class(
    fixture_config: Config,
    tmp_path: Path,
    generate_main: Callable[[Path], str],
    device_class: str | None,
    covering_type: int,
) -> None:
    source = fixture_config["cover"][0]
    source.pop("device_class")
    if device_class is not None:
        source["device_class"] = device_class
    cpp = _build(fixture_config, tmp_path, generate_main)
    # The copy inherits the class after the endpoint was registered
    assert covering_types(cpp) == [covering_type, covering_type]


def test_copy_keeps_its_own_device_class(
    fixture_config: Config, tmp_path: Path, generate_main: Callable[[Path], str]
) -> None:
    fixture_config["cover"][1]["device_class"] = "awning"
    cpp = _build(fixture_config, tmp_path, generate_main)
    assert covering_types(cpp) == [0x04, 0x05]
