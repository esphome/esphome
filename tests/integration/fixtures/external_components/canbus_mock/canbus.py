"""Host-only canbus platform that loops sent frames back as received frames."""

import esphome.codegen as cg
from esphome.components import canbus
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

canbus_mock_ns = cg.esphome_ns.namespace("canbus_mock")
MockCanbus = canbus_mock_ns.class_("MockCanbus", canbus.CanbusComponent)

CONFIG_SCHEMA = canbus.CANBUS_SCHEMA.extend(
    {cv.GenerateID(): cv.declare_id(MockCanbus)}
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await canbus.register_canbus(var, config, reports_status=False)
