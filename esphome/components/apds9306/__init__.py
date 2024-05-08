import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_APDS9306_ID = "apds9306_id"
CONF_AMBIENT_LIGHT_GAIN = "gain"
CONF_BIT_WIDTH = "bit_width"
CONF_MEASUREMENT_RATE = "measurement_rate"

MEASUREMENT_BIT_WIDTHS = {
    "20": 0,
    "19": 1,
    "18": 2,
    "17": 3,
    "16": 4,
    "13": 5
}

MEASUREMENT_RATES = {
    "25ms": 0,
    "50ms": 1,
    "100ms": 2,
    "200ms": 3,
    "500ms": 4,
    "1000ms": 5
}

AMBIANT_LIGHT_GAINS = {
    "1": 0,
    "3": 1,
    "6": 2,
    "9": 3,
    "18": 4
}

apds9306_nds = cg.esphome_ns.namespace("apds9306")
APDS9306 = apds9306_nds.class_("APDS9306", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(APDS9306),
            cv.Optional(CONF_AMBIENT_LIGHT_GAIN, "1"): cv.enum(AMBIANT_LIGHT_GAINS, lower=True),
            cv.Optional(CONF_BIT_WIDTH, "18"): cv.enum(MEASUREMENT_BIT_WIDTHS, 2),
            cv.Optional(CONF_MEASUREMENT_RATE, "100ms"): cv.enum(MEASUREMENT_RATES, 2)
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x52))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_bid_width(config[CONF_BIT_WIDTH]))
    cg.add(var.set_measurement_rate(config[CONF_MEASUREMENT_RATE]))
    cg.add(var.set_ambiant_light_gain(config[CONF_AMBIENT_LIGHT_GAIN]))
