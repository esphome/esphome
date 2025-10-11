from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_ENABLE_PIN,
    CONF_INTERRUPT_PIN,
    CONF_OFFSET,
    CONF_UPDATE_INTERVAL,
    CONF_X,
    CONF_Y,
    DEVICE_CLASS_DISTANCE,
    ICON_ARROW_EXPAND_VERTICAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
)

DEPENDENCIES = ["i2c"]

vl53l1x_ns = cg.esphome_ns.namespace("vl53l1x")
VL53L1xSensor = vl53l1x_ns.class_(
    "VL53L1xSensor", sensor.Sensor, cg.Component, i2c.I2CDevice
)

CONF_TIMING_BUDGET = "timing_budget"
CONF_DISTANCE_MODE = "distance_mode"
CONF_XTALK_CORRECTION = "xtalk_correction"
CONF_DISTANCE_THRESHOLD = "distance_threshold"
CONF_MIN = "min"
CONF_MAX = "max"
CONF_INTERRUPT_WHEN = "interrupt_when"
CONF_REGION_OF_INTEREST = "region_of_interest"
CONF_W = "w"
CONF_H = "h"
CONF_SIGNAL_THRESHOLD = "signal_threshold"
CONF_SIGMA_THRESHOLD = "sigma_threshold"

DISTANCE_MODE_ENUM = vl53l1x_ns.enum("DistanceMode")
INTERRUPT_WHEN_MODE = vl53l1x_ns.enum("InterruptWhenMode")
DISTANCE_MODE = {
    "short": DISTANCE_MODE_ENUM.SHORT,
    "long": DISTANCE_MODE_ENUM.LONG,
}

TIMING_BUDGET = {
    "15ms": 15,
    "20ms": 20,
    "33ms": 33,
    "50ms": 50,
    "100ms": 100,
    "200ms": 200,
    "500ms": 500,
}

INTERRUPT_WHEN = {
    "below_min": INTERRUPT_WHEN_MODE.BELOW_MIN,
    "above_max": INTERRUPT_WHEN_MODE.ABOVE_MAX,
    "outside_window": INTERRUPT_WHEN_MODE.OUTSIDE_WINDOW,
    "inside_window": INTERRUPT_WHEN_MODE.INSIDE_WINDOW,
}


