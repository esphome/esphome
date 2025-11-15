from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SCL_PIN, CONF_SDO_PIN

AUTO_LOAD = ["binary_sensor"]

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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    sdo = await cg.gpio_pin_expression(config[CONF_SDO_PIN])
    cg.add(var.set_sdo_pin(sdo))
    scl = await cg.gpio_pin_expression(config[CONF_SCL_PIN])
    cg.add(var.set_scl_pin(scl))
