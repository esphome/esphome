"""The hub gets a static frame buffer sized for its point cloud entities, unless PSRAM is used."""

import pytest

from esphome.components.ld600x import LD600XComponent, add_frame_buffer
from esphome.const import CONF_ID
from esphome.core import CORE, ID
from esphome.cpp_generator import MockObj

HUB_KEY = "ld600x_test_id"


def _buffers(**platforms: list[dict]) -> list[str]:
    hub_id = ID("radar", is_declaration=True, type=LD600XComponent)
    CORE.config = platforms
    add_frame_buffer(MockObj("radar"), {CONF_ID: hub_id}, HUB_KEY)
    return [str(s) for s in CORE.global_statements if "_buffer__pstorage" in str(s)]


def test_default_size() -> None:
    other_hub = ID("other", type=LD600XComponent)
    assert _buffers(sensor=[{HUB_KEY: other_hub, "point_count": {}}]) == [
        "static uint8_t ld600x__radar_buffer__pstorage[ld600x::DEFAULT_MAX_DATA_LEN];"
    ]


@pytest.mark.parametrize(
    ("domain", "key"), [("sensor", "point_count"), ("switch", "point_cloud")]
)
def test_point_cloud_size(domain: str, key: str) -> None:
    hub_id = ID("radar", type=LD600XComponent)
    assert _buffers(**{domain: [{HUB_KEY: hub_id, key: {}}]}) == [
        "static uint8_t ld600x__radar_buffer__pstorage[ld600x::DEFAULT_MAX_DATA_LEN_POINT_CLOUD];"
    ]


def test_psram_allocates_at_boot() -> None:
    assert _buffers(psram={"disabled": False}) == []
    assert (
        "radar.set_frame_buffer(psram_buffer<uint8_t>(ld600x::DEFAULT_MAX_DATA_LEN), "
        "ld600x::DEFAULT_MAX_DATA_LEN);"
    ) in [str(s) for s in CORE.main_statements]
