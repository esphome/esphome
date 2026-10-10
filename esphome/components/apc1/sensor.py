from esphome import automation, pins
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import sensor, uart
from esphome.components.aqi import CONF_AQI
import esphome.config_validation as cv
from esphome.const import (
    CONF_ECO2,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_PM_0_3UM,
    CONF_PM_0_5UM,
    CONF_PM_1_0,
    CONF_PM_1_0_STD,
    CONF_PM_1_0UM,
    CONF_PM_2_5,
    CONF_PM_2_5_STD,
    CONF_PM_2_5UM,
    CONF_PM_5_0UM,
    CONF_PM_10_0,
    CONF_PM_10_0_STD,
    CONF_PM_10_0UM,
    CONF_RESET_PIN,
    CONF_TEMPERATURE,
    CONF_TVOC,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_CHEMICAL_WEAPON,
    ICON_MOLECULE_CO2,
    ICON_OMEGA,
    ICON_RADIATOR,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_COUNT_DECILITRE,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_OHM,
    UNIT_PARTS_PER_BILLION,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

CODEOWNERS = ["@ademuri"]
DEPENDENCIES = ["uart"]

CONF_SET_PIN = "set_pin"
CONF_RAW_TEMPERATURE = "raw_temperature"
CONF_RAW_HUMIDITY = "raw_humidity"
CONF_RS0 = "rs0"
CONF_RS2 = "rs2"
CONF_RS3 = "rs3"
CONF_ERROR_CODE = "error_code"

apc1_ns = cg.esphome_ns.namespace("apc1")
APC1Component = apc1_ns.class_("APC1Component", uart.UARTDevice, cg.Component)


def _pm_mass_schema(device_class: str) -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
        icon=ICON_CHEMICAL_WEAPON,
        accuracy_decimals=0,
        device_class=device_class,
        state_class=STATE_CLASS_MEASUREMENT,
    )


_PM_COUNT_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_COUNT_DECILITRE,
    icon=ICON_CHEMICAL_WEAPON,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)
_TEMPERATURE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)
_HUMIDITY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_PERCENT,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_HUMIDITY,
    state_class=STATE_CLASS_MEASUREMENT,
)
_RESISTANCE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_OHM,
    icon=ICON_OMEGA,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(APC1Component),
            cv.Optional(CONF_PM_1_0): _pm_mass_schema(DEVICE_CLASS_PM1),
            cv.Optional(CONF_PM_2_5): _pm_mass_schema(DEVICE_CLASS_PM25),
            cv.Optional(CONF_PM_10_0): _pm_mass_schema(DEVICE_CLASS_PM10),
            cv.Optional(CONF_PM_1_0_STD): _pm_mass_schema(DEVICE_CLASS_PM1),
            cv.Optional(CONF_PM_2_5_STD): _pm_mass_schema(DEVICE_CLASS_PM25),
            cv.Optional(CONF_PM_10_0_STD): _pm_mass_schema(DEVICE_CLASS_PM10),
            cv.Optional(CONF_PM_0_3UM): _PM_COUNT_SCHEMA,
            cv.Optional(CONF_PM_0_5UM): _PM_COUNT_SCHEMA,
            cv.Optional(CONF_PM_1_0UM): _PM_COUNT_SCHEMA,
            cv.Optional(CONF_PM_2_5UM): _PM_COUNT_SCHEMA,
            cv.Optional(CONF_PM_5_0UM): _PM_COUNT_SCHEMA,
            cv.Optional(CONF_PM_10_0UM): _PM_COUNT_SCHEMA,
            cv.Optional(CONF_TVOC): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                icon=ICON_RADIATOR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ECO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AQI): sensor.sensor_schema(
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_AQI,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): _TEMPERATURE_SCHEMA,
            cv.Optional(CONF_HUMIDITY): _HUMIDITY_SCHEMA,
            cv.Optional(CONF_RAW_TEMPERATURE): _TEMPERATURE_SCHEMA,
            cv.Optional(CONF_RAW_HUMIDITY): _HUMIDITY_SCHEMA,
            cv.Optional(CONF_RS0): _RESISTANCE_SCHEMA,
            cv.Optional(CONF_RS2): _RESISTANCE_SCHEMA,
            cv.Optional(CONF_RS3): _RESISTANCE_SCHEMA,
            cv.Optional(CONF_ERROR_CODE): sensor.sensor_schema(
                icon="mdi:alert-circle-outline",
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_SET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


def final_validate(config: ConfigType) -> None:
    schema = uart.final_validate_device_schema(
        "apc1", baud_rate=9600, require_rx=True, require_tx=True
    )
    schema(config)


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_PM_1_0, var.set_pm_1_0_sensor)
    await sensors(CONF_PM_2_5, var.set_pm_2_5_sensor)
    await sensors(CONF_PM_10_0, var.set_pm_10_0_sensor)
    await sensors(CONF_PM_1_0_STD, var.set_pm_1_0_std_sensor)
    await sensors(CONF_PM_2_5_STD, var.set_pm_2_5_std_sensor)
    await sensors(CONF_PM_10_0_STD, var.set_pm_10_0_std_sensor)
    await sensors(CONF_PM_0_3UM, var.set_pm_0_3um_sensor)
    await sensors(CONF_PM_0_5UM, var.set_pm_0_5um_sensor)
    await sensors(CONF_PM_1_0UM, var.set_pm_1_0um_sensor)
    await sensors(CONF_PM_2_5UM, var.set_pm_2_5um_sensor)
    await sensors(CONF_PM_5_0UM, var.set_pm_5_0um_sensor)
    await sensors(CONF_PM_10_0UM, var.set_pm_10_0um_sensor)
    await sensors(CONF_TVOC, var.set_tvoc_sensor)
    await sensors(CONF_ECO2, var.set_eco2_sensor)
    await sensors(CONF_AQI, var.set_aqi_sensor)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_HUMIDITY, var.set_humidity_sensor)
    await sensors(CONF_RAW_TEMPERATURE, var.set_raw_temperature_sensor)
    await sensors(CONF_RAW_HUMIDITY, var.set_raw_humidity_sensor)
    await sensors(CONF_RS0, var.set_rs0_sensor)
    await sensors(CONF_RS2, var.set_rs2_sensor)
    await sensors(CONF_RS3, var.set_rs3_sensor)
    await sensors(CONF_ERROR_CODE, var.set_error_code_sensor)

    if (set_pin_config := config.get(CONF_SET_PIN)) is not None:
        set_pin = await cg.gpio_pin_expression(set_pin_config)
        cg.add(var.set_set_pin(set_pin))

    if (reset_pin_config := config.get(CONF_RESET_PIN)) is not None:
        reset_pin = await cg.gpio_pin_expression(reset_pin_config)
        cg.add(var.set_reset_pin(reset_pin))


APC1_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(APC1Component),
    }
)


for _name, _call in (
    ("apc1.set_active_mode", "set_active_mode()"),
    ("apc1.set_passive_mode", "set_passive_mode()"),
    ("apc1.request_measurement", "request_measurement()"),
    ("apc1.set_idle_mode", "set_idle_mode()"),
    ("apc1.set_measurement_mode", "set_measurement_mode()"),
):
    automation.register_apply_action(
        _name, APC1_ACTION_SCHEMA, automation.ApplyCall(_call)
    )
