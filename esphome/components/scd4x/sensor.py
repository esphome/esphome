import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.components import sensirion_common
from esphome import automation
from esphome.automation import maybe_simple_id

from esphome.const import (
    CONF_ID,
    CONF_CO2,
    CONF_HUMIDITY,
    CONF_TEMPERATURE,
    CONF_VALUE,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
    ICON_MOLECULE_CO2,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)

CODEOWNERS = ["@sjtrny", "@martgras"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

scd4x_ns = cg.esphome_ns.namespace("scd4x")
SCD4XComponent = scd4x_ns.class_(
    "SCD4XComponent", cg.PollingComponent, sensirion_common.SensirionI2CDevice
)
MeasurementMode = scd4x_ns.enum("MEASUREMENT_MODE")
MEASUREMENT_MODE_OPTIONS = {
    "periodic": MeasurementMode.PERIODIC,
    "low_power_periodic": MeasurementMode.LOW_POWER_PERIODIC,
    "single_shot": MeasurementMode.SINGLE_SHOT,
    "single_shot_rht_only": MeasurementMode.SINGLE_SHOT_RHT_ONLY,
}


# Actions
PerformForcedCalibrationAction = scd4x_ns.class_(
    "PerformForcedCalibrationAction", automation.Action
)
FactoryResetAction = scd4x_ns.class_("FactoryResetAction", automation.Action)


CONF_ALTITUDE_COMPENSATION = "altitude_compensation"
CONF_AMBIENT_PRESSURE_COMPENSATION = "ambient_pressure_compensation"
CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE = "ambient_pressure_compensation_source"
CONF_AUTOMATIC_SELF_CALIBRATION = "automatic_self_calibration"
CONF_MEASUREMENT_MODE = "measurement_mode"
CONF_TEMPERATURE_OFFSET = "temperature_offset"


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SCD4XComponent),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
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
            cv.Optional(CONF_AUTOMATIC_SELF_CALIBRATION, default=True): cv.boolean,
            cv.Optional(CONF_ALTITUDE_COMPENSATION, default="0m"): cv.All(
                cv.float_with_unit("altitude", "(m|m a.s.l.|MAMSL|MASL)"),
                cv.int_range(min=0, max=0xFFFF, max_included=False),
            ),
            cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION): cv.pressure,
            cv.Optional(CONF_TEMPERATURE_OFFSET, default="4°C"): cv.temperature,
            cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE): cv.use_id(
                sensor.Sensor
            ),
            cv.Optional(CONF_MEASUREMENT_MODE, default="periodic"): cv.enum(
                MEASUREMENT_MODE_OPTIONS, lower=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x62))
)

SENSOR_MAP = {
    CONF_CO2: "set_co2_sensor",
    CONF_TEMPERATURE: "set_temperature_sensor",
    CONF_HUMIDITY: "set_humidity_sensor",
}

SETTING_MAP = {
    CONF_AUTOMATIC_SELF_CALIBRATION: "set_automatic_self_calibration",
    CONF_ALTITUDE_COMPENSATION: "set_altitude_compensation",
    CONF_AMBIENT_PRESSURE_COMPENSATION: "set_ambient_pressure_compensation",
    CONF_TEMPERATURE_OFFSET: "set_temperature_offset",
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

    if CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE in config:
        sens = await cg.get_variable(config[CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE])
        cg.add(var.set_ambient_pressure_source(sens))

    cg.add(var.set_measurement_mode(config[CONF_MEASUREMENT_MODE]))


SCD4X_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(SCD4XComponent),
        cv.Required(CONF_VALUE): cv.templatable(cv.positive_int),
    }
)


@automation.register_action(
    "scd4x.perform_forced_calibration",
    PerformForcedCalibrationAction,
    SCD4X_ACTION_SCHEMA,
)
async def scd4x_frc_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_VALUE], args, cg.uint16)
    cg.add(var.set_value(template_))
    return var


SCD4X_RESET_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(SCD4XComponent),
    }
)


@automation.register_action(
    "scd4x.factory_reset", FactoryResetAction, SCD4X_RESET_ACTION_SCHEMA
)
async def scd4x_reset_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
