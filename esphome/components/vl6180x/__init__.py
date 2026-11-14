CODEOWNERS = ["@NonaSuomy"]

"""ESPHome external component configuration schema for VL6180X / VL6180."""

from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, sensor, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_ENABLE_PIN,
    CONF_GAIN,
    CONF_ID,
    CONF_INTERRUPT_PIN,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_RANGE,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_ILLUMINANCE,
    ICON_BRIGHTNESS_5,
    ICON_RULER,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
    UNIT_MILLIMETER,
)
from esphome.types import ConfigType

AUTO_LOAD = ["sensor", "text_sensor"]
DEPENDENCIES = ["i2c"]
DOMAIN = "vl6180x"

vl6180x_ns = cg.esphome_ns.namespace("vl6180x")
VL6180XComponent = vl6180x_ns.class_(
    "VL6180XComponent", cg.PollingComponent, i2c.I2CDevice
)
_request_instance_slot = cg.slot_counter("VL6180X_INSTANCE_COUNT")

CONF_SCALER = "scaler"
CONF_RANGE_STATUS = "range_status"
CONF_LUX = "lux"
CONF_RECOVERY = "recovery"
CONF_FAILURE_THRESHOLD = "failure_threshold"
CONF_RETRY_INTERVAL = "retry_interval"
CONF_MAX_RETRIES = "max_retries"

ALS_GAINS = {
    1: 6,
    1.25: 5,
    1.67: 4,
    2.5: 3,
    5: 2,
    10: 1,
    20: 0,
    40: 7,
    "1x": 6,
    "1.25x": 5,
    "1.67x": 4,
    "2.5x": 3,
    "5x": 2,
    "10x": 1,
    "20x": 0,
    "40x": 7,
}
ALS_GAIN_FACTORS = {6: 1.0, 5: 1.25, 4: 1.67, 3: 2.5, 2: 5.0, 1: 10.0, 0: 20.0, 7: 40.0}


RANGE_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_MILLIMETER,
    icon=ICON_RULER,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_DISTANCE,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(
    {
        cv.Optional(CONF_SCALER, default=1): cv.one_of(1, 2, 3, int=True),
    }
)

RANGE_STATUS_SCHEMA = text_sensor.text_sensor_schema(
    icon="mdi:map-marker-distance",
)

LUX_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_LUX,
    icon=ICON_BRIGHTNESS_5,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_ILLUMINANCE,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(
    {
        cv.Optional(CONF_GAIN, default=1): cv.enum(ALS_GAINS),
        cv.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
    }
)

RECOVERY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_FAILURE_THRESHOLD, default=3): cv.int_range(min=1, max=255),
        cv.Optional(
            CONF_RETRY_INTERVAL, default="30s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_RETRIES, default=10): cv.int_range(min=1, max=255),
    }
)

_ENTRY_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VL6180XComponent),
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_RANGE): RANGE_SENSOR_SCHEMA,
            cv.Optional(CONF_RANGE_STATUS): RANGE_STATUS_SCHEMA,
            cv.Optional(CONF_LUX): LUX_SENSOR_SCHEMA,
            cv.Optional(CONF_RECOVERY): RECOVERY_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x29))
)


def _validate(configs: list[ConfigType]) -> list[ConfigType]:
    for i, entry in enumerate(configs):
        if entry[CONF_ADDRESS] != 0x29 and CONF_ENABLE_PIN not in entry:
            raise cv.Invalid(
                "enable_pin is required when changing the sensor's default I2C "
                "address so the sensor can be reset and reassigned reliably",
                path=[i, CONF_ENABLE_PIN],
            )

    if len(configs) > 1:
        enable_pins = {}
        for i, entry in enumerate(configs):
            if CONF_ENABLE_PIN not in entry:
                raise cv.Invalid(
                    f"enable_pin is required on every vl6180x entry when more than one "
                    f"sensor is configured (missing on entry {i})",
                    path=[i, CONF_ENABLE_PIN],
                )
            pin_config = entry[CONF_ENABLE_PIN]
            pin_number = pin_config[CONF_NUMBER]
            pin_provider = repr(
                {
                    key: value
                    for key, value in pin_config.items()
                    if key not in (CONF_NUMBER, CONF_MODE, CONF_INVERTED)
                }
            )
            pin_identity = (pin_provider, pin_number)
            if pin_identity in enable_pins:
                raise cv.Invalid(
                    f"enable_pin GPIO{pin_number} is shared by entries "
                    f"{enable_pins[pin_identity]} and {i}; each sensor needs its "
                    f"own XSHUT/CE pin so addresses can be assigned one at a time",
                    path=[i, CONF_ENABLE_PIN],
                )
            enable_pins[pin_identity] = i
    addrs = [entry[CONF_ADDRESS] for entry in configs]
    if len(addrs) != len(set(addrs)):
        seen = set()
        for i, addr in enumerate(addrs):
            if addr in seen:
                raise cv.Invalid(
                    f"Duplicate I2C address 0x{addr:02X} on entry {i} — "
                    f"each vl6180x sensor must have a unique address",
                    path=[i, CONF_ADDRESS],
                )
            seen.add(addr)

    return configs


CONFIG_SCHEMA = cv.All(cv.ensure_list(_ENTRY_SCHEMA), _validate)


async def to_code(config: list[ConfigType]) -> None:
    for entry in config:
        var = cg.new_Pvariable(entry[CONF_ID])
        await cg.register_component(var, entry)
        await i2c.register_i2c_device(var, entry)

        cg.add(var.set_name_str(entry[CONF_ID].id))

        _request_instance_slot()
        cg.add(var.register_instance(var))

        if CONF_ENABLE_PIN in entry:
            pin = await cg.gpio_pin_expression(entry[CONF_ENABLE_PIN])
            cg.add(var.set_enable_pin(pin))

        if CONF_INTERRUPT_PIN in entry:
            pin = await cg.gpio_pin_expression(entry[CONF_INTERRUPT_PIN])
            cg.add(var.set_interrupt_pin(pin))

        if CONF_RANGE in entry:
            sens = await sensor.new_sensor(entry[CONF_RANGE])
            cg.add(var.set_range_sensor(sens))
            cg.add(var.set_scaling(entry[CONF_RANGE][CONF_SCALER]))

        if CONF_RANGE_STATUS in entry:
            sens = await text_sensor.new_text_sensor(entry[CONF_RANGE_STATUS])
            cg.add(var.set_range_status_sensor(sens))

        if CONF_LUX in entry:
            lux_conf = entry[CONF_LUX]
            sens = await sensor.new_sensor(lux_conf)
            cg.add(var.set_lux_sensor(sens))
            gain = lux_conf[CONF_GAIN]
            gain_code = gain.enum_value
            cg.add(var.set_als_gain(gain_code, ALS_GAIN_FACTORS[gain_code]))
            if CONF_UPDATE_INTERVAL in lux_conf:
                cg.add(var.set_lux_update_interval(lux_conf[CONF_UPDATE_INTERVAL]))

        if CONF_RECOVERY in entry:
            rec = entry[CONF_RECOVERY]
            cg.add(var.set_failure_threshold(rec[CONF_FAILURE_THRESHOLD]))
            cg.add(var.set_retry_interval(rec[CONF_RETRY_INTERVAL]))
            cg.add(var.set_max_retries(rec[CONF_MAX_RETRIES]))
