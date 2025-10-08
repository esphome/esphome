import esphome.codegen as cg
from esphome.components import i2c, sensirion_common, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_FORMALDEHYDE,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_GAS,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    ICON_RADIATOR,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PARTS_PER_BILLION,
    UNIT_PERCENT,
)

CODEOWNERS = ["@ghsensdev"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

sfa30_ns = cg.esphome_ns.namespace("sfa30")

SFA30Component = sfa30_ns.class_(
    "SFA30Component", cg.PollingComponent, sensirion_common.SensirionI2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SFA30Component),
            cv.Optional(CONF_FORMALDEHYDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                icon=ICON_RADIATOR,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_GAS,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x5D))
)

SENSOR_MAP = {
    CONF_FORMALDEHYDE: "set_formaldehyde_sensor",
    CONF_HUMIDITY: "set_humidity_sensor",
    CONF_TEMPERATURE: "set_temperature_sensor",
}


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    for key, funcName in SENSOR_MAP.items():
        if sensor_config := config.get(key):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(getattr(var, funcName)(sens))
