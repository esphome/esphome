import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.const import UNIT_COUNTS
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_ILLUMINANCE,
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
)
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

CONF_AMBIENT_LIGHT_GAIN = "ambient_light_gain"
CONF_LED_DRIVE = "led_drive"
CONF_PROXIMITY = "proximity"
CONF_PROXIMITY_GAIN = "proximity_gain"

DRIVE_LEVELS = {"100ma": 0, "50ma": 1, "25ma": 2, "12.5ma": 3}
PROXIMITY_LEVELS = {"1x": 0, "2x": 1, "4x": 2, "8x": 3}
AMBIENT_LEVELS = {"1x": 0, "8x": 1, "16x": 2, "120x": 3}

apds9930_ns = cg.esphome_ns.namespace("apds9930")
APDS9930Component = apds9930_ns.class_(
    "APDS9930Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(APDS9930Component),
            cv.Optional(CONF_LED_DRIVE, default="100ma"): cv.enum(
                DRIVE_LEVELS, lower=True
            ),
            cv.Optional(CONF_PROXIMITY_GAIN, default="8x"): cv.enum(
                PROXIMITY_LEVELS, lower=True
            ),
            cv.Optional(CONF_AMBIENT_LIGHT_GAIN, default="1x"): cv.enum(
                AMBIENT_LEVELS, lower=True
            ),
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_LUX,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PROXIMITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x39))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_led_drive(config[CONF_LED_DRIVE]))
    cg.add(var.set_proximity_gain(config[CONF_PROXIMITY_GAIN]))
    cg.add(var.set_ambient_gain(config[CONF_AMBIENT_LIGHT_GAIN]))
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_ILLUMINANCE, var.set_illuminance_sensor)
    await sensors(CONF_PROXIMITY, var.set_proximity_sensor)
