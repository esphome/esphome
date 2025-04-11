import esphome.codegen as cg
from esphome.components import sensor, uart
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

CODEOWNERS = ["@gcormier"]
DEPENDENCIES = ["uart"]

gcja5_ns = cg.esphome_ns.namespace("gcja5")

GCJA5Component = gcja5_ns.class_("GCJA5Component", cg.PollingComponent, uart.UARTDevice)

CONF_PMC_0_3 = "pmc_0_3"
CONF_PMC_5_0 = "pmc_5_0"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GCJA5Component),
        cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_CHEMICAL_WEAPON,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_PM1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_CHEMICAL_WEAPON,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_PM25,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_CHEMICAL_WEAPON,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_PM10,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PMC_0_3): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_COUNTER,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PMC_0_5): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_COUNTER,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PMC_1_0): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_COUNTER,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PMC_2_5): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_COUNTER,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PMC_5_0): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_COUNTER,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PMC_10_0): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_COUNTER,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
).extend(uart.UART_DEVICE_SCHEMA)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "gcja5", baud_rate=9600, require_rx=True, parity="EVEN"
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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    for key, funcName in TYPES.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, funcName)(sens))
