import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_BATTERY_VOLTAGE,
    CONF_ID,
    CONF_SIZE,
    CONF_TEMPERATURE,
    CONF_VOLTAGE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT,
)

DEPENDENCIES = ["i2c"]

lc709203f_ns = cg.esphome_ns.namespace("lc709203f")

CONF_B_CONSTANT = "b_constant"

LC709203FBatteryVoltage = lc709203f_ns.enum("LC709203FBatteryVoltage")
BATTERY_VOLTAGE_OPTIONS = {
    "3.7": LC709203FBatteryVoltage.LC709203F_BATTERY_VOLTAGE_3_7,
    "3.8": LC709203FBatteryVoltage.LC709203F_BATTERY_VOLTAGE_3_8,
}

lc709203f = lc709203f_ns.class_("Lc709203f", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(lc709203f),
            cv.Optional(CONF_SIZE, default="500"): cv.int_range(100, 3000),
            cv.Optional(CONF_VOLTAGE, default="3.7"): cv.enum(
                BATTERY_VOLTAGE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ).extend(
                {
                    cv.Required(CONF_B_CONSTANT): cv.int_range(0, 0xFFFF),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x0B))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_pack_size(config.get(CONF_SIZE)))
    cg.add(var.set_pack_voltage(BATTERY_VOLTAGE_OPTIONS[config[CONF_VOLTAGE]]))

    if voltage_config := config.get(CONF_BATTERY_VOLTAGE):
        sens = await sensor.new_sensor(voltage_config)
        cg.add(var.set_voltage_sensor(sens))

    if level_config := config.get(CONF_BATTERY_LEVEL):
        sens = await sensor.new_sensor(level_config)
        cg.add(var.set_battery_remaining_sensor(sens))

    if temp_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temp_config)
        cg.add(var.set_temperature_sensor(sens))
        cg.add(var.set_thermistor_b_constant(temp_config[CONF_B_CONSTANT]))
