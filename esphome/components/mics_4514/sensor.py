import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_AMMONIA,
    CONF_CARBON_MONOXIDE,
    CONF_ETHANOL,
    CONF_HYDROGEN,
    CONF_ID,
    CONF_METHANE,
    CONF_NITROGEN_DIOXIDE,
    DEVICE_CLASS_CARBON_MONOXIDE,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["i2c"]


mics_4514_ns = cg.esphome_ns.namespace("mics_4514")
MICS4514Component = mics_4514_ns.class_(
    "MICS4514Component", cg.PollingComponent, i2c.I2CDevice
)

SENSORS = {
    CONF_CARBON_MONOXIDE: DEVICE_CLASS_CARBON_MONOXIDE,
    CONF_METHANE: DEVICE_CLASS_EMPTY,
    CONF_ETHANOL: DEVICE_CLASS_EMPTY,
    CONF_HYDROGEN: DEVICE_CLASS_EMPTY,
    CONF_AMMONIA: DEVICE_CLASS_EMPTY,
    CONF_NITROGEN_DIOXIDE: DEVICE_CLASS_EMPTY,
}


def common_sensor_schema(*, device_class: str) -> cv.Schema:
    return sensor.sensor_schema(
        accuracy_decimals=2,
        device_class=device_class,
        state_class=STATE_CLASS_MEASUREMENT,
        unit_of_measurement=UNIT_PARTS_PER_MILLION,
    )


CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(MICS4514Component)})
    .extend(
        {
            cv.Optional(sensor_type): common_sensor_schema(device_class=device_class)
            for sensor_type, device_class in SENSORS.items()
        }
    )
    .extend(i2c.i2c_device_schema(0x75))
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    for sensor_type in SENSORS:
        if sensor_config := config.get(sensor_type):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(getattr(var, f"set_{sensor_type}_sensor")(sens))
