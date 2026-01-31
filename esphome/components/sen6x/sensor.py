from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c, sensirion_common, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALGORITHM_TUNING,
    CONF_ALTITUDE,
    CONF_ALTITUDE_COMPENSATION,
    CONF_AMBIENT_PRESSURE_COMPENSATION,
    CONF_CO2,
    CONF_FORMALDEHYDE,
    CONF_GAIN_FACTOR,
    CONF_GATING_MAX_DURATION_MINUTES,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_INDEX_OFFSET,
    CONF_LEARNING_TIME_GAIN_HOURS,
    CONF_LEARNING_TIME_OFFSET_HOURS,
    CONF_NORMALIZED_OFFSET_SLOPE,
    CONF_NOX,
    CONF_OFFSET,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_4_0,
    CONF_PM_10_0,
    CONF_PRESSURE,
    CONF_STD_INITIAL,
    CONF_STORE_BASELINE,
    CONF_TEMPERATURE,
    CONF_TEMPERATURE_COMPENSATION,
    CONF_TIME_CONSTANT,
    CONF_TYPE,
    CONF_VOC,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_TEMPERATURE,
    ICON_CHEMICAL_WEAPON,
    ICON_MOLECULE_CO2,
    ICON_RADIATOR,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)

CODEOWNERS = ["@martgras"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

sen6x_ns = cg.esphome_ns.namespace("sen6x")
SEN6XComponent = sen6x_ns.class_(
    "SEN6XComponent", cg.PollingComponent, sensirion_common.SensirionI2CDevice
)

CONF_SLOT = "slot"

# Additional configuration constants needed for GitHub version
CONF_ENABLED = "enabled"
CONF_INTERVAL = "interval"
CONF_AMBIENT_PRESSURE = "ambient_pressure"
CONF_SENSOR_ALTITUDE = "sensor_altitude"
CONF_CO2_ASC = "co2_automatic_self_calibration"
CONF_STARTUP_DELAY = "startup_delay"
CONF_AUTO_CLEANING = "auto_cleaning"
CONF_HCHO = "hcho"
CONF_VOC_BASELINE = "voc_baseline"
CONF_TEMPERATURE_ACCELERATION = "temperature_acceleration"
CONF_K = "k"
CONF_P = "p"
CONF_T1 = "t1"
CONF_T2 = "t2"

# Actions
StartFanAction = sen6x_ns.class_("StartFanAction", automation.Action)
PerformForcedCO2RecalibrationAction = sen6x_ns.class_(
    "PerformForcedCO2RecalibrationAction", automation.Action
)
CO2SensorFactoryResetAction = sen6x_ns.class_(
    "CO2SensorFactoryResetAction", automation.Action
)
ActivateSHTHeaterAction = sen6x_ns.class_("ActivateSHTHeaterAction", automation.Action)
GetSHTHeaterMeasurementsAction = sen6x_ns.class_(
    "GetSHTHeaterMeasurementsAction", automation.Action
)
StartMeasurementAction = sen6x_ns.class_("StartMeasurementAction", automation.Action)
StopMeasurementAction = sen6x_ns.class_("StopMeasurementAction", automation.Action)

CONF_REFERENCE_CO2 = "reference_co2"


GAS_SENSOR = cv.Schema(
    {
        cv.Optional(CONF_ALGORITHM_TUNING): cv.Schema(
            {
                cv.Optional(CONF_INDEX_OFFSET, default=100): cv.int_range(1, 250),
                cv.Optional(CONF_LEARNING_TIME_OFFSET_HOURS, default=12): cv.int_range(
                    1, 1000
                ),
                cv.Optional(CONF_LEARNING_TIME_GAIN_HOURS, default=12): cv.int_range(
                    1, 1000
                ),
                cv.Optional(
                    CONF_GATING_MAX_DURATION_MINUTES, default=720
                ): cv.int_range(0, 3000),
                cv.Optional(CONF_STD_INITIAL, default=50): cv.int_,
                cv.Optional(CONF_GAIN_FACTOR, default=230): cv.int_range(1, 1000),
            }
        )
    }
)


def float_previously_pct(value):
    if isinstance(value, str) and "%" in value:
        raise cv.Invalid(
            f"The value '{value}' is a percentage. Suggested value: {float(value.strip('%')) / 100}"
        )
    return value


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SEN6XComponent),
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
            cv.Optional(CONF_PM_4_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PM10,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VOC): sensor.sensor_schema(
                icon=ICON_RADIATOR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_AQI,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(GAS_SENSOR),
            cv.Optional(CONF_NOX): sensor.sensor_schema(
                icon=ICON_RADIATOR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_AQI,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(GAS_SENSOR),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HCHO): sensor.sensor_schema(
                unit_of_measurement="ppb",
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AMBIENT_PRESSURE): cv.int_range(700, 1200),
            cv.Optional(CONF_SENSOR_ALTITUDE): cv.int_range(0, 3000),
            cv.Optional(CONF_CO2_ASC): cv.boolean,
            cv.Optional(
                CONF_STARTUP_DELAY, default="60s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_AUTO_CLEANING): cv.Schema(
                {
                    cv.Optional(CONF_ENABLED, default=True): cv.boolean,
                    cv.Optional(
                        CONF_INTERVAL, default="1week"
                    ): cv.positive_time_period_seconds,
                }
            ),
            cv.Optional(CONF_STORE_BASELINE, default=True): cv.boolean,
            cv.Optional(CONF_VOC_BASELINE): cv.hex_uint16_t,
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
            cv.Optional(CONF_TEMPERATURE_COMPENSATION): cv.Schema(
                {
                    cv.Optional(CONF_OFFSET, default=0): cv.float_range(
                        -163.84, 163.835
                    ),
                    cv.Optional(CONF_NORMALIZED_OFFSET_SLOPE, default=0): cv.All(
                        float_previously_pct, cv.float_range(-3.2768, 3.2767)
                    ),
                    cv.Optional(CONF_TIME_CONSTANT, default=0): cv.int_range(0, 65535),
                    cv.Optional(CONF_SLOT, default=0): cv.int_range(0, 4),
                }
            ),
            cv.Optional(CONF_TEMPERATURE_ACCELERATION): cv.Schema(
                {
                    cv.Optional(CONF_K, default=0): cv.float_range(0, 6553.5),
                    cv.Optional(CONF_P, default=0): cv.float_range(0, 6553.5),
                    cv.Optional(CONF_T1, default=0): cv.float_range(0, 6553.5),
                    cv.Optional(CONF_T2, default=0): cv.float_range(0, 6553.5),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6B))
)

