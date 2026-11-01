import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_10_0,
    CONF_PMC_0_5,
    CONF_PMC_1_0,
    CONF_PMC_2_5,
    CONF_PMC_10_0,
    ICON_COUNTER,
    STATE_CLASS_MEASUREMENT,
)
from esphome.types import ConfigType

CODEOWNERS = ["@sjtrny"]
DEPENDENCIES = ["i2c"]

pmsa003i_ns = cg.esphome_ns.namespace("pmsa003i")

PMSA003IComponent = pmsa003i_ns.class_(
    "PMSA003IComponent", cg.PollingComponent, i2c.I2CDevice
)

CONF_STANDARD_UNITS = "standard_units"
UNIT_COUNTS_PER_100ML = "#/0.1L"
CONF_PMC_0_3 = "pmc_0_3"
CONF_PMC_5_0 = "pmc_5_0"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PMSA003IComponent),
            cv.Optional(CONF_STANDARD_UNITS, default=True): cv.boolean,
            cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
                kind=sensor.PM1, accuracy_decimals=2
            ),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                kind=sensor.PM25, accuracy_decimals=2
            ),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
                kind=sensor.PM10, accuracy_decimals=2
            ),
            cv.Optional(CONF_PMC_0_3): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS_PER_100ML,
                icon=ICON_COUNTER,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PMC_0_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS_PER_100ML,
                icon=ICON_COUNTER,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PMC_1_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS_PER_100ML,
                icon=ICON_COUNTER,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PMC_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS_PER_100ML,
                icon=ICON_COUNTER,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PMC_5_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS_PER_100ML,
                icon=ICON_COUNTER,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PMC_10_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS_PER_100ML,
                icon=ICON_COUNTER,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x12))
)

TYPES = {
    CONF_PM_1_0: "set_pm_1_0_sensor",
    CONF_PM_2_5: "set_pm_2_5_sensor",
    CONF_PM_10_0: "set_pm_10_0_sensor",
    CONF_PMC_0_3: "set_pmc_0_3_sensor",
    CONF_PMC_0_5: "set_pmc_0_5_sensor",
    CONF_PMC_1_0: "set_pmc_1_0_sensor",
    CONF_PMC_2_5: "set_pmc_2_5_sensor",
    CONF_PMC_5_0: "set_pmc_5_0_sensor",
    CONF_PMC_10_0: "set_pmc_10_0_sensor",
}


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    cg.add(var.set_standard_units(config[CONF_STANDARD_UNITS]))

    for key, funcName in TYPES.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, funcName)(sens))
