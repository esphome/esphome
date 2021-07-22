import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN

MULTI_CONF = True
AUTO_LOAD = ["sensor"]

CONF_ONE_WIRE_ID = "one_wire_id"
dallas_ns = cg.esphome_ns.namespace("dallas")
DallasComponent = dallas_ns.class_("DallasComponent", cg.PollingComponent)
ESPOneWire = dallas_ns.class_("ESPOneWire")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DallasComponent),
        cv.GenerateID(CONF_ONE_WIRE_ID): cv.declare_id(ESPOneWire),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    one_wire = cg.new_Pvariable(config[CONF_ONE_WIRE_ID], pin)
    var = cg.new_Pvariable(config[CONF_ID], one_wire)
    await cg.register_component(var, config)
