import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@neffs"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "text_sensor"]

CONF_BME68X_BSEC_ID = "bme68x_bsec_id"
CONF_TEMPERATURE_OFFSET = "temperature_offset"
CONF_IAQ_MODE = "iaq_mode"
CONF_SAMPLE_RATE = "sample_rate"
CONF_STATE_SAVE_INTERVAL = "state_save_interval"
CONF_BSEC_CONFIG = "bsec_configuration"

bme68x_bsec_ns = cg.esphome_ns.namespace("bme68x_bsec")


SampleRate = bme68x_bsec_ns.enum("SampleRate")
SAMPLE_RATE_OPTIONS = {
    "LP": SampleRate.SAMPLE_RATE_LP,
    "ULP": SampleRate.SAMPLE_RATE_ULP,
}

BME68xBSECComponent = bme68x_bsec_ns.class_(
    "BME68XBSECComponent", cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BME68xBSECComponent),
        cv.Optional(CONF_TEMPERATURE_OFFSET, default=0): cv.temperature,
        cv.Optional(CONF_SAMPLE_RATE, default="LP"): cv.enum(
            SAMPLE_RATE_OPTIONS, upper=True
        ),
        cv.Optional(
            CONF_STATE_SAVE_INTERVAL, default="6hours"
        ): cv.positive_time_period_minutes,
        cv.Optional(CONF_BSEC_CONFIG, default=""): cv.string,
    },
    cv.only_with_arduino,
).extend(i2c.i2c_device_schema(0x76))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_temperature_offset(config[CONF_TEMPERATURE_OFFSET]))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(
        var.set_state_save_interval(config[CONF_STATE_SAVE_INTERVAL].total_milliseconds)
    )
    if config[CONF_BSEC_CONFIG] != "":
        # Convert BSEC Config to int array
        temp = [int(a) for a in config[CONF_BSEC_CONFIG].split(",")]
        # We can't call set_config_() with this array directly, because we don't
        # have a pointer in this case.
        # Instead we use a define, and handle it .cpp
        cg.add_define("BME68X_BSEC_CONFIGURATION", temp)

    # Although this component does not use SPI, the BSEC library requires the SPI library
    cg.add_library("SPI", None)

    cg.add_define("USE_BSEC2")
    cg.add_library(
        "BME68x Sensor library",
        "1.1.40406",
        "https://github.com/BoschSensortec/Bosch-BME68x-Library.git",
    )
    cg.add_library(
        "BSEC2 Software Library",
        "1.1.2061",
        "https://github.com/BoschSensortec/Bosch-BSEC2-Library.git",
    )
