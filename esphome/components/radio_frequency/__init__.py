"""
Radio Frequency component for ESPHome.

WARNING: This component is EXPERIMENTAL. The API (both Python configuration
and C++ interfaces) may change at any time without following the normal
breaking changes policy. Use at your own risk.

Once the API is considered stable, this warning will be removed.
"""

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_CONTROL
from esphome.core import CORE, coroutine_with_priority
from esphome.core.entity_helpers import queue_entity_register, setup_entity
from esphome.coroutine import CoroPriority
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
AUTO_LOAD = ["remote_base"]

IS_PLATFORM_COMPONENT = True

radio_frequency_ns = cg.esphome_ns.namespace("radio_frequency")
RadioFrequency = radio_frequency_ns.class_(
    "RadioFrequency", cg.EntityBase, cg.Component
)
RadioFrequencyCall = radio_frequency_ns.class_("RadioFrequencyCall")
RadioFrequencyTraits = radio_frequency_ns.class_("RadioFrequencyTraits")
RadioFrequencyModulation = radio_frequency_ns.enum("RadioFrequencyModulation")

CONF_RADIO_FREQUENCY_ID = "radio_frequency_id"


def radio_frequency_schema(class_: type[cg.MockObjClass]) -> cv.Schema:
    """Create a schema for a radio frequency platform.

    :param class_: The radio frequency class to use for this schema.
    :return: An extended schema for radio frequency configuration.
    """
    entity_schema = cv.ENTITY_BASE_SCHEMA.extend(cv.COMPONENT_SCHEMA)
    return entity_schema.extend(
        {
            cv.GenerateID(): cv.declare_id(class_),
            cv.Optional(CONF_ON_CONTROL): automation.validate_automation({}),
        }
    )


@setup_entity("radio_frequency")
async def setup_radio_frequency_core_(var: cg.Pvariable, config: ConfigType) -> None:
    """Set up core radio frequency configuration."""


async def register_radio_frequency(var: cg.Pvariable, config: ConfigType) -> None:
    """Register a radio frequency device with the core."""
    cg.add_define("USE_RADIO_FREQUENCY")
    await cg.register_component(var, config)
    queue_entity_register("radio_frequency", config)
    await setup_radio_frequency_core_(var, config)
    CORE.register_platform_component("radio_frequency", var)

    for conf in config.get(CONF_ON_CONTROL, []):
        await automation.build_callback_automation(
            var, "add_on_control_callback", [(RadioFrequencyCall, "x")], conf
        )


async def new_radio_frequency(config: ConfigType, *args) -> cg.Pvariable:
    """Create a new RadioFrequency instance.

    :param config: Configuration dictionary.
    :param args: Additional arguments to pass to new_Pvariable.
    :return: The created RadioFrequency instance.
    """
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_radio_frequency(var, config)
    return var


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_global(radio_frequency_ns.using)
