import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_10_0,
    CONF_PMC_0_5,
    CONF_PMC_1_0,
    CONF_PMC_2_5,
    CONF_PMC_10_0,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    ICON_CHEMICAL_WEAPON,
    ICON_COUNTER,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
)

CODEOWNERS = ["@piitaya"]
DEPENDENCIES = ["i2c"]

ips7100_ns = cg.esphome_ns.namespace("ips7100")
IPS7100Component = ips7100_ns.class_(
    "IPS7100Component", cg.PollingComponent, i2c.I2CDevice
)

# Constants not yet in esphome.const (for external component compatibility)
CONF_PM_0_1 = "pm_0_1"
CONF_PM_0_3 = "pm_0_3"
CONF_PM_0_5 = "pm_0_5"
CONF_PM_5_0 = "pm_5_0"
CONF_PMC_0_1 = "pmc_0_1"
CONF_PMC_0_3 = "pmc_0_3"
CONF_PMC_5_0 = "pmc_5_0"

UNIT_COUNTS_PER_CUBIC_CENTIMETER = "#/cm³"


def pm_sensor_schema(device_class=None):
    """Create sensor schema for PM mass concentration."""
    kwargs = {
        "unit_of_measurement": UNIT_MICROGRAMS_PER_CUBIC_METER,
        "icon": ICON_CHEMICAL_WEAPON,
        "accuracy_decimals": 2,
        "state_class": STATE_CLASS_MEASUREMENT,
    }
    if device_class is not None:
        kwargs["device_class"] = device_class
    return sensor.sensor_schema(**kwargs)


def pmc_sensor_schema():
    """Create sensor schema for particle count."""
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_COUNTS_PER_CUBIC_CENTIMETER,
        icon=ICON_COUNTER,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    )


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(IPS7100Component),
            # PM mass concentration sensors (µg/m³)
            cv.Optional(CONF_PM_0_1): pm_sensor_schema(),
            cv.Optional(CONF_PM_0_3): pm_sensor_schema(),
            cv.Optional(CONF_PM_0_5): pm_sensor_schema(),
            cv.Optional(CONF_PM_1_0): pm_sensor_schema(DEVICE_CLASS_PM1),
            cv.Optional(CONF_PM_2_5): pm_sensor_schema(DEVICE_CLASS_PM25),
            cv.Optional(CONF_PM_5_0): pm_sensor_schema(),
            cv.Optional(CONF_PM_10_0): pm_sensor_schema(DEVICE_CLASS_PM10),
            # Particle count sensors (#/cm³)
            cv.Optional(CONF_PMC_0_1): pmc_sensor_schema(),
            cv.Optional(CONF_PMC_0_3): pmc_sensor_schema(),
            cv.Optional(CONF_PMC_0_5): pmc_sensor_schema(),
            cv.Optional(CONF_PMC_1_0): pmc_sensor_schema(),
            cv.Optional(CONF_PMC_2_5): pmc_sensor_schema(),
            cv.Optional(CONF_PMC_5_0): pmc_sensor_schema(),
            cv.Optional(CONF_PMC_10_0): pmc_sensor_schema(),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x4B))
)

SENSOR_MAP = {
    # PM mass sensors
    CONF_PM_0_1: "set_pm_0_1_sensor",
    CONF_PM_0_3: "set_pm_0_3_sensor",
    CONF_PM_0_5: "set_pm_0_5_sensor",
    CONF_PM_1_0: "set_pm_1_0_sensor",
    CONF_PM_2_5: "set_pm_2_5_sensor",
    CONF_PM_5_0: "set_pm_5_0_sensor",
    CONF_PM_10_0: "set_pm_10_0_sensor",
    # Particle count sensors
    CONF_PMC_0_1: "set_pmc_0_1_sensor",
    CONF_PMC_0_3: "set_pmc_0_3_sensor",
    CONF_PMC_0_5: "set_pmc_0_5_sensor",
    CONF_PMC_1_0: "set_pmc_1_0_sensor",
    CONF_PMC_2_5: "set_pmc_2_5_sensor",
    CONF_PMC_5_0: "set_pmc_5_0_sensor",
    CONF_PMC_10_0: "set_pmc_10_0_sensor",
}


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    for key, func_name in SENSOR_MAP.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, func_name)(sens))
