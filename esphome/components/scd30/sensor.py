import re
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_EMPTY,
    CONF_HUMIDITY,
    CONF_TEMPERATURE,
    CONF_CO2,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    ICON_EMPTY,
    UNIT_PARTS_PER_MILLION,
    ICON_MOLECULE_CO2,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)

DEPENDENCIES = ["i2c"]

scd30_ns = cg.esphome_ns.namespace("scd30")
SCD30Component = scd30_ns.class_("SCD30Component", cg.PollingComponent, i2c.I2CDevice)

CONF_AUTOMATIC_SELF_CALIBRATION = "automatic_self_calibration"
CONF_ALTITUDE_COMPENSATION = "altitude_compensation"
CONF_AMBIENT_PRESSURE_COMPENSATION = "ambient_pressure_compensation"
CONF_FORCED_RECALIBRATION_VALUE = "forced_recalibration_value"
CONF_TEMPERATURE_OFFSET = "temperature_offset"


def remove_altitude_suffix(value):
    return re.sub(r"\s*(?:m(?:\s+a\.s\.l)?)|(?:MAM?SL)$", "", value)


SetForcedRecalibrationValueAction = scd30_ns.class_(
    "SetForcedRecalibrationValueAction", automation.Action
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SCD30Component),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                UNIT_PARTS_PER_MILLION, ICON_MOLECULE_CO2, 0, DEVICE_CLASS_EMPTY
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                UNIT_CELSIUS, ICON_EMPTY, 1, DEVICE_CLASS_TEMPERATURE
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                UNIT_PERCENT, ICON_EMPTY, 1, DEVICE_CLASS_HUMIDITY
            ),
            cv.Optional(CONF_AUTOMATIC_SELF_CALIBRATION, default=True): cv.boolean,
            cv.Optional(CONF_ALTITUDE_COMPENSATION): cv.All(
                remove_altitude_suffix,
                cv.int_range(min=0, max=0xFFFF, max_included=False),
            ),
            cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION, default=0): cv.pressure,
            cv.Optional(CONF_TEMPERATURE_OFFSET): cv.temperature,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x61))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    cg.add(var.set_automatic_self_calibration(config[CONF_AUTOMATIC_SELF_CALIBRATION]))
    if CONF_ALTITUDE_COMPENSATION in config:
        cg.add(var.set_altitude_compensation(config[CONF_ALTITUDE_COMPENSATION]))

    if CONF_AMBIENT_PRESSURE_COMPENSATION in config:
        cg.add(
            var.set_ambient_pressure_compensation(
                config[CONF_AMBIENT_PRESSURE_COMPENSATION]
            )
        )

    if CONF_TEMPERATURE_OFFSET in config:
        cg.add(var.set_temperature_offset(config[CONF_TEMPERATURE_OFFSET]))

    if CONF_CO2 in config:
        sens = yield sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))

    if CONF_HUMIDITY in config:
        sens = yield sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))


@automation.register_action(
    "scd30.set_forced_recalibration_value",
    SetForcedRecalibrationValueAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(SCD30Component),
            cv.Required(CONF_FORCED_RECALIBRATION_VALUE): cv.templatable(
                cv.int_range(min=0, max=0xFFFF, max_included=False)
            ),
        }
    ),
)
def set_forced_recalibration_value_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_FORCED_RECALIBRATION_VALUE], args, int)
    cg.add(var.set_forced_recalibration_value(template_))
    yield var
