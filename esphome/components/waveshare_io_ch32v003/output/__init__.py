import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_WAVESHARE_IO_ID, WaveshareIOCH32V003Component, waveshare_io_ns

DEPENDENCIES = ["waveshare_io_ch32v003"]

WaveshareIOCH32V003Output = waveshare_io_ns.class_(
    "WaveshareIOCH32V003Output",
    output.FloatOutput,
    cg.Parented.template(WaveshareIOCH32V003Component),
)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(WaveshareIOCH32V003Output),
        cv.GenerateID(CONF_WAVESHARE_IO_ID): cv.use_id(WaveshareIOCH32V003Component),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)
    await cg.register_parented(var, config[CONF_WAVESHARE_IO_ID])
