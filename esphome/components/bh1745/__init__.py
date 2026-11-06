import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_GAIN,
    CONF_GLASS_ATTENUATION_FACTOR,
    CONF_INTEGRATION_TIME,
)
from esphome.types import ConfigType

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["i2c"]
DOMAIN = "bh1745"
MULTI_CONF = True

CONF_BH1745_ID = "bh1745_id"

bh1745_ns = cg.esphome_ns.namespace("bh1745")

BH1745Component = bh1745_ns.class_(
    "BH1745Component", cg.PollingComponent, i2c.I2CDevice
)

AdcGain = bh1745_ns.enum("AdcGain", is_class=True)
ADC_GAINS = AdcGain.options("1X", "2X", "16X", prefix="ADC_GAIN_")

MeasurementTime = bh1745_ns.enum("MeasurementTime", is_class=True)
MEASUREMENT_TIMES = {
    160: MeasurementTime.MEASUREMENT_TIME_160MS,
    320: MeasurementTime.MEASUREMENT_TIME_320MS,
    640: MeasurementTime.MEASUREMENT_TIME_640MS,
    1280: MeasurementTime.MEASUREMENT_TIME_1280MS,
    2560: MeasurementTime.MEASUREMENT_TIME_2560MS,
    5120: MeasurementTime.MEASUREMENT_TIME_5120MS,
}


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BH1745Component),
            cv.Optional(CONF_GAIN, default="1X"): cv.enum(ADC_GAINS, upper=True),
            cv.Optional(CONF_INTEGRATION_TIME, default="160ms"): cv.time_period_enum(
                MEASUREMENT_TIMES
            ),
            cv.Optional(CONF_GLASS_ATTENUATION_FACTOR, default=1.0): cv.float_range(
                min=1.0
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x38))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    cg.add(var.set_adc_gain(config[CONF_GAIN]))
    cg.add(var.set_measurement_time(config[CONF_INTEGRATION_TIME]))
    cg.add(var.set_glass_attenuation_factor(config[CONF_GLASS_ATTENUATION_FACTOR]))
