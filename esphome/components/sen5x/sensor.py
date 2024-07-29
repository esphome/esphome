import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor, sensirion_common
from esphome import automation
from esphome.automation import maybe_simple_id

from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    CONF_OFFSET,
    CONF_PM_1_0,
    CONF_PM_10_0,
    CONF_PM_2_5,
    CONF_PM_4_0,
    CONF_STORE_BASELINE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_TEMPERATURE,
    ICON_CHEMICAL_WEAPON,
    ICON_RADIATOR,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_PERCENT,
)

CODEOWNERS = ["@martgras"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

sen5x_ns = cg.esphome_ns.namespace("sen5x")
SEN5XComponent = sen5x_ns.class_(
    "SEN5XComponent", cg.PollingComponent, sensirion_common.SensirionI2CDevice
)
RhtAccelerationMode = sen5x_ns.enum("RhtAccelerationMode")

CONF_ACCELERATION_MODE = "acceleration_mode"
CONF_ALGORITHM_TUNING = "algorithm_tuning"
CONF_AUTO_CLEANING_INTERVAL = "auto_cleaning_interval"
CONF_GAIN_FACTOR = "gain_factor"
CONF_GATING_MAX_DURATION_MINUTES = "gating_max_duration_minutes"
CONF_INDEX_OFFSET = "index_offset"
CONF_LEARNING_TIME_GAIN_HOURS = "learning_time_gain_hours"
CONF_LEARNING_TIME_OFFSET_HOURS = "learning_time_offset_hours"
CONF_NORMALIZED_OFFSET_SLOPE = "normalized_offset_slope"
CONF_NOX = "nox"
CONF_STD_INITIAL = "std_initial"
CONF_TEMPERATURE_COMPENSATION = "temperature_compensation"
CONF_TIME_CONSTANT = "time_constant"
CONF_VOC = "voc"
CONF_VOC_BASELINE = "voc_baseline"


# Actions
StartFanAction = sen5x_ns.class_("StartFanAction", automation.Action)

ACCELERATION_MODES = {
    "low": RhtAccelerationMode.LOW_ACCELERATION,
    "medium": RhtAccelerationMode.MEDIUM_ACCELERATION,
    "high": RhtAccelerationMode.HIGH_ACCELERATION,
}

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
            cv.GenerateID(): cv.declare_id(SEN5XComponent),
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
            cv.Optional(CONF_AUTO_CLEANING_INTERVAL): cv.update_interval,
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
                    cv.Optional(CONF_OFFSET, default=0): cv.float_,
                    cv.Optional(CONF_NORMALIZED_OFFSET_SLOPE, default=0): cv.All(
                        float_previously_pct, cv.float_
                    ),
                    cv.Optional(CONF_TIME_CONSTANT, default=0): cv.int_,
                }
            ),
            cv.Optional(CONF_ACCELERATION_MODE): cv.enum(ACCELERATION_MODES),
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
}

SETTING_MAP = {
    CONF_AUTO_CLEANING_INTERVAL: "set_auto_cleaning_interval",
    CONF_ACCELERATION_MODE: "set_acceleration_mode",
}


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    for key, funcName in SETTING_MAP.items():
        if key in config:
            cg.add(getattr(var, funcName)(config[key]))

    for key, funcName in SENSOR_MAP.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, funcName)(sens))

    if CONF_VOC in config and CONF_ALGORITHM_TUNING in config[CONF_VOC]:
        cfg = config[CONF_VOC][CONF_ALGORITHM_TUNING]
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
    if CONF_NOX in config and CONF_ALGORITHM_TUNING in config[CONF_NOX]:
        cfg = config[CONF_NOX][CONF_ALGORITHM_TUNING]
        cg.add(
            var.set_nox_algorithm_tuning(
                cfg[CONF_INDEX_OFFSET],
                cfg[CONF_LEARNING_TIME_OFFSET_HOURS],
                cfg[CONF_LEARNING_TIME_GAIN_HOURS],
                cfg[CONF_GATING_MAX_DURATION_MINUTES],
                cfg[CONF_GAIN_FACTOR],
            )
        )
    if CONF_TEMPERATURE_COMPENSATION in config:
        cg.add(
            var.set_temperature_compensation(
                config[CONF_TEMPERATURE_COMPENSATION][CONF_OFFSET],
                config[CONF_TEMPERATURE_COMPENSATION][CONF_NORMALIZED_OFFSET_SLOPE],
                config[CONF_TEMPERATURE_COMPENSATION][CONF_TIME_CONSTANT],
            )
        )


SEN5X_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(SEN5XComponent),
    }
)


@automation.register_action(
    "sen5x.start_fan_autoclean", StartFanAction, SEN5X_ACTION_SCHEMA
)
async def sen54_fan_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
