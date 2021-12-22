import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import CONF_ID

AUTO_LOAD = ["climate_ir"]

climate_ir_lg_ns = cg.esphome_ns.namespace("climate_ir_lg")
LgIrClimate = climate_ir_lg_ns.class_("LgIrClimate", climate_ir.ClimateIR)

CONF_HEADER_HIGH = "header_high"
CONF_HEADER_LOW = "header_low"
CONF_BIT_HIGH = "bit_high"
CONF_BIT_ONE_LOW = "bit_one_low"
CONF_BIT_ZERO_LOW = "bit_zero_low"

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(LgIrClimate),
        cv.Optional(
            CONF_HEADER_HIGH, default="8000us"
        ): cv.positive_time_period_microseconds,
        cv.Optional(
            CONF_HEADER_LOW, default="4000us"
        ): cv.positive_time_period_microseconds,
        cv.Optional(
            CONF_BIT_HIGH, default="600us"
        ): cv.positive_time_period_microseconds,
        cv.Optional(
            CONF_BIT_ONE_LOW, default="1600us"
        ): cv.positive_time_period_microseconds,
        cv.Optional(
            CONF_BIT_ZERO_LOW, default="550us"
        ): cv.positive_time_period_microseconds,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate_ir.register_climate_ir(var, config)

    cg.add(var.set_header_high(config[CONF_HEADER_HIGH]))
    cg.add(var.set_header_low(config[CONF_HEADER_LOW]))
    cg.add(var.set_bit_high(config[CONF_BIT_HIGH]))
    cg.add(var.set_bit_one_low(config[CONF_BIT_ONE_LOW]))
    cg.add(var.set_bit_zero_low(config[CONF_BIT_ZERO_LOW]))
