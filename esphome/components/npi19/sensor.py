import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

CODEOWNERS = ["@bakerkj"]
DEPENDENCIES = ["i2c"]

npi19_ns = cg.esphome_ns.namespace("npi19")

NPI19Component = npi19_ns.class_("NPI19Component", cg.PollingComponent, i2c.I2CDevice)

CONF_RAW_PRESSURE = "raw_pressure"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NPI19Component),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RAW_PRESSURE): sensor.sensor_schema(
                accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x28))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(var.set_temperature_sensor(sens))

    if raw_pressure_config := config.get(CONF_RAW_PRESSURE):
        sens = await sensor.new_sensor(raw_pressure_config)
        cg.add(var.set_raw_pressure_sensor(sens))
