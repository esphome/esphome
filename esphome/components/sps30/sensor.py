import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_4_0,
    CONF_PM_10_0,
    CONF_PMC_0_5,
    CONF_PMC_1_0,
    CONF_PMC_2_5,
    CONF_PMC_4_0,
    CONF_PMC_10_0,
    CONF_PM_SIZE,
    DEVICE_CLASS_EMPTY,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_COUNTS_PER_CUBIC_METER,
    UNIT_MICROMETER,
    ICON_CHEMICAL_WEAPON,
    ICON_COUNTER,
    ICON_RULER,
)

DEPENDENCIES = ["i2c"]

sps30_ns = cg.esphome_ns.namespace("sps30")
SPS30Component = sps30_ns.class_("SPS30Component", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SPS30Component),
            cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
                UNIT_MICROGRAMS_PER_CUBIC_METER,
                ICON_CHEMICAL_WEAPON,
                2,
                DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                UNIT_MICROGRAMS_PER_CUBIC_METER,
                ICON_CHEMICAL_WEAPON,
                2,
                DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(CONF_PM_4_0): sensor.sensor_schema(
                UNIT_MICROGRAMS_PER_CUBIC_METER,
                ICON_CHEMICAL_WEAPON,
                2,
                DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
                UNIT_MICROGRAMS_PER_CUBIC_METER,
                ICON_CHEMICAL_WEAPON,
                2,
                DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(CONF_PMC_0_5): sensor.sensor_schema(
                UNIT_COUNTS_PER_CUBIC_METER, ICON_COUNTER, 2, DEVICE_CLASS_EMPTY
            ),
            cv.Optional(CONF_PMC_1_0): sensor.sensor_schema(
                UNIT_COUNTS_PER_CUBIC_METER, ICON_COUNTER, 2, DEVICE_CLASS_EMPTY
            ),
            cv.Optional(CONF_PMC_2_5): sensor.sensor_schema(
                UNIT_COUNTS_PER_CUBIC_METER, ICON_COUNTER, 2, DEVICE_CLASS_EMPTY
            ),
            cv.Optional(CONF_PMC_4_0): sensor.sensor_schema(
                UNIT_COUNTS_PER_CUBIC_METER, ICON_COUNTER, 2, DEVICE_CLASS_EMPTY
            ),
            cv.Optional(CONF_PMC_10_0): sensor.sensor_schema(
                UNIT_COUNTS_PER_CUBIC_METER, ICON_COUNTER, 2, DEVICE_CLASS_EMPTY
            ),
            cv.Optional(CONF_PM_SIZE): sensor.sensor_schema(
                UNIT_MICROMETER, ICON_RULER, 0, DEVICE_CLASS_EMPTY
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x69))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    if CONF_PM_1_0 in config:
        sens = yield sensor.new_sensor(config[CONF_PM_1_0])
        cg.add(var.set_pm_1_0_sensor(sens))

    if CONF_PM_2_5 in config:
        sens = yield sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(var.set_pm_2_5_sensor(sens))

    if CONF_PM_4_0 in config:
        sens = yield sensor.new_sensor(config[CONF_PM_4_0])
        cg.add(var.set_pm_4_0_sensor(sens))

    if CONF_PM_10_0 in config:
        sens = yield sensor.new_sensor(config[CONF_PM_10_0])
        cg.add(var.set_pm_10_0_sensor(sens))

    if CONF_PMC_0_5 in config:
        sens = yield sensor.new_sensor(config[CONF_PMC_0_5])
        cg.add(var.set_pmc_0_5_sensor(sens))

    if CONF_PMC_1_0 in config:
        sens = yield sensor.new_sensor(config[CONF_PMC_1_0])
        cg.add(var.set_pmc_1_0_sensor(sens))

    if CONF_PMC_2_5 in config:
        sens = yield sensor.new_sensor(config[CONF_PMC_2_5])
        cg.add(var.set_pmc_2_5_sensor(sens))

    if CONF_PMC_4_0 in config:
        sens = yield sensor.new_sensor(config[CONF_PMC_4_0])
        cg.add(var.set_pmc_4_0_sensor(sens))

    if CONF_PMC_10_0 in config:
        sens = yield sensor.new_sensor(config[CONF_PMC_10_0])
        cg.add(var.set_pmc_10_0_sensor(sens))

    if CONF_PM_SIZE in config:
        sens = yield sensor.new_sensor(config[CONF_PM_SIZE])
        cg.add(var.set_pm_size_sensor(sens))
