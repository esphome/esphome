"""Shared base for the Hi-Link 60 GHz radars that speak the TinyFrame serial protocol.

Covers the LD6002B and the LD6004. The LD6001A is a different family (AT commands and
another report format) and gets its own component, so it does not build on this one.
"""

import esphome.codegen as cg
from esphome.components import uart
from esphome.cpp_generator import MockObj

CODEOWNERS = ["@hepter", "@wolph"]
DEPENDENCIES = ["uart"]

ld600x_ns = cg.esphome_ns.namespace("ld600x")
LD600XComponent = ld600x_ns.class_("LD600XComponent", cg.Component, uart.UARTDevice)

_request_target_slot = cg.slot_counter("LD600X_MAX_TARGETS")
_request_area_kind_slot = cg.slot_counter("LD600X_AREA_KINDS")


def request_model_sizes(hub: MockObj, *, max_targets: int, area_kinds: int) -> None:
    for _ in range(max_targets):
        _request_target_slot(str(hub))
    for _ in range(area_kinds):
        _request_area_kind_slot(str(hub))
