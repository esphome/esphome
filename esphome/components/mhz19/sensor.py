from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_CO2,
    CONF_ID,
    CONF_TEMPERATURE,
    CONF_WARMUP_TIME,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_MOLECULE_CO2,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PARTS_PER_MILLION,
)
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]

CONF_AUTOMATIC_BASELINE_CALIBRATION = "automatic_baseline_calibration"
CONF_DETECTION_RANGE = "detection_range"

mhz19_ns = cg.esphome_ns.namespace("mhz19")
MHZ19Component = mhz19_ns.class_("MHZ19Component", cg.PollingComponent, uart.UARTDevice)
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

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "mhz19",
    baud_rate=9600,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
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


for _name, _call in (
    ("mhz19.calibrate_zero", "calibrate_zero()"),
    ("mhz19.abc_enable", "abc_enable()"),
    ("mhz19.abc_disable", "abc_disable()"),
):
    automation.register_apply_action(
        _name, NO_ARGS_ACTION_SCHEMA, automation.ApplyCall(_call)
    )


RANGE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(MHZ19Component),
        cv.Required(CONF_DETECTION_RANGE): cv.All(
            _validate_ppm, cv.enum(MHZ19_DETECTION_RANGE_ENUM)
        ),
    }
)


automation.register_apply_action(
    "mhz19.detection_range_set",
    RANGE_ACTION_SCHEMA,
    automation.ApplyField(CONF_DETECTION_RANGE, "range_set", mhz19_detection_range),
)
