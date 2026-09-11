"""Shared base for the infrared and radio_frequency entity components."""

import esphome.codegen as cg
from esphome.core import CORE
from esphome.core.entity_helpers import queue_entity_register
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81", "@bdraco"]
AUTO_LOAD = ["remote_base"]

ir_rf_base_ns = cg.esphome_ns.namespace("ir_rf_base")
IrRfEntity = ir_rf_base_ns.class_("IrRfEntity", cg.EntityBase, cg.Component)


async def register_ir_rf_entity(
    var: cg.MockObj, config: ConfigType, domain: str
) -> None:
    """Register an infrared or radio_frequency entity; USE_IR_RF covers both kinds."""
    cg.add_define("USE_IR_RF")
    await cg.register_component(var, config)
    queue_entity_register(domain, config)
    CORE.register_platform_component(domain, var)
