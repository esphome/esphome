import esphome.codegen as cg
from esphome.components import tuya
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

AUTO_LOAD = ["tuya"]

ns = cg.esphome_ns.namespace("fake_tuya_transport")
FakeTuyaTransport = ns.class_("FakeTuyaTransport", tuya.Tuya)

CONFIG_SCHEMA = tuya.TUYA_SCHEMA.extend(
    {cv.GenerateID(): cv.declare_id(FakeTuyaTransport)}
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await tuya.setup_tuya(var, config)
