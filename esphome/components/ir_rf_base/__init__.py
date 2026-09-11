"""Shared base for the infrared and radio_frequency entity components."""

import esphome.codegen as cg
from esphome.components import remote_base
from esphome.const import CONF_API
from esphome.core import CORE
from esphome.core.entity_helpers import queue_entity_register
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81", "@bdraco"]
AUTO_LOAD = ["remote_base"]

ir_rf_base_ns = cg.esphome_ns.namespace("ir_rf_base")
IrRfEntity = ir_rf_base_ns.class_("IrRfEntity", cg.EntityBase, cg.Component)


async def attach_transmitter(var: cg.MockObj, config: ConfigType, key: str) -> None:
    """Link the configured transmitter to an entity.

    With the API configured this also compiles in the transmit completion
    tracking that answers API transmit requests. An entity whose transmitter
    is set from C++ instead is answered as soon as the frame is handed over.
    """
    await remote_base.register_transmittable(var, config, key)
    if CONF_API in CORE.config:
        cg.add_define("USE_IR_RF_TRANSMIT_COMPLETE")


async def register_ir_rf_entity(
    var: cg.MockObj, config: ConfigType, domain: str
) -> None:
    """Register an infrared or radio_frequency entity; USE_IR_RF covers both kinds."""
    cg.add_define("USE_IR_RF")
    await cg.register_component(var, config)
    queue_entity_register(domain, config)
    CORE.register_platform_component(domain, var)
