"""Model parameters control the shared area schemas and option ordering."""

import pytest

from esphome.components.ld600x import LD600XComponent
from esphome.components.ld600x.entities import area_id_options, sensor_schema
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.parametrize("with_dwell", [False, True])
def test_area_id_options(with_dwell: bool) -> None:
    kinds: tuple[str, ...] = ("interference", "detection")
    expected: list[str] = [
        "interference_area_0",
        "interference_area_1",
        "interference_area_2",
        "interference_area_3",
        "detection_area_0",
        "detection_area_1",
        "detection_area_2",
        "detection_area_3",
    ]
    if with_dwell:
        kinds += ("dwell",)
        expected += ["dwell_area_0", "dwell_area_1", "dwell_area_2", "dwell_area_3"]
    assert area_id_options(kinds) == expected


@pytest.mark.parametrize("with_dwell", [False, True])
def test_sensor_area_kinds(
    set_core_config: SetCoreConfigCallable, with_dwell: bool
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    kinds: tuple[str, ...] = ("interference", "detection")
    if with_dwell:
        kinds += ("dwell",)
    schema: cv.Schema = sensor_schema(
        LD600XComponent, "hub_id", max_targets=3, area_kinds=kinds
    )
    assert "interference_area_0" in schema({"interference_area_0": {}})
    if with_dwell:
        assert "dwell_area_0" in schema({"dwell_area_0": {}})
    else:
        with pytest.raises(cv.Invalid, match="dwell_area_0"):
            schema({"dwell_area_0": {}})
