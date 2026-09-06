from esphome import automation, pins
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import sensor, uart
from esphome.components.const import CONF_AQI
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
    CONF_UPDATE_INTERVAL,
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
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArgsType
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

SetActiveModeAction = apc1_ns.class_("SetActiveModeAction", automation.Action)
SetPassiveModeAction = apc1_ns.class_("SetPassiveModeAction", automation.Action)
RequestMeasurementAction = apc1_ns.class_("RequestMeasurementAction", automation.Action)
SetIdleModeAction = apc1_ns.class_("SetIdleModeAction", automation.Action)
SetMeasurementModeAction = apc1_ns.class_("SetMeasurementModeAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(APC1Component),
            cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM25,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM10,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_1_0_STD): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_2_5_STD): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM25,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_10_0_STD): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM10,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_0_3UM): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNT_DECILITRE,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_0_5UM): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNT_DECILITRE,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_1_0UM): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNT_DECILITRE,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_2_5UM): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNT_DECILITRE,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_5_0UM): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNT_DECILITRE,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_10_0UM): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNT_DECILITRE,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
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
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RAW_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RAW_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RS0): sensor.sensor_schema(
                unit_of_measurement=UNIT_OHM,
                icon=ICON_OMEGA,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RS2): sensor.sensor_schema(
                unit_of_measurement=UNIT_OHM,
                icon=ICON_OMEGA,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RS3): sensor.sensor_schema(
                unit_of_measurement=UNIT_OHM,
                icon=ICON_OMEGA,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ERROR_CODE): sensor.sensor_schema(
                icon="mdi:alert-circle-outline",
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_SET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
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

SENSORS = [
    (CONF_PM_1_0, "set_pm_1_0_sensor"),
    (CONF_PM_2_5, "set_pm_2_5_sensor"),
    (CONF_PM_10_0, "set_pm_10_0_sensor"),
    (CONF_PM_1_0_STD, "set_pm_1_0_std_sensor"),
    (CONF_PM_2_5_STD, "set_pm_2_5_std_sensor"),
    (CONF_PM_10_0_STD, "set_pm_10_0_std_sensor"),
    (CONF_PM_0_3UM, "set_pm_0_3um_sensor"),
    (CONF_PM_0_5UM, "set_pm_0_5um_sensor"),
    (CONF_PM_1_0UM, "set_pm_1_0um_sensor"),
    (CONF_PM_2_5UM, "set_pm_2_5um_sensor"),
    (CONF_PM_5_0UM, "set_pm_5_0um_sensor"),
    (CONF_PM_10_0UM, "set_pm_10_0um_sensor"),
    (CONF_TVOC, "set_tvoc_sensor"),
    (CONF_ECO2, "set_eco2_sensor"),
    (CONF_AQI, "set_aqi_sensor"),
    (CONF_TEMPERATURE, "set_temperature_sensor"),
    (CONF_HUMIDITY, "set_humidity_sensor"),
    (CONF_RAW_TEMPERATURE, "set_raw_temperature_sensor"),
    (CONF_RAW_HUMIDITY, "set_raw_humidity_sensor"),
    (CONF_RS0, "set_rs0_sensor"),
    (CONF_RS2, "set_rs2_sensor"),
    (CONF_RS3, "set_rs3_sensor"),
    (CONF_ERROR_CODE, "set_error_code_sensor"),
]


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    for conf_key, setter in SENSORS:
        if (sensor_config := config.get(conf_key)) is not None:
            sens = await sensor.new_sensor(sensor_config)
            cg.add(getattr(var, setter)(sens))

    if (set_pin_config := config.get(CONF_SET_PIN)) is not None:
        set_pin = await cg.gpio_pin_expression(set_pin_config)
        cg.add(var.set_set_pin(set_pin))

    if (reset_pin_config := config.get(CONF_RESET_PIN)) is not None:
        reset_pin = await cg.gpio_pin_expression(reset_pin_config)
        cg.add(var.set_reset_pin(reset_pin))

    if (update_interval := config.get(CONF_UPDATE_INTERVAL)) is not None:
        cg.add(var.set_update_interval(update_interval))


APC1_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(APC1Component),
    }
)


@automation.register_action(
    "apc1.set_active_mode",
    SetActiveModeAction,
    APC1_ACTION_SCHEMA,
    synchronous=True,
)
@automation.register_action(
    "apc1.set_passive_mode",
    SetPassiveModeAction,
    APC1_ACTION_SCHEMA,
    synchronous=True,
)
@automation.register_action(
    "apc1.request_measurement",
    RequestMeasurementAction,
    APC1_ACTION_SCHEMA,
    synchronous=True,
)
@automation.register_action(
    "apc1.set_idle_mode",
    SetIdleModeAction,
    APC1_ACTION_SCHEMA,
    synchronous=True,
)
@automation.register_action(
    "apc1.set_measurement_mode",
    SetMeasurementModeAction,
    APC1_ACTION_SCHEMA,
    synchronous=True,
)
async def apc1_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