def check_keys(obj):
    if obj[CONF_ADDRESS] != 0x29 and CONF_ENABLE_PIN not in obj:
        msg = "Address other then 0x29 requires enable_pin definition to allow sensor\r"
        msg += (
            "re-addressing. Also if you have more then one VL53L1x device on the same\r"
        )
        msg += "i2c bus, then all VL53 devices must have enable_pin defined."
        raise cv.Invalid(msg)

    if obj[CONF_DISTANCE_MODE] == "long" and obj[CONF_TIMING_BUDGET] == "15ms":
        msg = "When 'distance_mode' = long) the sensor requires a timing budget of at least 20ms"
        raise cv.Invalid(msg)

    if cv.time_period(obj[CONF_UPDATE_INTERVAL]) < cv.time_period(
        obj[CONF_TIMING_BUDGET]
    ):
        raise cv.Invalid(
            "The update interval has to be at least as long the timing budget."
        )

    if CONF_REGION_OF_INTEREST in obj and (
        obj[CONF_REGION_OF_INTEREST][CONF_X] + obj[CONF_REGION_OF_INTEREST][CONF_W] > 16
        or obj[CONF_REGION_OF_INTEREST][CONF_Y] + obj[CONF_REGION_OF_INTEREST][CONF_H]
        > 16
    ):
        msg = "Region of interest coordinates cannot exceed 16 in either axis."
        raise cv.Invalid(msg)

    if CONF_DISTANCE_THRESHOLD in obj:
        if CONF_INTERRUPT_PIN not in obj:
            raise cv.Invalid(
                "'distance threshold' is only supported in interrupt mode. Configure 'interrupt_pin' to enable interrupt mode."
            )

        threshold_obj = obj[CONF_DISTANCE_THRESHOLD]
        if (
            CONF_MIN in threshold_obj
            and CONF_MAX in threshold_obj
            and to_uint16_mm(threshold_obj[CONF_MIN])
            >= to_uint16_mm(threshold_obj[CONF_MAX])
        ):
            raise cv.Invalid(
                "min must be less than max", [CONF_DISTANCE_THRESHOLD, CONF_MIN]
            )
        if (
            threshold_obj[CONF_INTERRUPT_WHEN]
            in ("below_min", "outside_window", "inside_window")
            and CONF_MIN not in threshold_obj
        ):
            raise cv.Invalid(
                f"When 'interrupt_when' = {threshold_obj[CONF_INTERRUPT_WHEN]}, then 'min' must be set.",
                [CONF_DISTANCE_THRESHOLD, CONF_MIN],
            )
        if (
            threshold_obj[CONF_INTERRUPT_WHEN]
            in ("above_max", "outside_window", "inside_window")
            and CONF_MAX not in threshold_obj
        ):
            raise cv.Invalid(
                f"When 'interrupt_when' = {threshold_obj[CONF_INTERRUPT_WHEN]}, then 'max' must be set.",
                [CONF_DISTANCE_THRESHOLD, CONF_MAX],
            )
    return obj


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        VL53L1xSensor,
        unit_of_measurement=UNIT_METER,
        icon=ICON_ARROW_EXPAND_VERTICAL,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_DISTANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(CONF_UPDATE_INTERVAL, default="60s"): cv.All(
                cv.positive_time_period_milliseconds,
            ),
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_TIMING_BUDGET, default="50ms"): cv.enum(
                TIMING_BUDGET, lower=True
            ),
            cv.Optional(CONF_DISTANCE_MODE, default="short"): cv.enum(
                DISTANCE_MODE, lower=True
            ),
            cv.Optional(CONF_OFFSET): cv.All(cv.distance, cv.float_range(-4.0, 12.0)),
            cv.Optional(CONF_XTALK_CORRECTION): cv.uint16_t,
            cv.Optional(CONF_REGION_OF_INTEREST): cv.Schema(
                {
                    cv.Required(CONF_X): cv.int_range(min=0, max=12),
                    cv.Required(CONF_Y): cv.int_range(min=0, max=12),
                    cv.Required(CONF_W): cv.int_range(min=4, max=16),
                    cv.Required(CONF_H): cv.int_range(min=4, max=16),
                }
            ),
            cv.Optional(CONF_SIGNAL_THRESHOLD): cv.uint16_t,
            cv.Optional(CONF_SIGMA_THRESHOLD): cv.uint16_t,
            # Interrupt config
            cv.Optional(CONF_INTERRUPT_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_DISTANCE_THRESHOLD): cv.Schema(
                {
                    cv.Optional(CONF_MIN): cv.All(
                        cv.distance, cv.float_range(0.0, 4.0)
                    ),
                    cv.Optional(CONF_MAX): cv.All(
                        cv.distance, cv.float_range(0.0, 4.0)
                    ),
                    cv.Required(CONF_INTERRUPT_WHEN): cv.enum(
                        INTERRUPT_WHEN, lower=True
                    ),
                }
            ),
        }
    )
    .extend(i2c.i2c_device_schema(0x29)),
    check_keys,
)


def to_uint16_mm(meters: float) -> int:
    return min(4000, max(0, int(meters * 1000.0)))


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    if CONF_ENABLE_PIN in config:
        enable = await cg.gpio_pin_expression(config[CONF_ENABLE_PIN])
        cg.add(var.set_enable_pin(enable))

    if CONF_INTERRUPT_PIN in config:
        interrupt = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
        cg.add(var.set_interrupt_pin(interrupt))

    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))

    if CONF_DISTANCE_THRESHOLD in config:
        threshold_obj = config[CONF_DISTANCE_THRESHOLD]
        cg.add(
            var.set_distance_threshold(
                to_uint16_mm(threshold_obj[CONF_MIN])
                if CONF_MIN in threshold_obj
                else 0xFF,
                to_uint16_mm(threshold_obj[CONF_MAX])
                if CONF_MAX in threshold_obj
                else 0xFF,
                threshold_obj[CONF_INTERRUPT_WHEN],
            )
        )

    if CONF_REGION_OF_INTEREST in config:
        roi_obj = config[CONF_REGION_OF_INTEREST]
        cg.add(
            var.set_roi(
                roi_obj[CONF_X],
                roi_obj[CONF_Y],
                roi_obj[CONF_W],
                roi_obj[CONF_H],
            )
        )

    if CONF_SIGMA_THRESHOLD in config:
        cg.add(var.set_sigma_threshold(config[CONF_SIGMA_THRESHOLD]))

    if CONF_SIGMA_THRESHOLD in config:
        cg.add(var.set_signal_threshold(config[CONF_SIGNAL_THRESHOLD]))

    cg.add(var.set_timing_budget(config[CONF_TIMING_BUDGET]))
    cg.add(var.set_distance_mode(config[CONF_DISTANCE_MODE]))

    await i2c.register_i2c_device(var, config)
