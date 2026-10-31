"""Repeated size requests for one hub keep the largest area kind count."""

from esphome.components.ld600x import request_model_sizes
from esphome.core import CORE
from esphome.cpp_generator import MockObj


def _defines() -> dict[str, str]:
    return {define.name: str(define.value) for define in CORE.defines}


def test_requests_keep_the_largest_count() -> None:
    hub_a = MockObj("hub_a")
    hub_b = MockObj("hub_b")
    request_model_sizes(hub_a, area_kinds=2)
    request_model_sizes(hub_b, area_kinds=2)
    request_model_sizes(hub_a, area_kinds=3)
    request_model_sizes(hub_a, area_kinds=3)
    request_model_sizes(hub_a, area_kinds=2)
    CORE.flush_tasks()
    defines = _defines()
    assert defines["LD600X_AREA_KINDS"] == "3"
    assert defines["LD600X_MAX_TARGETS"] == "3"


def test_two_area_kinds_by_default() -> None:
    request_model_sizes(MockObj("hub"), area_kinds=2)
    CORE.flush_tasks()
    assert _defines()["LD600X_AREA_KINDS"] == "2"
