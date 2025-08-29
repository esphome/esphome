from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c, sensirion_common, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALTITUDE_COMPENSATION,
    CONF_AMBIENT_PRESSURE_COMPENSATION,
    CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE,
    CONF_AUTOMATIC_SELF_CALIBRATION,
    CONF_CO2,
    CONF_GAIN_FACTOR,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_MODEL,
    CONF_OFFSET,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_4_0,
    CONF_PM_10_0,
    CONF_STORE_BASELINE,
    CONF_TEMPERATURE,
    CONF_TEMPERATURE_COMPENSATION,
    CONF_VALUE,
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
    UNIT_PARTS_PER_BILLION,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)

CODEOWNERS = ["@martgras", "@mikelawrence"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

sen5x_ns = cg.esphome_ns.namespace("sen5x")
SEN5XComponent = sen5x_ns.class_(
    "SEN5XComponent", cg.PollingComponent, sensirion_common.SensirionI2CDevice
)
Sen5xModel = sen5x_ns.enum("Sen5xModel")
RhtAccelerationMode = sen5x_ns.enum("RhtAccelerationMode")


CONF_ACCELERATION_MODE = "acceleration_mode"
CONF_ALGORITHM_TUNING = "algorithm_tuning"
CONF_AUTO_CLEANING_INTERVAL = "auto_cleaning_interval"
CONF_GATING_MAX_DURATION_MINUTES = "gating_max_duration_minutes"
CONF_HCHO = "hcho"
CONF_INDEX_OFFSET = "index_offset"
CONF_LEARNING_TIME_GAIN_HOURS = "learning_time_gain_hours"
CONF_LEARNING_TIME_OFFSET_HOURS = "learning_time_offset_hours"
CONF_NORMALIZED_OFFSET_SLOPE = "normalized_offset_slope"
CONF_NOX = "nox"
CONF_STD_INITIAL = "std_initial"
CONF_TIME_CONSTANT = "time_constant"
CONF_VOC = "voc"
CONF_VOC_BASELINE = "voc_baseline"
ICON_MOLECULE = "mdi:molecule"

# Actions
StartFanAction = sen5x_ns.class_("StartFanAction", automation.Action)
ActivateHeaterAction = sen5x_ns.class_("ActivateHeaterAction", automation.Action)
PerformForcedCo2CalibrationAction = sen5x_ns.class_(
    "PerformForcedCo2CalibrationAction", automation.Action
)
SetAmbientPressurehPa = sen5x_ns.class_("SetAmbientPressurehPa", automation.Action)

MODEL_SEN50 = "SEN50"
MODEL_SEN54 = "SEN54"
MODEL_SEN55 = "SEN55"
MODEL_SEN60 = "SEN60"
MODEL_SEN63C = "SEN63C"
MODEL_SEN65 = "SEN65"
MODEL_SEN66 = "SEN66"
MODEL_SEN68 = "SEN68"

SEN5X_MODELS = {
    MODEL_SEN50: Sen5xModel.SEN50,
    MODEL_SEN54: Sen5xModel.SEN54,
    MODEL_SEN55: Sen5xModel.SEN55,
    MODEL_SEN60: Sen5xModel.SEN60,
    MODEL_SEN63C: Sen5xModel.SEN63C,
    MODEL_SEN65: Sen5xModel.SEN65,
    MODEL_SEN66: Sen5xModel.SEN66,
    MODEL_SEN68: Sen5xModel.SEN68,
}

ACCELERATION_MODES = {
    "low": RhtAccelerationMode.LOW_ACCELERATION,
    "medium": RhtAccelerationMode.MEDIUM_ACCELERATION,
    "high": RhtAccelerationMode.HIGH_ACCELERATION,
}


def _gas_sensor(
    *,
    index_offset: int,
    learning_time_offset: int,
    learning_time_gain: int,
    gating_max_duration: int,
    std_initial: int,
    gain_factor: int,
) -> cv.Schema:
    return sensor.sensor_schema(
        icon=ICON_RADIATOR,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_AQI,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(
        {
            cv.Optional(CONF_ALGORITHM_TUNING): cv.Schema(
                {
                    cv.Optional(CONF_INDEX_OFFSET, default=index_offset): cv.int_range(
                        1, 250
                    ),
                    cv.Optional(
                        CONF_LEARNING_TIME_OFFSET_HOURS, default=learning_time_offset
                    ): cv.int_range(1, 1000),
                    cv.Optional(
                        CONF_LEARNING_TIME_GAIN_HOURS, default=learning_time_gain
                    ): cv.int_range(1, 1000),
                    cv.Optional(
                        CONF_GATING_MAX_DURATION_MINUTES, default=gating_max_duration
                    ): cv.int_range(0, 3000),
                    cv.Optional(CONF_STD_INITIAL, default=std_initial): cv.int_range(
                        10, 5000
                    ),
                    cv.Optional(CONF_GAIN_FACTOR, default=gain_factor): cv.int_range(
                        1, 1000
                    ),
                }
            )
        }
    )


CO2_SENSOR = cv.Schema(
    {
        cv.Optional(CONF_AUTOMATIC_SELF_CALIBRATION, default=True): cv.boolean,
        cv.Optional(CONF_ALTITUDE_COMPENSATION, default="0m"): cv.All(
            cv.float_with_unit("altitude", "(m|m a.s.l.|MAMSL|MASL)"),
            cv.int_range(min=0, max=0xFFFF, max_included=False),
        ),
        cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE): cv.use_id(
            sensor.Sensor
        ),
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
            cv.GenerateID(): cv.declare_id(SEN5XComponent),
            cv.Required(CONF_MODEL): cv.enum(SEN5X_MODELS, upper=True),
            cv.Optional(CONF_ACCELERATION_MODE): cv.enum(ACCELERATION_MODES),
            cv.Optional(CONF_AUTO_CLEANING_INTERVAL): cv.update_interval,
            cv.Optional(CONF_STORE_BASELINE): cv.boolean,
            cv.Optional(CONF_TEMPERATURE_COMPENSATION): cv.Schema(
                {
                    cv.Optional(CONF_OFFSET, default=0): cv.float_,
                    cv.Optional(CONF_NORMALIZED_OFFSET_SLOPE, default=0): cv.All(
                        float_previously_pct, cv.float_
                    ),
                    cv.Optional(CONF_TIME_CONSTANT, default=0): cv.int_,
                }
            ),
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
            cv.Optional(CONF_VOC): _gas_sensor(
                index_offset=100,
                learning_time_offset=12,
                learning_time_gain=12,
                gating_max_duration=180,
                std_initial=50,
                gain_factor=230,
            ),
            cv.Optional(CONF_NOX): _gas_sensor(
                index_offset=1,
                learning_time_offset=12,
                learning_time_gain=12,
                gating_max_duration=720,
                std_initial=50,
                gain_factor=230,
            ),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(CO2_SENSOR),
            cv.Optional(CONF_HCHO): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                icon=ICON_MOLECULE,
                accuracy_decimals=1,
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
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x69))
)

