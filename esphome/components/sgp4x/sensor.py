import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor, sensirion_common
from esphome.const import (
    CONF_COMPENSATION,
    CONF_ID,
    CONF_STORE_BASELINE,
    CONF_TEMPERATURE_SOURCE,
    ICON_RADIATOR,
    DEVICE_CLASS_AQI,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]
CODEOWNERS = ["@SenexCrenshaw", "@martgras"]

sgp4x_ns = cg.esphome_ns.namespace("sgp4x")
SGP4xComponent = sgp4x_ns.class_(
    "SGP4xComponent",
    sensor.Sensor,
    cg.PollingComponent,
    sensirion_common.SensirionI2CDevice,
)

CONF_ALGORITHM_TUNING = "algorithm_tuning"
CONF_GAIN_FACTOR = "gain_factor"
CONF_GATING_MAX_DURATION_MINUTES = "gating_max_duration_minutes"
CONF_HUMIDITY_SOURCE = "humidity_source"
CONF_INDEX_OFFSET = "index_offset"
CONF_LEARNING_TIME_GAIN_HOURS = "learning_time_gain_hours"
CONF_LEARNING_TIME_OFFSET_HOURS = "learning_time_offset_hours"
CONF_NOX = "nox"
CONF_STD_INITIAL = "std_initial"
CONF_VOC = "voc"
CONF_VOC_BASELINE = "voc_baseline"


def validate_sensors(config):
    if CONF_VOC not in config and CONF_NOX not in config:
        raise cv.Invalid(
            f"At least one sensor is required. Define {CONF_VOC} and/or {CONF_NOX}"
        )
    return config


GAS_SENSOR = cv.Schema(
    {
        cv.Optional(CONF_ALGORITHM_TUNING): cv.Schema(
            {
                cv.Optional(CONF_INDEX_OFFSET, default=100): cv.int_,
                cv.Optional(CONF_LEARNING_TIME_OFFSET_HOURS, default=12): cv.int_,
                cv.Optional(CONF_LEARNING_TIME_GAIN_HOURS, default=12): cv.int_,
                cv.Optional(CONF_GATING_MAX_DURATION_MINUTES, default=720): cv.int_,
                cv.Optional(CONF_STD_INITIAL, default=50): cv.int_,
                cv.Optional(CONF_GAIN_FACTOR, default=230): cv.int_,
            }
        )
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SGP4xComponent),
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
            cv.Optional(CONF_COMPENSATION): cv.Schema(
                {
                    cv.Required(CONF_HUMIDITY_SOURCE): cv.use_id(sensor.Sensor),
                    cv.Required(CONF_TEMPERATURE_SOURCE): cv.use_id(sensor.Sensor),
                },
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x59)),
    validate_sensors,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_COMPENSATION in config:
        compensation_config = config[CONF_COMPENSATION]
        sens = await cg.get_variable(compensation_config[CONF_HUMIDITY_SOURCE])
        cg.add(var.set_humidity_sensor(sens))
        sens = await cg.get_variable(compensation_config[CONF_TEMPERATURE_SOURCE])
        cg.add(var.set_temperature_sensor(sens))

    cg.add(var.set_store_baseline(config[CONF_STORE_BASELINE]))

    if CONF_VOC_BASELINE in config:
        cg.add(var.set_voc_baseline(CONF_VOC_BASELINE))

    if CONF_VOC in config:
        sens = await sensor.new_sensor(config[CONF_VOC])
        cg.add(var.set_voc_sensor(sens))
        if CONF_ALGORITHM_TUNING in config[CONF_VOC]:
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

    if CONF_NOX in config:
        sens = await sensor.new_sensor(config[CONF_NOX])
        cg.add(var.set_nox_sensor(sens))
        if CONF_ALGORITHM_TUNING in config[CONF_NOX]:
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
    cg.add_library(
        None,
        None,
        "https://github.com/Sensirion/arduino-gas-index-algorithm.git#3.2.1",
    )
