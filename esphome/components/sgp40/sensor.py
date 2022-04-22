import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor, sensirion_common

from esphome.const import (
    CONF_STORE_BASELINE,
    CONF_TEMPERATURE_SOURCE,
    ICON_RADIATOR,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

CODEOWNERS = ["@SenexCrenshaw"]

sgp40_ns = cg.esphome_ns.namespace("sgp40")
SGP40Component = sgp40_ns.class_(
    "SGP40Component",
    sensor.Sensor,
    cg.PollingComponent,
    sensirion_common.SensirionI2CDevice,
)

CONF_COMPENSATION = "compensation"
CONF_HUMIDITY_SOURCE = "humidity_source"
CONF_VOC_BASELINE = "voc_baseline"

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        SGP40Component,
        icon=ICON_RADIATOR,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(CONF_STORE_BASELINE, default=True): cv.boolean,
            cv.Optional(CONF_VOC_BASELINE): cv.hex_uint16_t,
            cv.Optional(CONF_COMPENSATION): cv.Schema(
                {
                    cv.Required(CONF_HUMIDITY_SOURCE): cv.use_id(sensor.Sensor),
                    cv.Required(CONF_TEMPERATURE_SOURCE): cv.use_id(sensor.Sensor),
                },
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x59))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_COMPENSATION in config:
        compensation_config = config[CONF_COMPENSATION]
        sens = await cg.get_variable(compensation_config[CONF_HUMIDITY_SOURCE])
        cg.add(var.set_humidity_sensor(sens))
        sens = await cg.get_variable(compensation_config[CONF_TEMPERATURE_SOURCE])
        cg.add(var.set_temperature_sensor(sens))

    cg.add(var.set_store_baseline(config[CONF_STORE_BASELINE]))

    if CONF_VOC_BASELINE in config:
        cg.add(var.set_voc_baseline(CONF_VOC_BASELINE))
