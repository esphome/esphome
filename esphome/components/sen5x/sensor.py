import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_4_0,
    CONF_PM_10_0,
    CONF_TEMPERATURE,
    CONF_HUMIDITY,
    CONF_TVOC,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    ICON_CHEMICAL_WEAPON,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    UNIT_EMPTY,
)

DEPENDENCIES = ["i2c"]

sen5x_ns = cg.esphome_ns.namespace("sen5x")
SEN5xComponent = sen5x_ns.class_("SEN5xComponent", cg.PollingComponent, i2c.I2CDevice)

# Temperature, humidity, VOC only supported in SEN54 and SEN55
# NOx Index only supported in SEN55
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SEN5xComponent),
            cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_PM1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_PM25,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_4_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_PM10,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TVOC): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            # TODO : Add config for NOx Index (SEN55 only)
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x69))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_PM_1_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_1_0])
        cg.add(var.set_pm_1_0_sensor(sens))

    if CONF_PM_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(var.set_pm_2_5_sensor(sens))

    if CONF_PM_4_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_4_0])
        cg.add(var.set_pm_4_0_sensor(sens))

    if CONF_PM_10_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_10_0])
        cg.add(var.set_pm_10_0_sensor(sens))

    if CONF_TVOC in config:
        sens = await sensor.new_sensor(config[CONF_TVOC])
        cg.add(var.set_voc_sensor(sens))

    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    # TODO : add handler for NOx (SEN55 hasn't been released yet though...)
