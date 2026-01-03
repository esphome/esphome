from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_CO2,
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_MOLECULE_CO2,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PARTS_PER_MILLION,
)

DEPENDENCIES = ["uart"]

CONF_AUTOMATIC_BASELINE_CALIBRATION = "automatic_baseline_calibration"
CONF_WARMUP_TIME = "warmup_time"
CONF_DETECTION_RANGE = "detection_range"

mhz19_ns = cg.esphome_ns.namespace("mhz19")
MHZ19Component = mhz19_ns.class_("MHZ19Component", cg.PollingComponent, uart.UARTDevice)
MHZ19CalibrateZeroAction = mhz19_ns.class_(
    "MHZ19CalibrateZeroAction", automation.Action, cg.Parented.template(MHZ19Component)
)
MHZ19ABCEnableAction = mhz19_ns.class_(
    "MHZ19ABCEnableAction", automation.Action, cg.Parented.template(MHZ19Component)
)
MHZ19ABCDisableAction = mhz19_ns.class_(
    "MHZ19ABCDisableAction", automation.Action, cg.Parented.template(MHZ19Component)
)
MHZ19DetectionRangeSetAction = mhz19_ns.class_(
    "MHZ19DetectionRangeSetAction",
    automation.Action,
    cg.Parented.template(MHZ19Component),
)

mhz19_detection_range = mhz19_ns.enum("MHZ19DetectionRange")
MHZ19_DETECTION_RANGE_ENUM = {
    2000: mhz19_detection_range.MHZ19_DETECTION_RANGE_0_2000PPM,
    5000: mhz19_detection_range.MHZ19_DETECTION_RANGE_0_5000PPM,
    10000: mhz19_detection_range.MHZ19_DETECTION_RANGE_0_10000PPM,
}

_validate_ppm = cv.float_with_unit("parts per million", "ppm")

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MHZ19Component),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AUTOMATIC_BASELINE_CALIBRATION): cv.boolean,
            cv.Optional(
                CONF_WARMUP_TIME, default="75s"
            ): cv.positive_time_period_seconds,
            cv.Optional(CONF_DETECTION_RANGE): cv.All(
                _validate_ppm, cv.enum(MHZ19_DETECTION_RANGE_ENUM)
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if co2 := config.get(CONF_CO2):
        sens = await sensor.new_sensor(co2)
        cg.add(var.set_co2_sensor(sens))

    if temperature := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature)
        cg.add(var.set_temperature_sensor(sens))

    if (
        automatic_baseline_calibration := config.get(
            CONF_AUTOMATIC_BASELINE_CALIBRATION
        )
    ) is not None:
        cg.add(var.set_abc_enabled(automatic_baseline_calibration))

    cg.add(var.set_warmup_seconds(config[CONF_WARMUP_TIME]))

    if CONF_DETECTION_RANGE in config:
        cg.add(var.set_detection_range(config[CONF_DETECTION_RANGE]))


NO_ARGS_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(MHZ19Component),
    }
)


@automation.register_action(
    "mhz19.calibrate_zero", MHZ19CalibrateZeroAction, NO_ARGS_ACTION_SCHEMA
)
@automation.register_action(
    "mhz19.abc_enable", MHZ19ABCEnableAction, NO_ARGS_ACTION_SCHEMA
)
@automation.register_action(
    "mhz19.abc_disable", MHZ19ABCDisableAction, NO_ARGS_ACTION_SCHEMA
)
async def mhz19_no_args_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


RANGE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(MHZ19Component),
        cv.Required(CONF_DETECTION_RANGE): cv.All(
            _validate_ppm, cv.enum(MHZ19_DETECTION_RANGE_ENUM)
        ),
    }
)


@automation.register_action(
    "mhz19.detection_range_set", MHZ19DetectionRangeSetAction, RANGE_ACTION_SCHEMA
)
async def mhz19_detection_range_set_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    detection_range = config.get(CONF_DETECTION_RANGE)
    template_ = await cg.templatable(detection_range, args, mhz19_detection_range)
    cg.add(var.set_detection_range(template_))
    return var
