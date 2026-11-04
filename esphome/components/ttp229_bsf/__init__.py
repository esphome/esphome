from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SCL_PIN, CONF_SDO_PIN
from esphome.types import ConfigType

AUTO_LOAD = ["binary_sensor"]
DOMAIN = "ttp229_bsf"

CONF_TTP229_ID = "ttp229_id"
ttp229_bsf_ns = cg.esphome_ns.namespace("ttp229_bsf")

TTP229BSFComponent = ttp229_bsf_ns.class_("TTP229BSFComponent", cg.Component)

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TTP229BSFComponent),
        cv.Required(CONF_SDO_PIN): pins.gpio_input_pullup_pin_schema,
        cv.Required(CONF_SCL_PIN): pins.gpio_output_pin_schema,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await cg.add_pin(var.set_sdo_pin, config[CONF_SDO_PIN])
    await cg.add_pin(var.set_scl_pin, config[CONF_SCL_PIN])