SENSOR_MAP = {
    CONF_PM_1_0: "set_pm_1_0_sensor",
    CONF_PM_2_5: "set_pm_2_5_sensor",
    CONF_PM_4_0: "set_pm_4_0_sensor",
    CONF_PM_10_0: "set_pm_10_0_sensor",
    CONF_VOC: "set_voc_sensor",
    CONF_NOX: "set_nox_sensor",
    CONF_TEMPERATURE: "set_temperature_sensor",
    CONF_HUMIDITY: "set_humidity_sensor",
    CONF_CO2: "set_co2_sensor",
    CONF_HCHO: "set_hcho_sensor",
}

SETTING_MAP = {
    CONF_MODEL: "set_model",
    CONF_AUTO_CLEANING_INTERVAL: "set_auto_cleaning_interval",
    CONF_ACCELERATION_MODE: "set_acceleration_mode",
}

CO2_SETTING_MAP = {
    CONF_AUTOMATIC_SELF_CALIBRATION: "set_co2_auto_calibrate",
    CONF_ALTITUDE_COMPENSATION: "set_co2_altitude_compensation",
    CONF_AMBIENT_PRESSURE_COMPENSATION: "set_co2_ambient_pressure_compensation",
}


def final_validate(config):
    model = config[CONF_MODEL]
    if CONF_ACCELERATION_MODE in config and model in {
        MODEL_SEN50,
        MODEL_SEN60,
        MODEL_SEN63C,
        MODEL_SEN65,
        MODEL_SEN66,
        MODEL_SEN68,
    }:
        raise cv.Invalid(f"Model {model} does not support '{CONF_ACCELERATION_MODE}'.")
    if CONF_AUTO_CLEANING_INTERVAL in config and model in {
        MODEL_SEN60,
        MODEL_SEN63C,
        MODEL_SEN65,
        MODEL_SEN66,
        MODEL_SEN68,
    }:
        raise cv.Invalid(
            f"'Model' {model} does not support '{CONF_AUTO_CLEANING_INTERVAL}'."
        )
    if CONF_STORE_BASELINE in config and model in {
        MODEL_SEN50,
        MODEL_SEN60,
        MODEL_SEN63C,
    }:
        raise cv.Invalid(f"Model {model} does not support '{CONF_STORE_BASELINE}'.")
    if CONF_TEMPERATURE_COMPENSATION in config and model in {
        MODEL_SEN50,
        MODEL_SEN60,
        MODEL_SEN63C,
        MODEL_SEN65,
        MODEL_SEN66,
        MODEL_SEN68,
    }:
        raise cv.Invalid(
            f"Model {model} does not support '{CONF_TEMPERATURE_COMPENSATION}'."
        )
    if CONF_VOC in config and model in {MODEL_SEN50, MODEL_SEN60, MODEL_SEN63C}:
        raise cv.Invalid(f"Model {model} does not support '{CONF_VOC}'.")
    if CONF_NOX in config and model in {
        MODEL_SEN50,
        MODEL_SEN54,
        MODEL_SEN60,
        MODEL_SEN63C,
    }:
        raise cv.Invalid(f"Model {model} does not support '{CONF_NOX}'.")
    if CONF_CO2 in config and model in {
        MODEL_SEN50,
        MODEL_SEN54,
        MODEL_SEN55,
        MODEL_SEN60,
        MODEL_SEN65,
        MODEL_SEN68,
    }:
        raise cv.Invalid(f"Model {model} does not support '{CONF_CO2}'.")
    if CONF_HCHO in config and model in {
        MODEL_SEN50,
        MODEL_SEN54,
        MODEL_SEN55,
        MODEL_SEN60,
        MODEL_SEN63C,
        MODEL_SEN65,
        MODEL_SEN66,
    }:
        raise cv.Invalid(f"Model {model} does not support '{CONF_HCHO}'.")
    if CONF_TEMPERATURE in config and model in {MODEL_SEN50, MODEL_SEN60}:
        raise cv.Invalid(f"Model {model} does not support '{CONF_TEMPERATURE}'.")
    if CONF_HUMIDITY in config and model in {MODEL_SEN50, MODEL_SEN60}:
        raise cv.Invalid(f"Model {model} does not support '{CONF_HUMIDITY}'.")


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    for key, funcName in SETTING_MAP.items():
        if cfg := config.get(key):
            cg.add(getattr(var, funcName)(cfg))

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
            )
        )
    if CONF_CO2 in config:
        for key, funcName in CO2_SETTING_MAP.items():
            if key in config[CONF_CO2]:
                cg.add(getattr(var, funcName)(config[CONF_CO2][key]))
            if CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE in config[CONF_CO2]:
                sens = await cg.get_variable(
                    config[CONF_CO2][CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE]
                )
                cg.add(var.set_ambient_pressure_source(sens))


SEN5X_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(SEN5XComponent),
    }
)


@automation.register_action(
    "sen5x.start_fan_autoclean", StartFanAction, SEN5X_ACTION_SCHEMA
)
async def sen5x_fan_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "sen5x.activate_heater", ActivateHeaterAction, SEN5X_ACTION_SCHEMA
)
async def sen5x_ah_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


SEN5X_VALUE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(SEN5XComponent),
        cv.Required(CONF_VALUE): cv.templatable(cv.positive_int),
    }
)


@automation.register_action(
    "sen5x.perform_forced_co2_calibration",
    PerformForcedCo2CalibrationAction,
    SEN5X_VALUE_ACTION_SCHEMA,
)
async def sen5x_pfcc_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_VALUE], args, cg.uint16)
    cg.add(var.set_value(template_))
    return var


SEN5X_PRESSURE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(SEN5XComponent),
        cv.Required(CONF_VALUE): cv.templatable(cv.positive_int),
    }
)


@automation.register_action(
    "sen5x.set_ambient_pressure_hpa",
    SetAmbientPressurehPa,
    SEN5X_VALUE_ACTION_SCHEMA,
)
async def sen5x_saph_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_VALUE], args, cg.uint16)
    cg.add(var.set_value(template_))
    return var
