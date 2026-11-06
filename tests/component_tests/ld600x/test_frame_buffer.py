"""The hub gets a static frame buffer sized for its point cloud entities, unless PSRAM is used."""

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


POINT_CLOUD_BUFFER = "static uint8_t ld600x__radar_buffer__pstorage[ld600x::DEFAULT_MAX_DATA_LEN_POINT_CLOUD];"


def test_point_cloud_size() -> None:
    hub_id = ID("radar", type=LD600XComponent)
    for platforms in (
        {"sensor": [{HUB_KEY: hub_id, "point_count": {}}]},
        {"switch": [{HUB_KEY: hub_id, "point_cloud": {}}]},
    ):
        CORE.reset()
        assert _buffers(**platforms) == [POINT_CLOUD_BUFFER]


def test_psram_keeps_the_heap_buffer() -> None:
    CORE.loaded_integrations.add("psram")
    assert _buffers() == []
