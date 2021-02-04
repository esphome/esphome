import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c  # , sensor
from esphome.const import (
    CONF_ID,
)

DEPENDENCIES = ["i2c"]

sgp40_ns = cg.esphome_ns.namespace("sgp40")
SGP40Component = sgp40_ns.class_("SGP40Component", cg.PollingComponent, i2c.I2CDevice)

CONF_ECO2 = "eco2"
CONF_TVOC = "tvoc"
CONF_BASELINE = "baseline"
CONF_ECO2_BASELINE = "eco2_baseline"
CONF_TVOC_BASELINE = "tvoc_baseline"
CONF_UPTIME = "uptime"
CONF_COMPENSATION = "compensation"
CONF_HUMIDITY_SOURCE = "humidity_source"
CONF_TEMPERATURE_SOURCE = "temperature_source"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SGP40Component),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x59))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