SENSOR_MAP = {
    CONF_PM_1_0: "set_pm_1_0_sensor",
    CONF_PM_2_5: "set_pm_2_5_sensor",
    CONF_PM_4_0: "set_pm_4_0_sensor",
    CONF_PM_10_0: "set_pm_10_0_sensor",
    CONF_TEMPERATURE: "set_temperature_sensor",
    CONF_HUMIDITY: "set_humidity_sensor",
    CONF_VOC: "set_voc_sensor",
    CONF_NOX: "set_nox_sensor",
    CONF_CO2: "set_co2_sensor",
    CONF_FORMALDEHYDE: "set_hcho_sensor",
}


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_STARTUP_DELAY in config:
        cg.add(var.set_startup_delay(config[CONF_STARTUP_DELAY]))

    if CONF_TYPE in config:
        cg.add(var.set_type(config[CONF_TYPE]))

    for key, funcName in SENSOR_MAP.items():
        if cfg := config.get(key):
            sens = await sensor.new_sensor(cfg)
            cg.add(getattr(var, funcName)(sens))

    if cfg := config.get(CONF_VOC, {}).get(CONF_ALGORITHM_TUNING):
        cg.add(
            var.set_voc_algorithm_tuning(
                cfg[CONF_INDEX_OFFSET],
                cfg[CONF_LEARNING_TIME_OFFSET_HOURS],
                cfg[CONF_LEARNING_TIME_GAIN_HOURS],
                cfg[CONF_GATING_MAX_DURATION_MINUTES],
                cfg[CONF_STD_INITIAL],
                cfg[CONF_GAIN_FACTOR],
            )
        )
    if cfg := config.get(CONF_NOX, {}).get(CONF_ALGORITHM_TUNING):
        cg.add(
            var.set_nox_algorithm_tuning(
                cfg[CONF_INDEX_OFFSET],
                cfg[CONF_LEARNING_TIME_OFFSET_HOURS],
                cfg[CONF_LEARNING_TIME_GAIN_HOURS],
                cfg[CONF_GATING_MAX_DURATION_MINUTES],
                cfg[CONF_GAIN_FACTOR],
            )
        )
    if cfg := config.get(CONF_TEMPERATURE_COMPENSATION):
        cg.add(
            var.set_temperature_compensation(
                cfg[CONF_OFFSET],
                cfg[CONF_NORMALIZED_OFFSET_SLOPE],
                cfg[CONF_TIME_CONSTANT],
                cfg[CONF_SLOT],
            )
        )
    if cfg := config.get(CONF_CO2, {}).get(CONF_AMBIENT_PRESSURE_COMPENSATION):
        cg.add(
            var.set_pressure_compensation(
                cfg[CONF_PRESSURE],
            )
        )
    if cfg := config.get(CONF_CO2, {}).get(CONF_ALTITUDE_COMPENSATION):
        cg.add(
            var.set_altitude_compensation(
                cfg[CONF_ALTITUDE],
            )
        )


SEN6X_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(SEN6XComponent),
    }
)


@automation.register_action(
    "sen6x.start_fan_autoclean", StartFanAction, SEN6X_ACTION_SCHEMA
)
async def sen6x_fan_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


SEN6X_FRC_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(SEN6XComponent),
        cv.Required(CONF_REFERENCE_CO2): cv.templatable(cv.int_range(0, 20000)),
    }
)


@automation.register_action(
    "sen6x.perform_forced_co2_recalibration",
    PerformForcedCO2RecalibrationAction,
    SEN6X_FRC_ACTION_SCHEMA,
)
async def sen6x_frc_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    templ = await cg.templatable(config[CONF_REFERENCE_CO2], args, cg.uint16)
    cg.add(var.set_reference(templ))
    return var


@automation.register_action(
    "sen6x.co2_sensor_factory_reset", CO2SensorFactoryResetAction, SEN6X_ACTION_SCHEMA
)
async def sen6x_co2_reset_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "sen6x.activate_sht_heater", ActivateSHTHeaterAction, SEN6X_ACTION_SCHEMA
)
async def sen6x_sht_heater_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "sen6x.get_sht_heater_measurements",
    GetSHTHeaterMeasurementsAction,
    SEN6X_ACTION_SCHEMA,
)
async def sen6x_sht_heater_measurements_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "sen6x.start_measurement", StartMeasurementAction, SEN6X_ACTION_SCHEMA
)
async def sen6x_start_measurement_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "sen6x.stop_measurement", StopMeasurementAction, SEN6X_ACTION_SCHEMA
)
async def sen6x_stop_measurement_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
