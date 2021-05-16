import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.const import CONF_OUTPUT_ID, CONF_OUTPUT

monochromatic_ns = cg.esphome_ns.namespace("monochromatic")
MonochromaticLightOutput = monochromatic_ns.class_(
    "MonochromaticLightOutput", light.LightOutput
)

CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(MonochromaticLightOutput),
        cv.Required(CONF_OUTPUT): cv.use_id(output.FloatOutput),
    }
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    yield light.register_light(var, config)

    out = yield cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(out))
