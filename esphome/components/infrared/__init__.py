"""
Infrared component for ESPHome.

WARNING: This component is EXPERIMENTAL. The API (both Python configuration
and C++ interfaces) may change at any time without following the normal
breaking changes policy. Use at your own risk.

Once the API is considered stable, this warning will be removed.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE, coroutine_with_priority
from esphome.core.entity_helpers import setup_entity
from esphome.coroutine import CoroPriority
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
AUTO_LOAD = ["remote_base"]

IS_PLATFORM_COMPONENT = True

infrared_ns = cg.esphome_ns.namespace("infrared")
Infrared = infrared_ns.class_("Infrared", cg.EntityBase, cg.Component)
InfraredCall = infrared_ns.class_("InfraredCall")
InfraredTraits = infrared_ns.class_("InfraredTraits")

CONF_INFRARED_ID = "infrared_id"
CONF_SUPPORTS_TRANSMITTER = "supports_transmitter"
CONF_SUPPORTS_RECEIVER = "supports_receiver"


def infrared_schema(class_: type[cg.MockObjClass]) -> cv.Schema:
    """Create a schema for an infrared platform.

    :param class_: The infrared class to use for this schema.
    :return: An extended schema for infrared configuration.
    """
    entity_schema = cv.ENTITY_BASE_SCHEMA.extend(cv.COMPONENT_SCHEMA)
    return entity_schema.extend(
        {
            cv.GenerateID(): cv.declare_id(class_),
        }
    )


async def setup_infrared_core_(var: cg.Pvariable, config: ConfigType) -> None:
    """Set up core infrared configuration."""
    await setup_entity(var, config, "infrared")


async def register_infrared(var: cg.Pvariable, config: ConfigType) -> None:
    """Register an infrared device with the core."""
    cg.add_define("USE_IR_RF")
    await cg.register_component(var, config)
    await setup_infrared_core_(var, config)
    cg.add(cg.App.register_infrared(var))
    CORE.register_platform_component("infrared", var)


async def new_infrared(config: ConfigType, *args) -> cg.Pvariable:
    """Create a new Infrared instance.

    :param config: Configuration dictionary.
    :param args: Additional arguments to pass to new_Pvariable.
    :return: The created Infrared instance.
    """
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_infrared(var, config)
    return var


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_global(infrared_ns.using)
