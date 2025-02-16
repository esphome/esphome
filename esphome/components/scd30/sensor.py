from esphome import automation, core
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.components import sensirion_common
from esphome.const import (
    CONF_ID,
    CONF_HUMIDITY,
    CONF_TEMPERATURE,
    CONF_CO2,
    CONF_TEMPERATURE_OFFSET,
    CONF_UPDATE_INTERVAL,
    CONF_VALUE,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
    ICON_MOLECULE_CO2,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

scd30_ns = cg.esphome_ns.namespace("scd30")
SCD30Component = scd30_ns.class_(
    "SCD30Component", cg.Component, sensirion_common.SensirionI2CDevice
)

# Actions
ForceRecalibrationWithReference = scd30_ns.class_(
    "ForceRecalibrationWithReference", automation.Action
)

CONF_AUTOMATIC_SELF_CALIBRATION = "automatic_self_calibration"
CONF_ALTITUDE_COMPENSATION = "altitude_compensation"
CONF_AMBIENT_PRESSURE_COMPENSATION = "ambient_pressure_compensation"


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SCD30Component),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
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
            cv.Optional(CONF_TEMPERATURE_OFFSET): cv.All(
                cv.temperature,
                cv.float_range(min=0, max=655.35),
            ),
            cv.Optional(CONF_UPDATE_INTERVAL, default="60s"): cv.All(
                cv.positive_time_period_seconds,
                cv.Range(
                    min=core.TimePeriod(seconds=2), max=core.TimePeriod(seconds=1800)
                ),
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x61))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

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

    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))

    if CONF_CO2 in config:
        sens = await sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))

    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))


@automation.register_action(
    "scd30.force_recalibration_with_reference",
    ForceRecalibrationWithReference,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(SCD30Component),
            cv.Required(CONF_VALUE): cv.templatable(
                cv.int_range(min=400, max=2000, max_included=True)
            ),
        },
        key=CONF_VALUE,
    ),
)
async def scd30_force_recalibration_with_reference_to_code(
    config, action_id, template_arg, args
):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_VALUE], args, cg.uint16)
    cg.add(var.set_value(template_))
    return var
