import esphome.codegen as cg
from esphome.components import binary_sensor, i2c, sensor, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_BATTERY_VOLTAGE,
    CONF_ID,
    CONF_INTERNAL_TEMPERATURE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_BATTERY_CHARGING,
    DEVICE_CLASS_PRESENCE,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ENTITY_CATEGORY_NONE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT,
)

CODEOWNERS = ["@mschnaubelt"]
DEPENDENCIES = ["i2c"]

CONF_ICC_LIMIT = "icc_limit"
CONF_BATTERY_PRESENT = "battery_present"
CONF_BATTERY_CHARGING = "battery_charging"
CONF_BATTERY_STATUS = "battery_status"

axp2101_ns = cg.esphome_ns.namespace("axp2101")
AXP2101Component = axp2101_ns.class_(
    "AXP2101Component", cg.PollingComponent, i2c.I2CDevice, sensor.Sensor
)

ICC_LIMIT_OPTIONS = {
    "0mA": 0x00,  # 0mA
    "25mA": 0x01,  # 25mA
    "50mA": 0x02,  # 50mA
    "75mA": 0x03,  # 75mA
    "100mA": 0x04,  # 100mA
    "125mA": 0x05,  # 125mA
    "150mA": 0x06,  # 150mA
    "175mA": 0x07,  # 175mA
    "200mA": 0x08,  # 200mA
    "300mA": 0x09,  # 300mA
    "400mA": 0x0A,  # 400mA
    "500mA": 0x0B,  # 500mA
    "600mA": 0x0C,  # 600mA
    "700mA": 0x0D,  # 700mA
    "800mA": 0x0E,  # 800mA
    "900mA": 0x0F,  # 900mA
    "1000mA": 0x10,  # 1000mA
    "1100mA": 0x11,  # 1100mA
    "1200mA": 0x12,  # 1200mA
    "1300mA": 0x13,  # 1300mA
    "1400mA": 0x14,  # 1400mA
    "1500mA": 0x15,  # 1500mA
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AXP2101Component),
            cv.Optional(CONF_ICC_LIMIT, default="100mA"): cv.enum(ICC_LIMIT_OPTIONS),
            cv.Optional(CONF_INTERNAL_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_NONE,
            ),
            cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_BATTERY_CHARGING): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_BATTERY_CHARGING
            ),
            cv.Optional(CONF_BATTERY_PRESENT): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PRESENCE,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_BATTERY_STATUS): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:battery-heart"
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x34))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_icc_limit(config[CONF_ICC_LIMIT]))

    if CONF_INTERNAL_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_INTERNAL_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_BATTERY_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(var.set_battery_remaining_sensor(sens))

    if CONF_BATTERY_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_VOLTAGE])
        cg.add(var.set_battery_voltage_sensor(sens))

    if CONF_BATTERY_PRESENT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_BATTERY_PRESENT])
        cg.add(var.set_battery_present_sensor(sens))

    if CONF_BATTERY_CHARGING in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_BATTERY_CHARGING])
        cg.add(var.set_battery_charging_sensor(sens))

    if CONF_BATTERY_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_BATTERY_STATUS])
        cg.add(var.set_battery_status_sensor(sens))
