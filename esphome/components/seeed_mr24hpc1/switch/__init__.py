import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SWITCH, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import CONF_MR24HPC1_ID, MR24HPC1Component, mr24hpc1_ns

UnderlyingOpenFuncSwitch = mr24hpc1_ns.class_(
    "UnderlyOpenFunctionSwitch", switch.Switch
)

CONF_UNDERLYING_OPEN_FUNCTION = "underlying_open_function"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MR24HPC1_ID): cv.use_id(MR24HPC1Component),
    cv.Optional(CONF_UNDERLYING_OPEN_FUNCTION): switch.switch_schema(
        UnderlyingOpenFuncSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:electric-switch",
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MR24HPC1_ID])
    switches = switch.sub_switches(config, parent=hub)
    await switches(
        CONF_UNDERLYING_OPEN_FUNCTION, hub.set_underlying_open_function_switch
    )
