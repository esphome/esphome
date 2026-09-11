"""Shared base for the infrared and radio_frequency entity components."""

import esphome.codegen as cg
from esphome.components.remote_base import CONF_TRANSMITTER_ID
from esphome.core import CORE
from esphome.core.entity_helpers import queue_entity_register
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81", "@bdraco"]
AUTO_LOAD = ["remote_base"]

ir_rf_base_ns = cg.esphome_ns.namespace("ir_rf_base")
IrRfEntity = ir_rf_base_ns.class_("IrRfEntity", cg.EntityBase, cg.Component)

# Each entity with a transmitter listens for that transmitter's completions when the API
# can reply to a transmit request; the transmitter's listener list is sized from this count
_request_complete_slot = cg.slot_counter("REMOTE_BASE_COMPLETE_LISTENER_COUNT")


async def attach_transmitter(
    var: MockObj, config: ConfigType, key: str = CONF_TRANSMITTER_ID
) -> None:
    """Link the configured transmitter to an entity.

    A transmitter set from C++ has no completion listener slot, so API transmit
    requests on that entity are answered by the 30 s timeout instead.
    """
    transmitter = await cg.get_variable(config[key])
    cg.add(var.set_transmitter(transmitter))
    if "api" in CORE.config:
        _request_complete_slot()


async def register_ir_rf_entity(
    var: cg.MockObj, config: ConfigType, domain: str
) -> None:
    """Register an infrared or radio_frequency entity; USE_IR_RF covers both kinds."""
    cg.add_define("USE_IR_RF")
    await cg.register_component(var, config)
    queue_entity_register(domain, config)
    CORE.register_platform_component(domain, var)
