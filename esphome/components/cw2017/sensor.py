import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_BATTERY_VOLTAGE,
    CONF_RAW_DATA_ID,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
    UNIT_VOLT,
)
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

CONF_BATTERY_PROFILE = "battery_profile"
PROFILE_SIZE = 80

cw2017_ns = cg.esphome_ns.namespace("cw2017")
CW2017 = cw2017_ns.class_("CW2017", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CW2017),
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
            cv.Required(CONF_BATTERY_PROFILE): cv.All(
                cv.ensure_list(cv.hex_uint8_t),
                cv.Length(min=PROFILE_SIZE, max=PROFILE_SIZE),
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x63))
)


async def to_code(config: ConfigType) -> None:
    profile = cg.progmem_array(config[CONF_RAW_DATA_ID], config[CONF_BATTERY_PROFILE])
    var = await i2c.new_i2c_component(config, profile)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_BATTERY_LEVEL, var.set_battery_level_sensor)
    await sensors(CONF_BATTERY_VOLTAGE, var.set_battery_voltage_sensor)
