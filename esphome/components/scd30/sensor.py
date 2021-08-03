import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.const import (
    CONF_ID,
    CONF_HUMIDITY,
    CONF_TEMPERATURE,
    CONF_CO2,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
    ICON_MOLECULE_CO2,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)

DEPENDENCIES = ["i2c"]

scd30_ns = cg.esphome_ns.namespace("scd30")
SCD30Component = scd30_ns.class_("SCD30Component", cg.PollingComponent, i2c.I2CDevice)
SCD30ForcedRecalibrationAction = scd30_ns.class_(
    "SCD30ForcedRecalibrationAction", automation.Action
)
SCD30ASCEnableAction = scd30_ns.class_("SCD30ASCEnableAction", automation.Action)
SCD30ASCDisableAction = scd30_ns.class_("SCD30ASCDisableAction", automation.Action)

CONF_AUTOMATIC_SELF_CALIBRATION = "automatic_self_calibration"
CONF_ALTITUDE_COMPENSATION = "altitude_compensation"
CONF_AMBIENT_PRESSURE_COMPENSATION = "ambient_pressure_compensation"
CONF_TEMPERATURE_OFFSET = "temperature_offset"
CONF_FORCED_CALIBRATION_BASELINE = "forced_calibration_baseline"


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SCD30Component),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
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
            cv.Optional(CONF_AUTOMATIC_SELF_CALIBRATION, default=True): cv.boolean,
            cv.Optional(CONF_ALTITUDE_COMPENSATION): cv.All(
                cv.float_with_unit("altitude", "(m|m a.s.l.|MAMSL|MASL)"),
                cv.int_range(min=0, max=0xFFFF, max_included=False),
            ),
            cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION, default=0): cv.pressure,
            cv.Optional(CONF_TEMPERATURE_OFFSET): cv.temperature,
            cv.Optional(CONF_FORCED_CALIBRATION_BASELINE): cv.int_range(
                min=400, max=2000, max_included=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x61))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_automatic_self_calibration(config[CONF_AUTOMATIC_SELF_CALIBRATION]))
    if CONF_ALTITUDE_COMPENSATION in config:
        cg.add(var.set_altitude_compensation(config[CONF_ALTITUDE_COMPENSATION]))

    if CONF_FORCED_CALIBRATION_BASELINE in config:
        cg.add(
            var.set_forced_calibration_baseline(
                config[CONF_FORCED_CALIBRATION_BASELINE]
            )
        )

    if CONF_AMBIENT_PRESSURE_COMPENSATION in config:
        cg.add(
            var.set_ambient_pressure_compensation(
                config[CONF_AMBIENT_PRESSURE_COMPENSATION]
            )
        )

    if CONF_TEMPERATURE_OFFSET in config:
        cg.add(var.set_temperature_offset(config[CONF_TEMPERATURE_OFFSET]))

    if CONF_CO2 in config:
        sens = await sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))

    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))


CALIBRATION_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(SCD30Component),
    }
)


@automation.register_action(
    "scd30.start_forced_recalibration",
    SCD30ForcedRecalibrationAction,
    CALIBRATION_ACTION_SCHEMA,
)
@automation.register_action(
    "scd30.automatic_self_calibration_enable",
    SCD30ASCEnableAction,
    CALIBRATION_ACTION_SCHEMA,
)
@automation.register_action(
    "scd30.automatic_self_calibration_disable",
    SCD30ASCDisableAction,
    CALIBRATION_ACTION_SCHEMA,
)
async def scd30_calibration_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    await cg.new_Pvariable(action_id, template_arg, paren)
